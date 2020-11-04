#pragma once

#include <asgard/asgard.h>

#include <asgard/consensus.h>

int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
int stop_follower(struct proto_instance *ins);
int start_follower(struct proto_instance *ins);
void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, int param1, int param2);
