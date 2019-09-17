#pragma once

#include <sassy/sassy.h>

#include "sassy_consensus.h"

int leader_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
int start_leader(struct consensus_priv *priv);
int stop_leader(struct consensus_priv *priv);
