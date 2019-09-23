#pragma once

#include <sassy/sassy.h>

#include <sassy/consensus.h>

int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
void reset_ftimeout(struct proto_instance *ins);
int stop_follower(struct proto_instance *ins);
int start_follower(struct proto_instance *ins);
void init_timeout(struct proto_instance *ins);
void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, int param1, int param2);