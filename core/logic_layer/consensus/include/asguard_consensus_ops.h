#pragma once

#include <asguard/asguard.h>

int consensus_init(struct proto_instance *ins);
int consensus_start(struct proto_instance *ins);
int consensus_stop(struct proto_instance *ins);
int consensus_clean(struct proto_instance *ins);
int consensus_info(struct proto_instance *ins);

int consensus_post_payload(struct proto_instance *ins, int remote_lid, int cluster_id,
		    void *payload);
int consensus_post_ts(struct proto_instance *ins, unsigned char *remote_mac,
	       uint64_t ts);
int consensus_us_update(struct proto_instance *ins, void *payload);
int consensus_init_payload(void *payload);
