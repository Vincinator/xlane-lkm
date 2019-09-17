#pragma once

#include <sassy/sassy.h>

int consensus_init(struct sassy_device *);
int consensus_start(struct sassy_device *);
int consensus_stop(struct sassy_device *);
int consensus_clean(struct sassy_device *);
int consensus_info(struct sassy_device *);

int consensus_post_payload(struct sassy_device *sdev, unsigned char *remote_mac,
		    void *payload);
int consensus_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
	       uint64_t ts);
int consensus_us_update(struct sassy_device *sdev, void *payload);
int consensus_init_payload(void *payload);
