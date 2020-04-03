#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/slab.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION (4, 17, 0)
#define vm_fault_t int
#endif

#ifndef DEVNAME
#define DEVNAME "synbuf"
#endif

#include "include/synbuf-chardev.h"

#include <asm/page.h>
#include <linux/fs.h>
#include <linux/mm.h>

#define SYNBUF_MEMBLOCK_SIZE 512 /* Size in Bytes to be read at once */

/* Determines how many NIC devices are supported in parallel.
 * Used for minor device number generation
 */
#define SYNBUF_MAX_DEVICES 16 
static unsigned int synbuf_bypass_major = 0;
static unsigned int synbuf_bypass_minor = 0;

static struct class *synbuf_bypass_class = NULL;

struct synbuf_device *inode_synbuf(struct inode *inode)
{
	struct cdev *cdev = inode->i_cdev;

	return container_of(cdev, struct synbuf_device, cdev);
}

struct synbuf_device* create_synbuf(const char *name, int num_pages)
{
    int err = 0;
    struct synbuf_device *device
            = kmalloc(sizeof(struct synbuf_device), GFP_KERNEL);

    if(!device) {
        printk(KERN_ERR"could not allocate memory for synbuf device\n");
        goto error;
    }

    // allocate num_pages Page Buffer
    err = synbuf_chardev_init(device, name, num_pages * PAGE_SIZE);

    if(err != 0) {
        printk(KERN_ERR"Failed initializing synbuf device\n");
        goto error;
    }

    printk(KERN_ERR"Initilized synbuf for %s\n", name);

    return device;

    error:
    // if a device was allocated, but an error occurred ...
    if(device)
        kfree(device); // ... clean it up directly!

    return NULL;

}


int synbuf_bypass_open(struct inode *inode, struct file *filp)
{
	struct synbuf_device *sdev;

	sdev = inode_synbuf(inode);

	if(!sdev) {
        printk(KERN_INFO"[SYNBUF] synbuf is not properly intialized in function: %s \n", __FUNCTION__);
        return -ENODEV;
    }

	filp->private_data = sdev; 

	return 0;
}

int synbuf_bypass_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO"[SYNBUF] Enter: %s \n", __FUNCTION__);
	return 0;
}


ssize_t synbuf_bypass_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	struct synbuf_device *sdev = (struct synbuf_device *)filp->private_data;
	ssize_t ret = 0;

    if(!sdev) {
        printk(KERN_INFO"[SYNBUF] synbuf is not properly intialized in function: %s \n", __FUNCTION__);
        return -ENODEV;
    }

	if (mutex_lock_killable(&sdev->ubuf_mutex))
		return -EINTR;

	/* End of File */
	if (*f_pos >= sdev->bufsize)
		goto out;

	/* Would exceed end of file,  */
	if (*f_pos + count > sdev->bufsize)
		count = sdev->bufsize - *f_pos;

	/* Throttle Reading */
	if (count > SYNBUF_MEMBLOCK_SIZE)
		count = SYNBUF_MEMBLOCK_SIZE;
	
	if (copy_to_user(buf, &(sdev->ubuf[*f_pos]), count) != 0) {
		ret = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	ret = count;
	
out:
	mutex_unlock(&sdev->ubuf_mutex);
	return ret;

}

ssize_t synbuf_bypass_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{

	struct synbuf_device *sdev = (struct synbuf_device *)filp->private_data;
	ssize_t ret = 0;

	printk(KERN_INFO"[SYNBUF] Enter: %s \n", __FUNCTION__);

	if (mutex_lock_killable(&sdev->ubuf_mutex))
		return -EINTR;

    /* End of File */
    if (*f_pos >= sdev->bufsize)
        goto out;

    /* Would exceed end of file,  */
    if (*f_pos + count > sdev->bufsize)
        count = sdev->bufsize - *f_pos;
	
	if (count > SYNBUF_MEMBLOCK_SIZE)
		count = SYNBUF_MEMBLOCK_SIZE;
	
	if (copy_from_user(&(sdev->ubuf[*f_pos]), buf, count) != 0)
	{
		ret = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	ret = count;
	
out:
	mutex_unlock(&sdev->ubuf_mutex);
	return ret;
}

void bypass_vma_open(struct vm_area_struct *vma)
{
    struct synbuf_device *sdev = vma->vm_private_data;
    sdev->mapcount++;
}

void bypass_vma_close(struct vm_area_struct *vma)
{
    struct synbuf_device *sdev = vma->vm_private_data;
    sdev->mapcount--;
}

struct page *get_synbuf_buf_page(struct synbuf_device * sdev, pgoff_t pgoff) {
    return vmalloc_to_page(sdev->ubuf + (pgoff << PAGE_SHIFT));
}

vm_fault_t bypass_vm_fault(struct vm_fault *vmf)
{
	struct page *page;
	struct synbuf_device *sdev;

	printk(KERN_INFO"[SYNBUF] %s  \n", __FUNCTION__);

	sdev = (struct synbuf_device *)
	        vmf->vma->vm_private_data;

	if(!sdev) {
        printk(KERN_ERR"[SYNBUF] synbuf device is null in function %s  \n", __FUNCTION__);
        return 0;
    }

    if(!vmf) {
        printk(KERN_ERR"[SYNBUF] vmf is null in function %s  \n", __FUNCTION__);
        return 0;
    }

	if (sdev->ubuf) {
        printk(KERN_INFO"[SYNBUF] vmf->pgoff: %d  \n", vmf->pgoff);

        page = get_synbuf_buf_page(sdev, vmf->pgoff);
        get_page(page);
        vmf->page = page;
	}

	return 0;
}


static struct vm_operations_struct bypass_vm_ops = {
	.open  = bypass_vma_open,
	.fault = bypass_vm_fault,
	.close = bypass_vma_close,
};


static int synbuf_bypass_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct synbuf_device *sdev = filp->private_data;
    int ret;


    if(!sdev) {
        printk(KERN_ERR"synbuf device is NULL\n");
        return -ENODEV;
    }

    ret = remap_vmalloc_range(vma, sdev->ubuf, vma->vm_pgoff);

    if (ret){
        printk(KERN_ERR"remap_vmalloc_range error: %d\n", ret);
        return ret;
    }

    vma->vm_ops = &bypass_vm_ops;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
    vma->vm_private_data = filp->private_data;

    vma->vm_ops->open(vma);

	return 0;
}

static const struct file_operations bypass_fileops = {
	.owner	 = THIS_MODULE,
	.open	 = synbuf_bypass_open,
	.release = synbuf_bypass_release,
	.write	 = synbuf_bypass_write,
	.read	 = synbuf_bypass_read,
	.mmap    = synbuf_bypass_mmap
};

int synbuf_chardev_init(struct synbuf_device *sdev, const char *name, int size)
{
	int ret = 0;
	int page_aligned_size;
	dev_t devno;

    printk(KERN_INFO"[SYNBUF] Enter: %s \n", __FUNCTION__);

	if(!synbuf_bypass_class) {
	    printk(KERN_ERR"[SYNBUF] Synbuf Class is NULL\n Did you initialize synbuf class in your module?\n");
	    return -ENODEV;
	}
	if(!sdev) {
        printk(KERN_ERR"[SYNBUF] Synbuf Device is NULL\n Did you initialize synbuf class in your module?\n");
	    return -ENODEV;
	}

	devno = MKDEV(synbuf_bypass_major, synbuf_bypass_minor++);

	mutex_init(&sdev->ubuf_mutex);

	sdev->mapcount = 0;

	cdev_init(&sdev->cdev, &bypass_fileops);

	sdev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&sdev->cdev, devno, 1);

	if (ret) {
		printk(KERN_ERR"[SYNBUF] Failed to to add %s%d (error %ld)\n", DEVNAME, synbuf_bypass_minor, ret);
		goto cdev_error;
	}

	sdev->device = device_create(synbuf_bypass_class, NULL, devno, NULL, DEVNAME "/%s", name);

	if (!sdev->device) {
		printk(KERN_ERR "[SYNBUF] Failed to create device %d\n", synbuf_bypass_minor);
		ret = -ENODEV;
		goto device_create_error;
	}

    page_aligned_size = PAGE_ALIGN(size);

    sdev->ubuf = vmalloc_user(page_aligned_size);

	if (!sdev->ubuf) {
		printk(KERN_ERR "[SYNBUF] Failed to allocate ubuf memory\n");
		ret = -ENOMEM;
		goto ubuf_error;
	}

    printk(KERN_INFO"[SYNBUF] Success: %s \n", __FUNCTION__);

	return 0;

ubuf_error:
device_create_error:
	cdev_del(&sdev->cdev);
cdev_error:
	mutex_destroy(&sdev->ubuf_mutex);
	return ret;
}


void synbuf_chardev_exit(struct synbuf_device *sdev)
{
    printk(KERN_INFO"[SYNBUF] -: %s \n", __FUNCTION__);

    if(!sdev)
        return;

    mutex_destroy(&sdev->ubuf_mutex);

    device_del (sdev->device);

    cdev_del(&sdev->cdev);

    if(sdev->ubuf){
        printk(KERN_INFO"[SYNBUF] cleaning ubuf now..\n", __FUNCTION__);
        vfree(sdev->ubuf); // Make sure to match allocation method - and use the right friend!
    }
    printk(KERN_INFO"[SYNBUF] Success: %s \n", __FUNCTION__);

}

void synbuf_clean_class(void)
{
    printk(KERN_INFO"[SYNBUF] Enter: %s \n", __FUNCTION__);

    if (!synbuf_bypass_class) {
        printk(KERN_INFO"[SYNBUF] Synbuf was not properly initialized! %s \n", __FUNCTION__);
        return;
    }

    class_destroy(synbuf_bypass_class);

    if(synbuf_bypass_major == 0)
        return; // DEBUG..

    unregister_chrdev_region(MKDEV(synbuf_bypass_major, 0), SYNBUF_MAX_DEVICES);
    printk(KERN_INFO"[SYNBUF] Success: %s \n", __FUNCTION__);
}

int synbuf_bypass_init_class(void) {

	long err = 0;
	dev_t dev = 0;
	
	printk(KERN_INFO"[SYNBUF] Enter: %s \n", __FUNCTION__);

	err = alloc_chrdev_region(&dev, 0, SYNBUF_MAX_DEVICES, DEVNAME);
	if (err < 0) {
		printk(KERN_ERR "[SYNBUF] alloc_chrdev_region() failed in %s\n", __FUNCTION__);
		return err;
	}

	synbuf_bypass_major = MAJOR(dev);

    printk(KERN_INFO "[SYNBUF]synbuf_bypass_major %d\n", synbuf_bypass_major);

    synbuf_bypass_class = class_create(THIS_MODULE, DEVNAME);

	if (!synbuf_bypass_class) {
		err = -ENODEV;
		goto error;
	}

    printk(KERN_INFO"[SYNBUF] Success: %s \n", __FUNCTION__);
	return 0;
error:
	printk(KERN_ERR"[SYNBUF] Error in %s, cleaning up now \n", __FUNCTION__);
	synbuf_clean_class();
	return err;
}
