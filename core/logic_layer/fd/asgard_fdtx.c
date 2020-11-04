#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mm_types.h>

#include <asgard/asgard.h>
#include <asgard/logger.h>


#include <asm/page.h>
#include <linux/fs.h>
#include <linux/mm.h>

#include "include/asgard_fdtx.h"
#include "include/asgard_fd.h"

#define DEVNAME "asgard_fd_tx_mem"

#define ASGARD_MEMBLOCK_SIZE 512 /* Size in Bytes to be read at once */

/* Determines how many NIC devices are supported in parallel.
 * Used for minor device number generation
 */
#define ASGARD_MAX_DEVICES 16
static unsigned int asgard_bypass_major;
static struct class *asgard_bypass_class;


/* Allocates pages and sets the PG_reserved bit for each allocated page*/
static char *asgard_alloc_mmap_buffer(void)
{
	char *page = (char *)get_zeroed_page(GFP_KERNEL);

	asgard_dbg("Enter: %s\n", __func__);

	if (!page)
		goto error;

	SetPageReserved(virt_to_page((void *)page));

	return page;
error:
	asgard_error("Failed in %s\n", __func__);
	return page;
}

static void asgard_free_mmap_buffer(void *page)
{
	asgard_dbg("Enter: %s\n", __func__);

	ClearPageReserved(virt_to_page(page));
	free_page((unsigned long)page);
}

static int asgard_bypass_open(struct inode *inode, struct file *filp)
{
	struct asgard_fd_priv *sdev =
		container_of(inode->i_cdev, struct asgard_fd_priv, cdev_tx);

	asgard_dbg("Enter: %s\n", __func__);

	filp->private_data = sdev;

	return 0;
}

static int asgard_bypass_release(struct inode *inode, struct file *filp)
{
	asgard_dbg("Enter: %s\n", __func__);
	return 0;
}

static ssize_t asgard_bypass_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *f_pos)
{
	struct asgard_fd_priv *priv = (struct asgard_fd_priv *)filp->private_data;
	ssize_t ret = 0;

	asgard_dbg("Enter: %s\n", __func__);

	BUG_ON(priv == NULL);

	if (mutex_lock_killable(&priv->tx_mutex))
		return -EINTR;

	if (*f_pos >= PAGE_SIZE) /* EOF */
		goto out;

	if (*f_pos + count > PAGE_SIZE)
		count = PAGE_SIZE - *f_pos;

	if (count > ASGARD_MEMBLOCK_SIZE)
		count = ASGARD_MEMBLOCK_SIZE;

	if (copy_to_user((void __user *)buf, &(priv->tx_buf[*f_pos]), count) != 0) {
		ret = -EFAULT;
		goto out;
	}
#if VERBOSE_DEBUG
	print_hex_dump(KERN_DEBUG, "Aliveness Counter: ", DUMP_PREFIX_NONE, 16,
		       1, priv->tx_buf, ASGARD_PAYLOAD_BYTES, 0);
#endif
	*f_pos += count;
	ret = count;

out:
	mutex_unlock(&priv->tx_mutex);
	return ret;
}

static ssize_t asgard_bypass_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *f_pos)
{
	struct asgard_fd_priv *priv = (struct asgard_fd_priv *)filp->private_data;
	ssize_t ret = 0;

	asgard_dbg("Enter: %s\n", __func__);

	if (mutex_lock_killable(&priv->tx_mutex))
		return -EINTR;

	if (*f_pos >= PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	if (*f_pos + count > PAGE_SIZE)
		count = PAGE_SIZE - *f_pos;

	if (count > ASGARD_MEMBLOCK_SIZE)
		count = ASGARD_MEMBLOCK_SIZE;

	if (copy_from_user(&(priv->tx_buf[*f_pos]), (void  __user *) buf, count) != 0) {
		ret = -EFAULT;
		goto out;
	}

	*f_pos += count;
	ret = count;

out:
	mutex_unlock(&priv->tx_mutex);
	return ret;
}

static void bypass_vma_open(struct vm_area_struct *vma)
{
	asgard_dbg("Bypass VMA open, virt %lx, phys %lx\n",
		  vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

static void bypass_vma_close(struct vm_area_struct *vma)
{
	asgard_dbg("Simple VMA close.\n");
}

static vm_fault_t bypass_vm_fault(struct vm_fault *vmf)
{
	struct page *page;
	struct asgard_fd_priv *priv;

	asgard_dbg("Enter: %s\n", __func__);

	priv = (struct asgard_fd_priv *)vmf->vma->vm_private_data;
	if (priv->tx_buf) {
		page = virt_to_page(priv->tx_buf);
		get_page(page);
		vmf->page = page;
	}
	return 0;
}

static struct vm_operations_struct bypass_vm_ops = {
	.open = bypass_vma_open,
	.fault = bypass_vm_fault,
	.close = bypass_vma_close,
};

static int asgard_bypass_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct asgard_fd_priv *priv = filp->private_data;
	int ret;

	asgard_dbg("Enter: %s\n", __func__);

	if (!priv) {
		asgard_error(
			"private data of file struct is faulty\n");
		return -ENODEV;
	}

	// map ubuf of sdev to vma
	ret = remap_pfn_range(vma, vma->vm_start,
			      page_to_pfn(virt_to_page(priv->tx_buf)),
			      vma->vm_end - vma->vm_start, vma->vm_page_prot);

	if (ret < 0) {
		asgard_error("Mapping of kernel memory failed\n");
		return -EIO;
	}
	vma->vm_ops = &bypass_vm_ops;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = filp->private_data;

	bypass_vma_open(vma);
	return 0;
}

static const struct file_operations bypass_fileops = {
	.owner = THIS_MODULE,
	.open = asgard_bypass_open,
	.release = asgard_bypass_release,
	.write = asgard_bypass_write,
	.read = asgard_bypass_read,
	.mmap = asgard_bypass_mmap
};

int asgard_setup_chardev(struct asgard_device *sdev, struct asgard_fd_priv *priv)
{
	int ret = 0;
	dev_t devno;

	BUG_ON(sdev == NULL || asgard_bypass_class == NULL);
	asgard_dbg("Enter: %s\n", __func__);

	devno = MKDEV(asgard_bypass_major, sdev->ifindex);

	mutex_init(&priv->tx_mutex);

	cdev_init(&priv->cdev_tx, &bypass_fileops);
	priv->cdev_tx.owner = THIS_MODULE;

	ret = cdev_add(&priv->cdev_tx, devno, 1);
	if (ret) {
		asgard_error("Failed to to add %s%d (error %d)\n",
			    DEVNAME, sdev->ifindex, ret);
		goto cdev_error;
	}

	priv->tx_device = device_create(asgard_bypass_class, NULL, devno, NULL,
					DEVNAME "%d", sdev->ifindex);

	if (IS_ERR(priv->tx_device)) {
		asgard_error("Failed to create device %d\n",
			    sdev->ifindex);
		ret = PTR_ERR(priv->tx_device);
		goto device_create_error;
	}

	priv->tx_buf = asgard_alloc_mmap_buffer();

	if (IS_ERR(priv->tx_buf)) {
		asgard_error("Failed to allocate ubuf memory\n");
		ret = PTR_ERR(priv->tx_buf);
		goto ubuf_error;
	}

	return 0;

ubuf_error:
device_create_error:
	cdev_del(&priv->cdev_tx);
cdev_error:
	mutex_destroy(&priv->tx_mutex);
	return ret;
}

void asgard_clean_class(struct asgard_device *sdev)
{
	device_destroy(asgard_bypass_class, MKDEV(asgard_bypass_major, sdev->ifindex));

	class_destroy(asgard_bypass_class);

	unregister_chrdev_region(MKDEV(asgard_bypass_major, 0),
				 ASGARD_MAX_DEVICES);
}

int asgard_bypass_init_class(struct asgard_device *sdev)
{
	int err = 0;
	dev_t dev = 0;

	asgard_dbg("Enter: %s\n", __func__);

	err = alloc_chrdev_region(&dev, 0, ASGARD_MAX_DEVICES, DEVNAME);
	if (err < 0) {
		asgard_error("alloc_chrdev_region() failed in %s\n",
			    __func__);
		return err;
	}

	asgard_bypass_major = MAJOR(dev);
	asgard_bypass_class = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(asgard_bypass_class)) {
		err = PTR_ERR(asgard_bypass_class);
		goto error;
	}

	return 0;
error:
	asgard_error("Error in %s, cleaning up now\n", __func__);
	asgard_clean_class(sdev);
	return err;
}
