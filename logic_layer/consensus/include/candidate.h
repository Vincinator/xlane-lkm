#pragma once

#include <sassy/sassy.h>

#include "sassy_consensus.h"

int candidate_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
void reset_ctimeout(struct proto_instance *ins);
int stop_candidate(struct proto_instance *ins);
int start_candidate(struct proto_instance *ins);
void init_ctimeout(struct proto_instance *ins);
int setup_nomination(struct proto_instance *ins);


