#pragma once


#include "libasraft.h"

int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
int start_follower(struct proto_instance *ins);
int stop_follower(struct proto_instance *ins);