#pragma once



#ifndef ASGARD_KERNEL_MODULE
#include <pthread.h>
#include <errno.h>
#endif


#include "consensus.h"
#include "logger.h"
#include "payload.h"
#include "kvstore.h"

#include "libasraft.h"




int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);
int start_follower(struct proto_instance *ins);
int stop_follower(struct proto_instance *ins);