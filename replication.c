//
// Created by Riesop, Vincent on 10.12.20.
//

#include <stdlib.h>
#include <string.h>

#include "replication.h"

#include "leader.h"
#if ASGARD_KERNEL_MODULE == 0
#include "list.h"
#endif
#include "pktqueue.h"

#include "payload.h"
#include "kvstore.h"


void update_next_retransmission_request_idx(struct consensus_priv *priv)
{
    int i;
    int first_re_idx = -2;
    int cur_idx = -2;
    int skipped = 0;
    int cur_buf_idx;

    if(priv->sm_log.last_idx == -1){
        asgard_dbg("Nothing has been received yet!\n");
        return;
    }

    /* stable_idx + 1 always points to an invalid entry and
     * if stable_idx != last_idx is also true, we have found
     * a missing entry. The latter case is true for all loop iterations
     *
     *
     */
    for(i = priv->sm_log.stable_idx + 1; i < priv->sm_log.last_idx; i++) {

        // if request has already been sent, skip indicies that may be included
        // ... in the next log replication packet
        if(priv->sm_log.next_retrans_req_idx == i){
            i += priv->max_entries_per_pkt - 1;
            skipped = 1;
            continue;
        }
        cur_buf_idx = consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if(cur_buf_idx == -1) {
            asgard_error("Invalid idx. could not convert to buffer idx in %s",__FUNCTION__);
            return;
        }

        if(!priv->sm_log.entries[cur_buf_idx]){
            cur_idx = i;

            if (first_re_idx == -2)
                first_re_idx = i;

            // we can use first found idx
            if(priv->sm_log.next_retrans_req_idx == -2)
                break;

            // found next missing entry after prev request
            if(i > priv->sm_log.next_retrans_req_idx)
                break;
        }
    }

    // note: with next_retrans_req_idx = -2, we would start with requesting the last missing idx
    // .... this should not be a problem though
    priv->sm_log.next_retrans_req_idx
            = priv->sm_log.next_retrans_req_idx < cur_idx ? cur_idx : first_re_idx;
}

void async_clear_queue( struct asgard_async_queue_priv *queue)
{
    struct asgard_async_pkt *entry, *tmp_entry;

    if(!queue) {
        asgard_error("BUG - tried to clear uninitialized async queue\n");
        return;
    }

    if(queue->doorbell <= 0)
        return;

    pthread_rwlock_wrlock(&(queue->queue_rwlock));

    if(list_empty(&(queue->head_of_async_pkt_queue)))
        goto unlock;

    list_for_each_entry_safe(entry, tmp_entry, &(queue->head_of_async_pkt_queue), async_pkts_head)
    {
        if(entry) {
            list_del(&(entry->async_pkts_head));
            AFREE(entry);
            asgard_dbg("freed apkt entry\n");
        }
    }


    unlock:
    pthread_rwlock_unlock(&(queue->queue_rwlock));
}



int do_prepare_log_replication_multicast(struct asgard_device *sdev){
    int ret;

    struct asgard_async_pkt *apkt = NULL;

    if (!sdev->is_leader)
        return -1;

    asgard_error("WARNING! MULTICAST NOT PORTED! \n");

    // ret = setup_append_multicast_msg(sdev, get_payload_ptr(apkt));

    // TODO: setup append mutlicast message

    // handle errors
    if(ret < 0) {
        //kfree(apkt->payload);
        AFREE(apkt);
        return ret;
    }

    enqueue_async_pkt(sdev->multicast.aapriv, apkt);

    ring_aa_doorbell(sdev->multicast.aapriv);

    return ret;
}


int do_prepare_log_replication(struct asgard_device *sdev, int target_id, int32_t next_index, int retrans) {
    struct consensus_priv *cur_priv = NULL;
    int j, ret;
    struct pminfo *spminfo = &sdev->pminfo;
    int more = 0;
    struct asgard_async_pkt *apkt = NULL;

    if(sdev->is_leader == 0)
        return -1; // node lost leadership

    if(spminfo->pm_targets[target_id].alive == 0) {
        asgard_dbg("target is not alive\n");
        return -1; // Current target is not alive!
    }

    // iterate through consensus protocols and include LEAD messages if node is leader
    for (j = 0; j < sdev->num_of_proto_instances; j++) {

        if (sdev->protos[j] != NULL && sdev->protos[j]->proto_type == ASGARD_PROTO_CONSENSUS) {

            // get corresponding local instance data for consensus
            cur_priv =
                    (struct consensus_priv *)sdev->protos[j]->proto_data;

            if (cur_priv->nstate != LEADER)
                continue;

            // asgard_dbg("enqueuing pkt for target %d - num_of_proto_instances  %d\n", target_id,sdev->num_of_proto_instances);
            apkt = create_async_pkt(spminfo->pm_targets[target_id].pkt_data.sockaddr);

            if(!apkt) {
                asgard_error("Could not allocate async pkt!\n");
                continue;
            }

            // asgard_dbg("Calling setup_append_msg with next_index=%d, retrans=%d, target_id=%d", next_index, retrans, target_id);

            ret = setup_append_msg(cur_priv,
                                   apkt->pkt_data.payload,
                                   sdev->protos[j]->instance_id,
                                   target_id,
                                   next_index,
                                   retrans);

            // handle errors
            if(ret < 0) {
                //kfree(apkt->payload);
                // asgard_error("Something went wrong in the setup append msg function!\n");
                AFREE(apkt);
                continue;
            }

            more += ret;

            sdev->pminfo.pm_targets[target_id].scheduled_log_replications++;

            if(retrans)
                push_front_async_pkt(spminfo->pm_targets[target_id].aapriv, apkt);
            else
                enqueue_async_pkt(spminfo->pm_targets[target_id].aapriv, apkt);

            ring_aa_doorbell(spminfo->pm_targets[target_id].aapriv);
        }
    }

    return more;

}

void *prepare_log_replication_handler(void *data)
{
    struct asgard_leader_pkt_work_data *aw = data;
    int more = 0;

    more = do_prepare_log_replication(aw->sdev, aw->target_id, aw->next_index, aw->retrans);

    /* not ready to prepare log replication */
    if(more < 0){
        goto cleanup;
    }

    if(!list_empty(&aw->sdev->consensus_priv->sm_log.retrans_head[aw->target_id]) || more) {
        check_pending_log_rep_for_target(aw->sdev, aw->target_id);
    }

cleanup:
    AFREE(aw);
    return NULL;
}


void schedule_log_rep(struct asgard_device *sdev, int target_id, int next_index, int32_t retrans, int multicast_enabled)
{
    struct asgard_leader_pkt_work_data *work = NULL;
    pthread_t pt_logrep;
    int more = 0;

    // if leadership has been dropped, do not schedule leader work
    if(sdev->is_leader == 0) {
        asgard_dbg("node is not a leader\n");
        return;
    }

    // if pacemaker has been stopped, do not schedule leader tx work
    if(sdev->pminfo.state != ASGARD_PM_EMITTING) {
        asgard_dbg("pacemaker not in emitting state\n");
        return;
    }


    if (multicast_enabled > 0 ){
        do{
            more = do_prepare_log_replication_multicast(sdev); // TODO: warn, not ported yet!
        }
        while(more > 0);
    }
        //prepare_log_replication_multicast_handler(sdev);
    else{
        // freed by prepare_log_replication_handler
        work =AMALLOC(sizeof(struct asgard_leader_pkt_work_data), GFP_KERNEL);
        work->sdev = sdev;
        work->next_index = next_index;
        work->retrans = retrans;

        work->target_id = target_id;
        //asgard_dbg("scheduling prep handler for target node %d\n", target_id);

        pthread_create(&pt_logrep, NULL, prepare_log_replication_handler, work);

        // TODO: schedule pthread worker to process log rep
        /*INIT_WORK(&work->work, prepare_log_replication_handler);

        if(!queue_work(sdev->asgard_leader_wq, &work->work)) {
            asgard_dbg("Work item not put in queue ..");
            sdev->bug_counter++;
            if(work)
                AFREE(work); //right?
            return;
        } */

    }
}


int check_target_id(struct consensus_priv *priv, int target_id)
{

    if (target_id < 0)
        goto error;

    if (target_id > priv->sdev->pminfo.num_of_targets)
        goto error;

    return 1;
    error:
    asgard_dbg("Target ID is not valid: %d\n", target_id);
    return 0;
}


int32_t get_match_idx(struct consensus_priv *priv, int target_id)
{
    int32_t match_index;

    if (check_target_id(priv, target_id))
        match_index = priv->sm_log.match_index[target_id];
    else
        match_index = -1;

    return match_index;
}

int32_t get_next_idx(struct consensus_priv *priv, int target_id)
{
    int32_t next_index;

    if (check_target_id(priv, target_id)){
        next_index = priv->sm_log.next_index[target_id];
        if(next_index > priv->sm_log.last_idx)
            next_index = -2;
    } else
        next_index = -2;

    return next_index;
}

int32_t get_next_idx_for_target(struct consensus_priv *cur_priv, int target_id, int *retrans)
{
    int32_t next_index = -1;
    struct retrans_request *cur_rereq = NULL;


    pthread_rwlock_wrlock(&cur_priv->sm_log.retrans_list_lock[target_id]);

    if(cur_priv->sm_log.retrans_entries[target_id] > 0) {

        cur_rereq = list_first_entry_or_null(&cur_priv->sm_log.retrans_head[target_id], struct retrans_request, retrans_req_head);

        if(!cur_rereq) {
            asgard_dbg("entry is null!");
            goto unlock;
        }

        cur_priv->sm_log.retrans_entries[target_id]--;

        next_index = cur_rereq->request_idx;
        list_del(&cur_rereq->retrans_req_head);
        AFREE(cur_rereq);
        *retrans = 1;
    } else {
        next_index = get_next_idx(cur_priv, target_id);
        *retrans = 0;
    }

unlock:
    pthread_rwlock_unlock(&cur_priv->sm_log.retrans_list_lock[target_id]);
    return next_index;
}



void check_pending_log_rep_for_target(struct asgard_device *sdev, int target_id)
{
    int32_t next_index, match_index;
    int retrans;
    struct consensus_priv *cur_priv = NULL;
    int j;

    // TODO: utilise all protocol instances, currently only support for one consensus proto instance - the first
    for (j = 0; j < sdev->num_of_proto_instances; j++) {
        if (sdev->protos[j] != NULL && sdev->protos[j]->proto_type == ASGARD_PROTO_CONSENSUS) {
            // get corresponding local instance data for consensus
            cur_priv =
                    (struct consensus_priv *)sdev->protos[j]->proto_data;
            break;
        }
    }

    if(cur_priv == NULL){
        asgard_error("BUG. Invalid local proto data");
        return;
    }

    if(sdev->is_leader == 0)
        return;

    if(sdev->pminfo.state != ASGARD_PM_EMITTING)
        return;

    next_index = get_next_idx_for_target(cur_priv, target_id, &retrans);

    if(next_index < 0)
        return;

    match_index = get_match_idx(cur_priv, target_id);

    if(next_index == match_index) {
        asgard_dbg("nothing to send for target %d\n", target_id);
        return;
    }

    schedule_log_rep(sdev, target_id, next_index, retrans, 0);
}

void check_pending_log_rep(struct asgard_device *sdev)
{
    int i;

    if(sdev->is_leader == 0)
        return;

    if(sdev->pminfo.state != ASGARD_PM_EMITTING)
        return;

    if (sdev->multicast.enable)
        check_pending_log_rep_for_multicast(sdev);
    else{
        for(i = 0; i < sdev->pminfo.num_of_targets; i++) {
            check_pending_log_rep_for_target(sdev, i);
        }
    }
}