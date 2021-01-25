#pragma once

#include "libasraft.h"

int start_candidate(struct proto_instance *ins);
int stop_candidate(struct proto_instance *ins);
int candidate_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);