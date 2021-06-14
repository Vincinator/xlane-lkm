/*
 * Contains general initializers for libasraft.
 */



#ifdef ASGARD_KERNEL_MODULE
#include <linux/slab.h>
#include "module.h"

#include "../lkm/core-ctrl.h"
#include "../lkm/pm-ctrl.h"
#include "../lkm/multicast-ctrl.h"
#include "../lkm/proto-instance-ctrl.h"
#include "../lkm/kernel_ts.h"
#include "../lkm/ts-ctrl.h"

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
#include "pingpong.h"

const char *asgard_get_protocol_name(enum asgard_protocol_type protocol_type)
{
    switch (protocol_type) {
        case ASGARD_PROTO_FD:
            return "Failure Detector";
        case ASGARD_PROTO_ECHO:
            return "Echo";
        case ASGARD_PROTO_CONSENSUS:
            return "Consensus";
        case ASGARD_PROTO_PP:
            return "Ping Ping";
        default:
            return "Unknown Protocol!";
    }
}
#ifdef ASGARD_KERNEL_MODULE
EXPORT_SYMBOL(asgard_get_protocol_name);
#endif


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
int asgard_convert_mac(const char *str, unsigned char *bytestring_mac)
{
    unsigned int tmp_data[6];
    // must be freed by caller
    // char *bytestring_mac =AMALLOC(sizeof(unsigned char) * 6, 1);
    int i;

    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x", &tmp_data[0], &tmp_data[1],
               &tmp_data[2], &tmp_data[3], &tmp_data[4],
               &tmp_data[5]) == 6) {
        for (i = 0; i < 6; i++)
            bytestring_mac[i] = (char)tmp_data[i];
        return 0;
    }

    return -1;
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
        case ASGARD_PROTO_PP:
            sproto = get_pp_proto_instance(sdev);
            break;
        default:
            asgard_error("not a known protocol id\n");
            break;
    }

    return sproto;
}


int init_asgard_device(struct asgard_device *sdev, int asgard_id, int ifindex){
    int i;

    sdev->hold_fire = 0;
   
    sdev->tx_port = 4000;
    sdev->tx_counter = 0;

#ifdef ASGARD_MODULE_GIT_VERSION
    asgard_dbg("Running asgard Version: %s\n", ASGARD_MODULE_GIT_VERSION);
#endif

#ifdef ASGARD_KERNEL_MODULE


	sdev->ifindex = ifindex;
	sdev->hb_interval = 0;

	sdev->bug_counter = 0;
	sdev->asgard_id = asgard_id;
	sdev->ndev = asgard_get_netdevice(ifindex);
	sdev->pminfo.num_of_targets = 0;
	sdev->pminfo.waiting_window = 100000;
	sdev->verbose = 0;
	sdev->rx_state = ASGARD_RX_DISABLED;
	sdev->ts_state = ASGARD_TS_UNINIT;
	sdev->last_leader_ts = 0;
	sdev->num_of_proto_instances = 0;
	sdev->hold_fire = 0;
	sdev->tx_port = 3319;
	sdev->cur_leader_lid = -1;
	sdev->is_leader = 0;
	sdev->consensus_priv = NULL;
	sdev->echo_priv = NULL;

    sdev->multicast.naddr.dst_mac = AMALLOC(sizeof(char) * 6, 1);
	sdev->multicast.naddr.dst_ip = asgard_ip_convert("232.43.211.234");
	asgard_convert_mac("01:00:5e:2b:d3:ea", sdev->multicast.naddr.dst_mac);

    sdev->multicast.aapriv =
		kmalloc(sizeof(struct asgard_async_queue_priv), GFP_KERNEL);

	sdev->multicast.delay = 0;
	sdev->multicast.enable = 0;
	sdev->multicast.nextIdx = 0;

	init_asgard_async_queue(sdev->multicast.aapriv);

	if (sdev->ndev) {
		if (!sdev->ndev->ip_ptr ||
		    !sdev->ndev->ip_ptr->ifa_list) {
			asgard_error(
				"Network Interface with ifindex %d has no IP Address configured!\n",
				ifindex);
			return -EINVAL;
		}

		sdev->self_ip =
			sdev->ndev->ip_ptr->ifa_list->ifa_address;

		if (!sdev->self_ip) {
			asgard_error("self IP Address is NULL!");
			return -EINVAL;
		}

		if (!sdev->ndev->dev_addr) {
			asgard_error("self MAC Address is NULL!");
			return -EINVAL;
		}

		sdev->self_mac = kmalloc(6, GFP_KERNEL);

		memcpy(sdev->self_mac,
		       sdev->ndev->dev_addr, 6);

		asgard_dbg("Using IP: %x and MAC: %pMF",
			   sdev->self_ip,
			   sdev->self_mac);
	}



	sdev->pminfo.multicast_pkt_data.payload =
		kzalloc(sizeof(struct asgard_payload), GFP_KERNEL);

	sdev->pminfo.multicast_pkt_data_oos.payload =
		kzalloc(sizeof(struct asgard_payload), GFP_KERNEL);

	spin_lock_init(
		&sdev->pminfo.multicast_pkt_data_oos.slock);

    asg_mutex_init(&sdev->pminfo.multicast_pkt_data_oos.mlock);

	sdev->asgard_leader_wq = alloc_workqueue(
		"asgard_leader", WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);

    /* Only one active Worker! Due to reading of the ringbuffer ..*/
	sdev->asgard_ringbuf_reader_wq =
		alloc_workqueue("asgard_ringbuf_reader",
				WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);

	for (i = 0; i < MAX_PROTO_INSTANCES; i++)
		sdev->instance_id_mapping[i] = -1;

	// freed by clear_protocol_instances
	sdev->protos =
		kmalloc_array(MAX_PROTO_INSTANCES,
			      sizeof(struct proto_instance *), GFP_KERNEL);

	if (!sdev->protos)
		asgard_error("ERROR! Not enough memory for protocols\n");


	/* set default heartbeat interval */
	//sdev->pminfo.hbi = DEFAULT_HB_INTERVAL;
	sdev->pminfo.hbi = CYCLES_PER_1MS;


    asg_init_workqueues(sdev);


	/* Initialize Timestamping for NIC */
	init_asgard_ts_ctrl_interfaces(sdev);
	init_timestamping(sdev);

	/* Initialize logger base for NIC */
	//init_log_ctrl_base(sdev);

	/*  Initialize protocol instance controller */
	init_proto_instance_ctrl(sdev);

	/*  Initialize multicast controller */
	init_multicast(sdev);

	/* Initialize Control Interfaces for NIC */
	init_asgard_pm_ctrl_interfaces(sdev);
	init_asgard_ctrl_interfaces(sdev);


#endif

    for(i = 0; i < MAX_PROTO_INSTANCES; i++)
        sdev->instance_id_mapping[i] = -1;

    sdev->num_of_proto_instances = 0;

    // Allocate pointer for proto instance placeholders
    sdev->protos = AMALLOC(MAX_PROTO_INSTANCES * sizeof(struct proto_instance *), GFP_KERNEL);

    // Only use consensus protocol for this evaluation.
    //sdev->protos[0] = generate_protocol_instance(sdev, ASGARD_PROTO_CONSENSUS);

    register_protocol_instance(sdev, 1, ASGARD_PROTO_CONSENSUS, 0);

    register_protocol_instance(sdev, 2, ASGARD_PROTO_PP, 1);



#ifdef ASGARD_KERNEL_MODULE

	/* Initialize synbuf for Cluster Membership - one page is enough */
	sdev->synbuf_clustermem =
		create_synbuf("clustermem", 1);

	if (!sdev->synbuf_clustermem) {
		asgard_error("Could not create synbuf for clustermem");
		return -1;
	}

    /* Write ci changes directly to the synbuf
     * ubuf is at least one page which should be enough
     */
	sdev->ci =
		(struct cluster_info *)sdev->synbuf_clustermem->ubuf;

	sdev->ci->overall_cluster_member = 1; /* Node itself is a member */
	sdev->ci->cluster_self_id =
    sdev->pminfo.cluster_id;
#else
    sdev->ci = ACMALLOC(1, sizeof(struct cluster_info), GFP_KERNEL);

#endif

    return 0;
}

