/*
 * Contains general initializers for libasraft.
 */



#ifdef ASGARD_KERNEL_MODULE
#include <linux/slab.h>
#include "module.h"
#endif

#ifndef ASGARD_KERNEL_MODULE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#endif





#include "consensus.h"
#include "ringbuffer.h"
#include "libasraft.h"

#include "follower.h"
#include "leader.h"
#include "candidate.h"
#include "consensus.h"
#include "membership.h"

#include "logger.h"
#include "proto.h"


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
#ifdef ASGARD_KERNEL_MODULE
EXPORT_SYMBOL(asgard_ip_convert);
#endif


/*
 * Converts an MAC address to hex char array
 */
char * asgard_convert_mac(const char *str)
{
    unsigned int tmp_data[6];
    // must be freed by caller
    char *bytestring_mac =AMALLOC(sizeof(unsigned char) * 6, 1);
    int i;

    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x", &tmp_data[0], &tmp_data[1],
               &tmp_data[2], &tmp_data[3], &tmp_data[4],
               &tmp_data[5]) == 6) {
        for (i = 0; i < 6; i++)
            bytestring_mac[i] = (char)tmp_data[i];
        return bytestring_mac;
    }

    return NULL;
}
#ifdef ASGARD_KERNEL_MODULE
EXPORT_SYMBOL(asgard_convert_mac);
#endif



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


void init_asgard_device(struct asgard_device *sdev){
    int i;

    sdev->hold_fire = 0;
    sdev->multicast.enable = 0;
    sdev->multicast.nextIdx = 0;
    sdev->tx_port = 4000;
    sdev->tx_counter = 0;

#ifdef ASGARD_KERNEL_MODULE
    asg_init_workqueues(sdev);
#endif


    for(i = 0; i < MAX_PROTO_INSTANCES; i++)
        sdev->instance_id_mapping[i] = -1;

    sdev->num_of_proto_instances = 0;

    // Allocate pointer for proto instance placeholders
    sdev->protos = AMALLOC(MAX_PROTO_INSTANCES * sizeof(struct proto_instance *), GFP_KERNEL);

    // Only use consensus protocol for this evaluation.
    //sdev->protos[0] = generate_protocol_instance(sdev, ASGARD_PROTO_CONSENSUS);
    register_protocol_instance(sdev, 1, ASGARD_PROTO_CONSENSUS);
    sdev->ci = ACMALLOC(1, sizeof(struct cluster_info), GFP_KERNEL);
}
