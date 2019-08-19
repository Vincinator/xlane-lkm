#pragma once
#include <sassy/sassy.h>

int fd_init(struct sassy_device *);
int fd_start(struct sassy_device *);
int fd_stop(struct sassy_device *);
int fd_clean(struct sassy_device *);
int fd_info(struct sassy_device *);

int fd_post_payload(struct sassy_device *sdev, unsigned char *remote_mac,
		    void *payload);
int fd_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
	       uint64_t ts);
int fd_us_update(struct sassy_device *sdev, void *payload);
int fd_init_payload(struct sassy_payload *payload);
