//
// Created by Riesop, Vincent on 09.12.20.
//

#include "membership.h"


#ifdef ASGARD_DPDK
#include <rte_ether.h>
#endif

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][MEMBERSHIP]"

int peer_is_registered(struct pminfo *spminfo , int cid)
{
    int i;

    for (i = 0; i < spminfo->num_of_targets; i++)
        if (spminfo->pm_targets[i].cluster_id == cid)
            return 1;
    return 0;
}

#ifndef ASGARD_KERNEL_MODULE
int hostname_to_ip(const char *hostname)
{
    struct hostent *hp;
    asgard_dbg("Retrieving IP address from host %s ...\n", hostname);

    hp = gethostbyname(hostname);

    if (hp == NULL) {
        asgard_error("gethostbyname() failed\n");
    } else {
        unsigned int i=0;
        while ( hp -> h_addr_list[i] != NULL) {
            asgard_dbg( "Found IP %s for host %s\n", inet_ntoa( *( struct in_addr*)( hp -> h_addr_list[i])), hostname);
            return ((struct in_addr*)( hp -> h_addr_list[i]))->s_addr;
        }
    }
    asgard_error("Did not find IP address for host %s", hostname);
    return -1;
}
#endif


int get_local_index_by_cluster_id(struct asgard_device *sdev, int cluster_id){

    int i;
    for(i=0; i < sdev->pminfo.num_of_targets; i++){
        if(sdev->pminfo.pm_targets[i].cluster_id == cluster_id)
            return i;
    }
    return -1;
}


int add_mac_to_peer_id(struct asgard_device *sdev, char *mac, int id){

    struct asgard_pm_target_info *pmtarget;
    int local_id;

    if(id == sdev->pminfo.cluster_id){
        asgard_dbg("Adding Own Mac Address: %s!\n", mac);

#ifdef ASGARD_DPDK
        sdev->self_mac =AMALLOC(sizeof(struct rte_ether_addr), GFP_KERNEL);
        rte_ether_unformat_addr(mac, (struct rte_ether_addr*)sdev->self_mac);

        if(!rte_is_local_admin_ether_addr(sdev->self_mac)){
            asgard_error("MAC Address specified is not assigned to DPDK Device!\n");
        } else {
            asgard_dbg("Successfully registered MAC address of DPDK enabled Device\n");
        }
#else
        asgard_convert_mac(mac, sdev->self_mac);
#endif

        return 0;
    }

    if (id >= MAX_REMOTE_SOURCES) {
        asgard_error("Reached Limit of remote hosts.\n");
        asgard_error("Limit is=%d\n", MAX_REMOTE_SOURCES);
        return 1;
    }
    asgard_dbg("Looking for local index\n");

    local_id = get_local_index_by_cluster_id(sdev, id);

    if(local_id == -1){
        asgard_error("could not find cluster with cluster id %d\n", id);
        return 0;
    }

    pmtarget = &sdev->pminfo.pm_targets[local_id];
    asgard_dbg("registering mac for local id: %d\n", local_id);
    if (!pmtarget) {
        asgard_error("Pacemaker target is not initialized!\n");
        return 0;
    }

    asgard_dbg("registering mac for cluster id: %d\n", pmtarget->cluster_id);
#ifdef ASGARD_DPDK
    pmtarget->mac_addr = AMALLOC(sizeof(struct rte_ether_addr), 0);

    if(!pmtarget->mac_addr){
        asgard_dbg("failed to allocate memory for mac addr!\n");
        return 0;
    }

    asgard_dbg("converting mac '%s' string to rte_ether_addr struct\n", mac);

    rte_ether_unformat_addr(mac, pmtarget->mac_addr);

    if(!rte_is_valid_assigned_ether_addr(pmtarget->mac_addr)){
        asgard_error("MAC Address specified is not valid!\n");
    } else {
        asgard_dbg("Successfully registered mac address: %s for local id: %d global id: %d\n", mac, local_id, id);
        //asg_print_mac(pmtarget->mac_addr);
    }
#else

    return 0;
#endif
    return 1;

}


/*
 * Returns 0 if ip has been successfully registered
 */
int register_peer_by_ip(struct asgard_device *sdev, int local_id, uint32_t ip, int cluster_id){
 
    struct asgard_pm_target_info *pmtarget;

    if(cluster_id == sdev->pminfo.cluster_id){
        sdev->self_ip = ip;
        return -EINVAL;
    }
    
    asgard_dbg(" %s, local_id=%d, cluster_id=%d\n",  __FUNCTION__ , local_id, cluster_id);

    if (local_id >= MAX_REMOTE_SOURCES) {
        asgard_error("Reached Limit of remote hosts.\n");
        asgard_error("Limit is=%d\n", MAX_REMOTE_SOURCES);
        return -EINVAL;
    }

    pmtarget = &sdev->pminfo.pm_targets[local_id];

    if (!pmtarget) {
        asgard_error("Pacemaker target is NULL\n");
        return -EINVAL;
    }

    /* Local ID is increasing with the number of targets 
     * Initial node state is 2 (not joined yet)
     */
    add_cluster_member(sdev->ci, cluster_id, local_id, 2);


    asg_mutex_init(&pmtarget->pkt_data.mlock);

    pmtarget->alive = 0;
    //pmtarget->pkt_data.sockaddr.sin_family = AF_INET; // IPv4
    pmtarget->pkt_data.naddr.dst_ip = htonl(ip);
    pmtarget->pkt_data.naddr.port = htons(sdev->tx_port);

    pmtarget->cluster_id = cluster_id;
    pmtarget->lhb_ts = 0;
    pmtarget->chb_ts = 0;
    pmtarget->resp_factor = 8;
    pmtarget->cur_waiting_interval = 4;
    pmtarget->pkt_tx_counter = 0;
    pmtarget->pkt_rx_counter = 0;
    pmtarget->pkt_tx_errors = 0;
    pmtarget->fire = 0;

    pmtarget->scheduled_log_replications = 0;
    pmtarget->received_log_replications = 0;

    // payload buffer
    pmtarget->pkt_data.payload = ACMALLOC(1, sizeof(struct asgard_payload), GFP_KERNEL);
    pmtarget->pkt_data.payload->protocols_included = 0;


    pmtarget->hb_pkt_data.payload = ACMALLOC(1, sizeof(struct asgard_payload), GFP_KERNEL);
    pmtarget->hb_pkt_data.payload->protocols_included = 0;

    /* off-by one - so at index num_peers is nothing registered yet */
    pmtarget->aapriv = ACMALLOC(1, sizeof(struct asgard_async_queue_priv), GFP_KERNEL);
    init_asgard_async_queue(pmtarget->aapriv);
 
#ifdef ASGARD_KERNEL_MODULE

    spin_lock_init(&pmtarget->pkt_data.slock);

#endif

    sdev->pminfo.num_of_targets++;

    return 0;
}

int register_peer(struct asgard_device *sdev, uint32_t ip, char *mac, 
                    int current_protocol, 
                    int cluster_id){

    struct asgard_pm_target_info *pmtarget;
    int local_id;

    local_id = sdev->pminfo.num_of_targets;

    if(register_peer_by_ip(sdev, local_id, ip, cluster_id)){
        asgard_error("Failed to init Peer\n");
        return 0;
    }

    pmtarget = &sdev->pminfo.pm_targets[local_id];

    pmtarget->pkt_data.naddr.dst_mac = AMALLOC(sizeof(unsigned char) * 6, GFP_KERNEL);
    memcpy(pmtarget->pkt_data.naddr.dst_mac, mac,
           sizeof(unsigned char) * 6);



#ifdef ASGARD_KERNEL_MODULE
    /* Out of schedule SKB  pre-allocation*/
    pmtarget->pkt_data.skb =
            asgard_reserve_skb(sdev->ndev, ip, mac, NULL);

    skb_set_queue_mapping(
            pmtarget->pkt_data.skb,
            sdev->pminfo.active_cpu); // Queue mapping same for each target i

    pmtarget->hb_pkt_data.skb = 
        asgard_reserve_skb(sdev->ndev,ip, mac, NULL);

    skb_set_queue_mapping(
            pmtarget->hb_pkt_data.skb,
            sdev->pminfo.active_cpu); // Queue mapping same for each target i


#endif


    return 1;
}

#ifndef ASGARD_KERNEL_MODULE
int register_peer_by_name(struct asgard_device *sdev, const char *cur_name, int id){
    int ret = register_peer_by_ip(sdev, hostname_to_ip(cur_name), id);

    if(ret == 0)
        asgard_dbg("Registered peer with: name = %s, port=%d, id = %d\n", cur_name, sdev->tx_port, id);
    return ret;
}
#endif

/* Checks if at least 3 Nodes have joined the cluster yet  */
int check_warmup_state(struct asgard_device *sdev, struct pminfo *spminfo)
{
    int i;
    int live_nodes = 0;

    if (sdev->warmup_state == WARMING_UP) {
        if (spminfo->num_of_targets < 2) {
            asgard_error("number of targets in cluster is less than 2 (%d)\n", spminfo->num_of_targets);
            return 1;
        }

        // Do not start Leader Election until at least three nodes are alive in the cluster
        for (i = 0; i < spminfo->num_of_targets; i++)
            if (spminfo->pm_targets[i].alive)
                live_nodes++;

        if (live_nodes < 2) {
            //asgard_error("live nodes is less than 2\n");
            return 1;
        }

        // Starting all protocols
        for (i = 0; i < sdev->num_of_proto_instances; i++) {
            if (sdev->protos != NULL && sdev->protos[i] != NULL &&
                sdev->protos[i]->ctrl_ops.start != NULL) {
                sdev->protos[i]->ctrl_ops.start(
                        sdev->protos[i]);
            } else {
                asgard_dbg(
                        "protocol instance %d not initialized",
                        i);
            }
        }
        asgard_dbg("Warmup done!\n");
        sdev->warmup_state = WARMED_UP;
        update_leader(sdev, &sdev->pminfo);
    }
    return 0;
}


void update_self_state(struct cluster_info *ci, node_state_t state) {


    switch(state) {
        case FOLLOWER:
            ci->node_state = 1;
            break;
        case CANDIDATE:
            ci->node_state = 2;
            break;
        case LEADER:
            ci->node_state = 3;
            break;
        default:
            ci->node_state = 0; /* Memory is zero initialized, so this matches the uninit state */
    }
}

void add_cluster_member(struct cluster_info* ci, int cluster_id, int local_id, uint8_t init_state)
{
    if(!ci) {
        asgard_error("BUG detected. cluster info is NULL\n");
        return;
    }

    if(local_id > MAX_CLUSTER_MEMBER){
        asgard_error("Local cluster ID (%d) is higher than hardcoded current cluster limit (%d)\n", local_id, MAX_CLUSTER_MEMBER);
        return;
    }

    ci->member_info[local_id].state = init_state;
    ci->member_info[local_id].global_cluster_id = cluster_id;
    ci->overall_cluster_member++;

}

void update_cluster_member(struct cluster_info* ci, int local_id, uint8_t state)
{
    uint8_t prev_state;

    if(!ci) {
        asgard_error("BUG detected. cluster info is NULL\n");
        return;
    }

    if(local_id > MAX_CLUSTER_MEMBER){
        asgard_error("Local Cluster ID (%d) is higher than hardcoded current cluster limit (%d)\n", local_id, MAX_CLUSTER_MEMBER);
        return;
    }

    if(local_id < 0) {
        asgard_error("Local Cluster ID (%d) is invalid\n", local_id);
        return;
    }

    prev_state = ci->member_info[local_id].state;

    /* TODO: Locking of cluster info data (ci)? */

    /* Cluster Member state changed */
    if(state != prev_state){

        /* Cluster Member became marked as dead
         * Only update cluster member if it was in the cluster previously
         */
        if(state == 0 && prev_state != 2) {
            ci->dead_cluster_member++;
            ci->cluster_dropouts++;
            asgard_dbg("Cluster Member with local_id: %d dropped out\n", local_id);
        }

        /* Cluster Member became alive */
        if(state == 1) {
            if(prev_state == 2){
                // cluster member was never dead, joining the first time
                asgard_dbg("New Cluster Member with local_id: %d joined\n", local_id);
            } else {
                asgard_dbg("Cluster Member with local_id: %d became alive again \n", local_id);
                ci->dead_cluster_member--; 
            }
            ci->active_cluster_member++;
            ci->cluster_joins++;

        }

        /* Update the state now */
        ci->member_info[local_id].state = state;
    }

    ci->last_update_timestamp = ASGARD_TIMESTAMP;

    /* TODO: Switch dual buffer ? Unocking? */


}
