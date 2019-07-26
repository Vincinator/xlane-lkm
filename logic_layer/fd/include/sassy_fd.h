#pragma once

#include <sassy/sassy.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <asm/page.h>
#include <linux/fs.h>
#include <linux/mm.h>

struct sassy_fd_priv {
	
	/* Character Device to mmap FD aliveness counter memory to user space */
	struct device* tx_device;
	struct cdev cdev_tx;
	struct mutex tx_mutex;		
	char *tx_buf; 			// Ptr to Kernel Mem


	/* Character Device to mmap FD RX Results to user space */
	struct device* rx_device;
	struct cdev cdev_rx;
	struct mutex rx_mutex;
	char *rx_buf; 			// Ptr to Kernel Mem

};


/* MUST not exceed SASSY_PAYLOAD_BYTES in size! */
struct fd_payload {
	u8 protocol_id; 		/* must be the first element */
	u8 message;				/* short message bundled with this hb */
	u8 alive_rp;			/* Number of alive processes */
	struct sassy_process_info pinfo[MAX_PROCESSES_PER_HOST];
};