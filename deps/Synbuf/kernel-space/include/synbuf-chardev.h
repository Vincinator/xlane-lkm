#pragma once

#include <linux/cdev.h>

struct synbuf_device {
	int unsigned minorIndex; /* will be set by synbuf */

	struct cdev cdev;     	/* Char device structure (MUST NOT be a pointer, otherwise container_of does not work*/
	char *ubuf; 			/* Ptr to Kernel Mem, which is shared with user space via mmap */

	struct mutex ubuf_mutex;/* Mutex for accessing ubuf memory */
	struct device* device;
    size_t bufsize;
};

long synbuf_chardev_init(struct synbuf_device *sdev, const char *name, int size);
void synbuf_chardev_exit(struct synbuf_device *sdev);

long synbuf_bypass_init_class(void);
void synbuf_clean_class(void);
