#ifndef ASGUARD_ASGARD_UFACE_H
#define ASGUARD_ASGARD_UFACE_H

#include <linux/types.h>
#include <asguard/consensus.h>


// arbitrary limit
#define MAX_CLUSTER_MEMBER 512


struct cluster_member_info {

    /*
     * 0 = dead
     * 1 = alive
     * 2 = never seen before
     * 253 other states are currently free ;)
     */
    u8 state;

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
void add_cluster_member(struct cluster_info* ci, int cluster_id, u8 init_state);
void update_cluster_member(struct cluster_info* ci, int cluster_id, u8 state);
void update_self_state(struct cluster_info *ci, enum node_state state);



#endif //ASGUARD_ASGARD_UFACE_H
