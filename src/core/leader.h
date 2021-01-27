#pragma once

#ifndef ASGARD_KERNEL_MODULE
#include <stdlib.h>
#endif

#include "libasraft.h"
#include "consensus.h"

struct asgard_leader_pkt_work_data {

#ifdef ASGARD_KERNEL_MODULE
    struct work_struct work;
#endif

    struct asgard_device *sdev;

    int target_id;
    int32_t next_index;
    int retrans;

};

int start_leader(struct proto_instance *ins);
int stop_leader(struct proto_instance *ins);
int leader_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt);

