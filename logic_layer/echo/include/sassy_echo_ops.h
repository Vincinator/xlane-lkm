#pragma once

#include <sassy/sassy.h>

struct sassy_echo_priv {
	int test;
};

int echo_init(struct sassy_device *);
int echo_start(struct sassy_device *);
int echo_stop(struct sassy_device *);
int echo_clean(struct sassy_device *);
int echo_info(struct sassy_device *);
int echo_post_payload(struct sassy_device *sdev, unsigned char *remote_mac,
		      void *payload);
int echo_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
		 uint64_t ts);
int echo_init_payload(void *payload);
