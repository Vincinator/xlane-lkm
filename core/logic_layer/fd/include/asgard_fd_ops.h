#pragma once
#include <asgard/asgard.h>

int fd_init(struct proto_instance *);
int fd_start(struct proto_instance *);
int fd_stop(struct proto_instance *);
int fd_clean(struct proto_instance *);
int fd_info(struct proto_instance *);

int fd_post_payload(struct proto_instance *sdev, int remote_lid, int cluster_id,
		    void *payload);
int fd_post_ts(struct proto_instance *sdev, unsigned char *remote_mac,
	       uint64_t ts);
int fd_us_update(struct proto_instance *sdev, void *payload);
int fd_init_payload(void *payload);
