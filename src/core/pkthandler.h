#pragma once


#ifndef ASGARD_KERNEL_MODULE

#include <stdlib.h>


#endif


#include "payload.h"
#include "libasraft.h"



struct pkt_work_data{

#ifdef ASGARD_KERNEL_MODULE
    struct work_struct work;
#endif

    struct asgard_device *sdev;
    int remote_lid;
    int rcluster_id;
    struct asgard_payload *payload;
    int received_proto_instances;
    uint32_t bcnt;

};


#ifdef ASGARD_KERNEL_MODULE
void asgard_post_payload(int asgard_id, void *payload_in, uint16_t headroom, uint32_t cqe_bcnt);
#else
void asgard_post_payload(struct asgard_device *sdev, uint32_t remote_ip, void *payload_in, uint32_t payload_len);

#endif


void handle_sub_payloads(struct asgard_device *sdev, int remote_lid,
        int cluster_id, char *payload, int instances, uint32_t bcnt);
