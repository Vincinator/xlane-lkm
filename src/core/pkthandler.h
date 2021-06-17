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
    char *payload;
    struct asgard_payload *user_data;
    int received_proto_instances;
    uint32_t bcnt;
    uint64_t ots;

};


#ifdef ASGARD_KERNEL_MODULE
void asgard_post_payload(int asgard_id, void *payload_in, uint16_t headroom, uint32_t cqe_bcnt, uint64_t ots);

#else
void asgard_post_payload(struct asgard_device *sdev, uint32_t remote_ip, void *payload_in, uint32_t payload_len, uint64_t ots);
void DumpHex(const void* data, size_t size);
#endif


void handle_sub_payloads(struct asgard_device *sdev, int remote_lid,
        int cluster_id, char *payload, int instances, uint32_t bcnt, uint64_t ots);
