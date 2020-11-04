#pragma once

#include <asgard/asgard.h>

#include <asgard/consensus.h>

int candidate_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
int stop_candidate(struct proto_instance *priv);
int start_candidate(struct proto_instance *priv);
int setup_nomination(struct proto_instance *ins);


