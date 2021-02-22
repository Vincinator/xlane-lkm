//
// Created by Riesop, Vincent on 10.12.20.
//

#include "proto.h"
#include "logger.h"

#ifndef ASGARD_KERNEL_MODULE
#include <errno.h>
#endif



int register_protocol_instance(struct asgard_device *sdev, int instance_id, int protocol_id, int verbosity)
{

    int ret;


    if (instance_id > MAX_PROTO_INSTANCES) {
        ret = -EPERM;
        asgard_dbg("Too many instances exist, can not exceed maximum of %d instances\n", MAX_PROTOCOLS);
        asgard_dbg("Current active instances: %d\n", sdev->num_of_proto_instances);

        goto error;
    }
    if(instance_id < 0) {
        ret = -EPERM;
        asgard_dbg("Invalid Instance ID: %d\n",instance_id);
        goto error;
    }

    if(sdev->instance_id_mapping[instance_id] != -1){
        asgard_dbg("Instance Already registered! (%d)\n", instance_id);
        ret = -EINVAL;
        goto error;
    }

    sdev->protos[instance_id] = generate_protocol_instance(sdev, protocol_id);

    if (!sdev->protos[instance_id]) {
        asgard_dbg("Could not allocate memory for new protocol instance!\n");
        ret = -ENOMEM;
        goto error;
    }

    sdev->instance_id_mapping[instance_id] = instance_id;

    sdev->protos[instance_id]->instance_id = instance_id;

    sdev->num_of_proto_instances++;

    sdev->protos[instance_id]->ctrl_ops.init(sdev->protos[instance_id], verbosity);

    asgard_dbg("Registered  Protocol instance %d with protocol %s\n", instance_id, asgard_get_protocol_name(protocol_id));

    return 0;
error:
    asgard_error("Could not register new protocol instance %d\n", ret);
    return ret;
}
