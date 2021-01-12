/*
 * Contains general initializers for libasraft.
 */


#include <stdio.h>
#include <stdlib.h>

#include <errno.h>

#include "consensus.h"
#include "ringbuffer.h"
#include "libasraft.h"

#include "follower.h"
#include "leader.h"
#include "candidate.h"
#include "consensus.h"
#include "membership.h"



void generate_asgard_eval_uuid(unsigned char uuid[16]) {
    //uuid_generate_random(uuid);
    asgard_dbg("===================== Start of Run ====================\n");
}

uint32_t asgard_ip_convert(const char *str)
{
    unsigned int byte0;
    unsigned int byte1;
    unsigned int byte2;
    unsigned int byte3;

    if (sscanf(str, "%u.%u.%u.%u", &byte0, &byte1, &byte2, &byte3) == 4)
        return (byte0 << 24) + (byte1 << 16) + (byte2 << 8) + byte3;

    return -EINVAL;
}

/*
 * Converts an MAC address to hex char array
 */
unsigned char *asgard_convert_mac(const char *str)
{
    unsigned int tmp_data[6];
    // must be freed by caller
    unsigned char *bytestring_mac = malloc(sizeof(unsigned char) * 6);
    int i;

    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x", &tmp_data[0], &tmp_data[1],
               &tmp_data[2], &tmp_data[3], &tmp_data[4],
               &tmp_data[5]) == 6) {
        for (i = 0; i < 6; i++)
            bytestring_mac[i] = (unsigned char)tmp_data[i];
        return bytestring_mac;
    }

    return NULL;
}



struct proto_instance *generate_protocol_instance(struct asgard_device *sdev, int protocol_id)
{
    struct proto_instance *sproto;
    enum asgard_protocol_type proto_type = (enum asgard_protocol_type)protocol_id;

    sproto = NULL;

    switch (proto_type) {
        case ASGARD_PROTO_FD:
            // sproto = get_fd_proto_instance(sdev);
            break;
        case ASGARD_PROTO_ECHO:
            //sproto = get_echo_proto_instance(sdev);
            break;
        case ASGARD_PROTO_CONSENSUS:
            sproto = get_consensus_proto_instance(sdev);
            break;
        default:
            asgard_error("not a known protocol id\n");
            break;
    }

    return sproto;
}


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


struct asgard_device *init_asgard_device(struct asgard_device *sdev){

    sdev->hold_fire = 0;
    sdev->multicast.delay = 0;
    sdev->multicast.enable = 0;
    sdev->multicast.nextIdx = 0;
    sdev->tx_port = 4000;
    sdev->tx_counter = 0;

    for(int i = 0; i < MAX_PROTO_INSTANCES; i++)
        sdev->instance_id_mapping[i] = -1;

    sdev->num_of_proto_instances = 0;

    // Allocate pointer for proto instance placeholders
    sdev->protos = malloc(MAX_PROTO_INSTANCES * sizeof(struct proto_instance *));

    // Only use consensus protocol for this evaluation.
    //sdev->protos[0] = generate_protocol_instance(sdev, ASGARD_PROTO_CONSENSUS);
    register_protocol_instance(sdev, 1, ASGARD_PROTO_CONSENSUS);
    sdev->ci = calloc(1, sizeof(struct cluster_info));
}
