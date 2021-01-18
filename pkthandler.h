#pragma once

#include <stdlib.h>
#include "payload.h"
#include "libasraft.h"



struct pkt_work_data{
    struct asgard_device *sdev;
    int remote_lid;
    int rcluster_id;
    struct asgard_payload *payload;
    int received_proto_instances;
    uint32_t bcnt;

};

void post_payload(struct asgard_device *sdev, in_addr_t remote_ip,
        void *payload_in, int payload_len);
