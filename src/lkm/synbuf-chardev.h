#pragma once

#include <linux/cdev.h>



struct synbuf_device {
    struct cdev cdev;     	/* Char device structure (MUST NOT be a pointer, otherwise container_of does not work*/
    char *ubuf; 			/* Ptr to Kernel Mem, which is shared with user space via mmap */

    struct mutex ubuf_mutex;/* Mutex for accessing ubuf memory */
    struct device* device;
    size_t bufsize;
    int mapcount;
};

/*
 * Allocates a page alligned buffer with <num_pages> pages and initializes it
 */
struct synbuf_device* create_synbuf(const char *name, size_t size);

int synbuf_chardev_init(struct synbuf_device *sdev, const char *name, int size);
void synbuf_chardev_exit(struct synbuf_device *sdev);

int synbuf_bypass_init_class(void);
void synbuf_clean_class(void);
