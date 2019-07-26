#include <sassy/sassy.h>


struct sassy_fd_priv {
	
	/* Character Device to mmap FD aliveness counter memory to user space */
	struct device* tx_device;
	struct cdev *cdev_tx;
	struct mutex tx_mutex;		
	char *tx_buf; 			// Ptr to Kernel Mem


	/* Character Device to mmap FD RX Results to user space */
	struct device* rx_device;
	struct cdev *cdev_rx;
	struct mutex rx_mutex;
	char *rx_buf; 			// Ptr to Kernel Mem


};


int fd_init(struct sassy_device*);
int fd_start(struct sassy_device*);
int fd_stop(struct sassy_device*);
int fd_clean(struct sassy_device*);
int fd_info(struct sassy_device*);

int fd_post_payload(struct sassy_device* sdev, unsigned char *remote_mac, void* payload);
int fd_post_ts(struct sassy_device* sdev, unsigned char *remote_mac, uint64_t ts);

int fd_init_payload(void *payload);