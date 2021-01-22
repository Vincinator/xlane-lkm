#pragma once


#ifndef ASGARD_KERNEL_MODULE
#include <stdlib.h>
#endif



#include "consensus.h"


// arbitrary limit
#define MAX_CLUSTER_MEMBER 512

struct cluster_member_info {
    /*
     * 0 = dead
     * 1 = alive
     * 2 = never seen before
     * 253 other states are currently free ;)
     */
    uint8_t state;

    int global_cluster_id;

    /* ... may contain additional fields in the future ... */

};

/*
 * Cluster Info from an ASGARD perspective
 */
struct cluster_info {

    /* Unique cluster id of node that captures this info */
    int cluster_self_id;

    /*
     * 0 = not initialized
     * 1 = FOLLOWER
     * 2 = CANDIDATE
     * 3 = LEADER
     * 4 = FAILURE
     */
    int node_state;

    /* Gets updated every time cluster_info is updated */
    uint64_t last_update_timestamp;

    /* Number of unique cluster members dead or alive since last reset/start of cluster */
    int overall_cluster_member;

    /* Number of unique cluster members currently alive in the cluster */
    int active_cluster_member;

    /* Number of unique cluster members currently marked as dead in the cluster */
    int dead_cluster_member;

    /* How often did the cluster accept any node */
    int cluster_joins;

    /* How often did the cluster drop out a node because it was considered dead */
    int cluster_dropouts;

    /*
     * Information for each unique cluster member
     *
     * Array index corresponds to unique ID of that cluster member
     */
    struct cluster_member_info member_info[MAX_CLUSTER_MEMBER];

};
int check_warmup_state(struct asgard_device *sdev, struct pminfo *spminfo);
int register_peer_by_name(struct asgard_device *sdev, const char *cur_name, int id);
int add_mac_to_peer_id(struct asgard_device *sdev, char *mac, int id);
int register_peer_by_ip(struct asgard_device *sdev, uint32_t ip, int id);
int peer_is_registered(struct pminfo *spminfo , int cid);
void update_cluster_member(struct cluster_info* ci, int local_id, uint8_t state);
void update_self_state(struct cluster_info *ci, node_state_t state);
int hostname_to_ip(const char *hostname);
void add_cluster_member(struct cluster_info* ci, int cluster_id, int local_id, uint8_t init_state);