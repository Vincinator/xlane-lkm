#pragma once

#include <asgard/asgard.h>
#include <asgard/consensus.h>

int leader_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
int start_leader(struct proto_instance *ins);
int stop_leader(struct proto_instance *ins);
