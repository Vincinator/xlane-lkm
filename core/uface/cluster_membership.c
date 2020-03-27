#include <asgard_uface.h>
#include <linux/slab.h>
#include <asguard/logger.h>
#include <asguard/asguard.h>


void add_cluster_member(struct cluster_info* ci, int cluster_id, u8 init_state)
{
    if(!ci) {
        asguard_error("BUG detected. cluster info is NULL\n");
        return;
    }

    if(cluster_id > MAX_CLUSTER_MEMBER){
        asguard_error("Cluster ID (%d) is higher than hardcoded current cluster limit (%d)\n", cluster_id, MAX_CLUSTER_MEMBER);
        return;
    }

    ci->member_info[cluster_id].state = init_state;
    ci->overall_cluster_member++;

}

void update_cluster_member(struct cluster_info* ci, int cluster_id, u8 state)
{
    u8 prev_state;

    if(!ci) {
        asguard_error("BUG detected. cluster info is NULL\n");
        return;
    }

    if(cluster_id > MAX_CLUSTER_MEMBER){
        asguard_error("Cluster ID (%d) is higher than hardcoded current cluster limit (%d)\n", cluster_id, MAX_CLUSTER_MEMBER);
        return;
    }

    prev_state = ci->member_info[cluster_id].state;

    /* TODO: Locking of cluster info data (ci)? */

    /* Cluster Member state changed */
    if(state != prev_state){

        /* Cluster Member became marked as dead */
        if(state == 0) {
            ci->dead_cluster_member++;
            ci->active_cluster_member--;
            ci->cluster_dropouts++;
        }

        /* Cluster Member became alive */
        if(state == 1) {
            ci->dead_cluster_member--;
            ci->active_cluster_member++;
            ci->cluster_joins++;
        }

        /* Update the state now */
        ci->member_info[cluster_id].state = state;
    }

    ci->last_update_timestamp = RDTSC_ASGUARD;

    /* TODO: Switch dual buffer ? Unocking? */


}