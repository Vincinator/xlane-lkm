//
// Created by Riesop, Vincent on 10.12.20.
//

#include "proto.h"
#include "logger.h"

#ifndef ASGARD_KERNEL_MODULE
#include <errno.h>
#endif



int register_protocol_instance(struct asgard_device *sdev, int instance_id, int protocol_id)
{

    int idx = sdev->num_of_proto_instances;
    int ret;


    if (idx > MAX_PROTO_INSTANCES) {
        ret = -EPERM;
        asgard_dbg("Too many instances exist, can not exceed maximum of %d instances\n", MAX_PROTOCOLS);
        asgard_dbg("Current active instances: %d\n", sdev->num_of_proto_instances);

        goto error;
    }
    if(idx < 0) {
        ret = -EPERM;
        asgard_dbg("Invalid Instance ID: %d\n",instance_id);
        goto error;
    }

    if(sdev->instance_id_mapping[instance_id] != -1){
        asgard_dbg("Instance Already registered! (%d)\n", instance_id);
        ret = -EINVAL;
        goto error;
    }

    sdev->protos[idx] = generate_protocol_instance(sdev, protocol_id);

    if (!sdev->protos[idx]) {
        asgard_dbg("Could not allocate memory for new protocol instance!\n");
        ret = -ENOMEM;
        goto error;
    }

    sdev->instance_id_mapping[instance_id] = idx;

    sdev->protos[idx]->instance_id = instance_id;

    sdev->num_of_proto_instances++;

    sdev->protos[idx]->ctrl_ops.init(sdev->protos[idx]);

    return 0;
    error:
    asgard_error("Could not register new protocol instance %d\n", ret);
    return ret;
}