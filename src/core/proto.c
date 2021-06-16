//
// Created by Riesop, Vincent on 10.12.20.
//

#include "proto.h"
#include "logger.h"


#ifdef ASGARD_KERNEL_MODULE
#include "../lkm/proto-instance-ctrl.h"

#else
#include <errno.h>

#endif



int register_protocol_instance(struct asgard_device *sdev, int instance_id, int protocol_id, int verbosity)
{
    int idx = sdev->num_of_proto_instances;
    int ret;

    if (idx >= MAX_PROTO_INSTANCES) {
        ret = -EPERM;
        asgard_dbg("Too many instances exist, can not exceed maximum of %d instances\n", MAX_PROTO_INSTANCES);
        asgard_dbg("Current active instances: %d\n", sdev->num_of_proto_instances);
        goto error;
    }

    if(idx < 0) {
        ret = -EPERM;
        asgard_dbg("Invalid Instance ID: %d\n",instance_id);
        goto error;
    }

    if(instance_id > MAX_PROTO_INSTANCES) {
        asgard_dbg("Instance ID is invalid. Must be between 0 and %d\n", MAX_PROTO_INSTANCES - 1);
    }

    if(sdev->instance_id_mapping[instance_id] != -1){
        asgard_dbg("Instance Already registere! (%d)\n", instance_id);
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


#ifdef ASGARD_KERNEL_MODULE
    // Required before we call init of protocol
    init_proto_instance_root(sdev, instance_id);
#endif

    if(sdev->protos[idx]->ctrl_ops.init(sdev->protos[idx], verbosity))
        goto error;

    asgard_dbg("Registered  Protocol instance %d with protocol id %d protocol name: %s\n", instance_id, protocol_id, asgard_get_protocol_name(protocol_id));
    sdev->num_of_proto_instances++;

    return 0;
error:
    asgard_error("Could not register new protocol instance %d\n", ret);
    return ret;
}
