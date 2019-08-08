#pragma once
#include <sassy/sassy.h>

int fd_init(const struct sassy_device *);
int fd_start(const struct sassy_device *);
int fd_stop(const struct sassy_device *);
int fd_clean(const struct sassy_device *);
int fd_info(const struct sassy_device *);

int fd_post_payload(const struct sassy_device *sdev, unsigned char *remote_mac,
		    void *payload);
int fd_post_ts(const struct sassy_device *sdev, unsigned char *remote_mac,
	       uint64_t ts);
int fd_us_update(const struct sassy_device *sdev, void *payload);
int fd_init_payload(void *payload);