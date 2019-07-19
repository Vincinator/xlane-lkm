#include <sassy/sassy.h>


struct sassy_fd_priv {
	int test;
};


int fd_init(struct sassy_device*);
int fd_start(struct sassy_device*);
int fd_stop(struct sassy_device*);
int fd_clean(struct sassy_device*);
int fd_info(struct sassy_device*);

int fd_post_payload(struct sassy_device* sdev, unsigned char *remote_mac, void* payload);
int fd_post_ts(struct sassy_device* sdev, unsigned char *remote_mac, uint64_t ts);

int fd_init_payload(void *payload);