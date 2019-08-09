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

#include <sassy/sassy.h>
#include <sassy/logger.h>


#include <asm/page.h>
#include <linux/fs.h>
#include <linux/mm.h>

#include "include/sassy_fdtx.h"
#include "include/sassy_fd.h"

#define DEVNAME "sassy_fd_tx_mem"

#define SASSY_MEMBLOCK_SIZE 512 /* Size in Bytes to be read at once */

/* Determines how many NIC devices are supported in parallel.
 * Used for minor device number generation
 */
#define SASSY_MAX_DEVICES 16
static unsigned int sassy_bypass_major = 0;
static struct class *sassy_bypass_class = NULL;

struct sassy_fd_priv *inode_sassy_fd_priv(struct inode *inode)
{
	struct cdev *cdev = inode->i_cdev;

	return container_of(cdev, struct sassy_fd_priv, cdev_tx);
}

/* Allocates pages and sets the PG_reserved bit for each allocated page*/
char *sassy_alloc_mmap_buffer(void)
{
	char *page = (char *)get_zeroed_page(GFP_KERNEL);

	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	if (!page)
		goto error;

	SetPageReserved(virt_to_page((void *)page));

	return page;
error:
	sassy_error("[SASSY] Failed in %s\n", __FUNCTION__);
	return page;
}

void sassy_free_mmap_buffer(void *page)
{
	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	ClearPageReserved(virt_to_page(page));
	free_page((unsigned long)page);
}

int sassy_bypass_open(struct inode *inode, struct file *filp)
{
	struct sassy_fd_priv *sdev = inode_sassy_fd_priv(inode);

	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	filp->private_data = sdev;

	return 0;
}

int sassy_bypass_release(struct inode *inode, struct file *filp)
{
	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);
	return 0;
}

ssize_t sassy_bypass_read(struct file *filp, char *buf, size_t count,
			  loff_t *f_pos)
{
	struct sassy_fd_priv *priv = (struct sassy_fd_priv *)filp->private_data;
	ssize_t ret = 0;

	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	BUG_ON(priv == NULL);

	if (mutex_lock_killable(&priv->tx_mutex))
		return -EINTR;

	if (*f_pos >= PAGE_SIZE) /* EOF */
		goto out;

	if (*f_pos + count > PAGE_SIZE)
		count = PAGE_SIZE - *f_pos;

	if (count > SASSY_MEMBLOCK_SIZE)
		count = SASSY_MEMBLOCK_SIZE;

	if (copy_to_user(buf, &(priv->tx_buf[*f_pos]), count) != 0) {
		ret = -EFAULT;
		goto out;
	}

	print_hex_dump(KERN_DEBUG, "Aliveness Counter: ", DUMP_PREFIX_NONE, 16,
		       1, priv->tx_buf, SASSY_PAYLOAD_BYTES, 0);

	*f_pos += count;
	ret = count;

out:
	mutex_unlock(&priv->tx_mutex);
	return ret;
}

static ssize_t sassy_bypass_write(struct file *filp, const char *buf,
				size_t count, loff_t *f_pos)
{
	struct sassy_fd_priv *priv = (struct sassy_fd_priv *)filp->private_data;
	ssize_t ret = 0;

	sassy_dbg("Enter: %s\n", __FUNCTION__);

	if (mutex_lock_killable(&priv->tx_mutex))
		return -EINTR;

	if (*f_pos >= PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	if (*f_pos + count > PAGE_SIZE)
		count = PAGE_SIZE - *f_pos;

	if (count > SASSY_MEMBLOCK_SIZE)
		count = SASSY_MEMBLOCK_SIZE;

	if (copy_from_user(&(priv->tx_buf[*f_pos]), buf, count) != 0) {
		ret = -EFAULT;
		goto out;
	}

	*f_pos += count;
	ret = count;

out:
	mutex_unlock(&priv->tx_mutex);
	return ret;
}

void bypass_vma_open(struct vm_area_struct *vma)
{
	sassy_dbg("[SASSY] Bypass VMA open, virt %lx, phys %lx\n",
		  vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void bypass_vma_close(struct vm_area_struct *vma)
{
	sassy_dbg("[SASSY] Simple VMA close.\n");
}

vm_fault_t bypass_vm_fault(struct vm_fault *vmf)
{
	struct page *page;
	struct sassy_fd_priv *priv;

	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	priv = (struct sassy_fd_priv *)vmf->vma->vm_private_data;
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

static int sassy_bypass_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct sassy_fd_priv *priv = filp->private_data;
	int ret;

	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	if (!priv) {
		sassy_error(
			"[SASSY] Could not find sdev in private data of file struct\n");
		return -ENODEV;
	}

	// map ubuf of sdev to vma
	ret = remap_pfn_range(vma, vma->vm_start,
			      page_to_pfn(virt_to_page(priv->tx_buf)),
			      vma->vm_end - vma->vm_start, vma->vm_page_prot);

	if (ret < 0) {
		sassy_error("[SASSY] Mapping of kernel memory failed\n");
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
	.open = sassy_bypass_open,
	.release = sassy_bypass_release,
	.write = sassy_bypass_write,
	.read = sassy_bypass_read,
	.mmap = sassy_bypass_mmap
};

int sassy_setup_chardev(struct sassy_device *sdev)
{
	int ret = 0;
	dev_t devno;

	struct sassy_protocol *proto = sdev->proto;
	struct sassy_fd_priv *priv = (struct sassy_fd_priv *)proto->priv;

	BUG_ON(sdev == NULL || sassy_bypass_class == NULL);
	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	devno = MKDEV(sassy_bypass_major, sdev->ifindex);

	mutex_init(&priv->tx_mutex);

	cdev_init(&priv->cdev_tx, &bypass_fileops);
	priv->cdev_tx.owner = THIS_MODULE;

	ret = cdev_add(&priv->cdev_tx, devno, 1);
	if (ret) {
		sassy_error("[SASSY] Failed to to add %s%d (error %d)\n",
			    DEVNAME, sdev->ifindex, ret);
		goto cdev_error;
	}

	priv->tx_device = device_create(sassy_bypass_class, NULL, devno, NULL,
					DEVNAME "%d", sdev->ifindex);

	if (IS_ERR(priv->tx_device)) {
		sassy_error("[SASSY] Failed to create device %d\n",
			    sdev->ifindex);
		ret = PTR_ERR(priv->tx_device);
		goto device_create_error;
	}

	priv->tx_buf = sassy_alloc_mmap_buffer();

	if (IS_ERR(priv->tx_buf)) {
		sassy_error("[SASSY] Failed to allocate ubuf memory\n");
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

void sassy_clean_class(void)
{
	if (sassy_bypass_class)
		class_destroy(sassy_bypass_class);

	unregister_chrdev_region(MKDEV(sassy_bypass_major, 0),
				 SASSY_MAX_DEVICES);
}

int sassy_bypass_init_class(void)
{
	int err = 0;
	dev_t dev = 0;

	sassy_dbg("[SASSY] Enter: %s\n", __FUNCTION__);

	err = alloc_chrdev_region(&dev, 0, SASSY_MAX_DEVICES, DEVNAME);
	if (err < 0) {
		sassy_error("[SASSY] alloc_chrdev_region() failed in %s\n",
			    __FUNCTION__);
		return err;
	}

	sassy_bypass_major = MAJOR(dev);
	sassy_bypass_class = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(sassy_bypass_class)) {
		err = PTR_ERR(sassy_bypass_class);
		goto error;
	}

	return 0;
error:
	sassy_error("[SASSY] Error in %s, cleaning up now\n", __FUNCTION__);
	sassy_clean_class();
	return err;
}
