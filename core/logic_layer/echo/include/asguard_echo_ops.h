#pragma once

#include <asguard/asguard.h>

struct asguard_echo_priv {
	struct asguard_logger echo_logger;
	struct asguard_device *sdev;
	struct proto_instance *ins;
};

int echo_init(struct proto_instance *);
int echo_start(struct proto_instance *);
int echo_stop(struct proto_instance *);
int echo_clean(struct proto_instance *);
int echo_info(struct proto_instance *);
int echo_post_payload(struct proto_instance *, int remote_lid, int cluster_id,
		      void *payload);
int echo_post_ts(struct proto_instance *, unsigned char *remote_mac,
		 uint64_t ts);
int echo_init_payload(void *payload);
