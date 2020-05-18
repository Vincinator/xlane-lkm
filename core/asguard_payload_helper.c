
#include <linux/slab.h>


#include <asguard/consensus.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>
#include <asguard/logger.h>
#include <asguard/asguard_async.h>

#include <linux/workqueue.h>



/* Returns a pointer to the <n>th protocol of <spay>
 *
 * If less than n protocols are included, a NULL ptr is returned
 */
char *asguard_get_proto(struct asguard_payload *spay, int n)
{
	int i;
	char *cur_proto;
	int cur_offset = 0;

	cur_proto = spay->proto_data;

	if (spay->protocols_included < n) {
		asguard_error("only %d protocols are included, requested %d", spay->protocols_included, n);
		return NULL;
	}

	// Iterate through existing protocols
	for (i = 0; i < n; i++) {
		cur_offset = GET_PROTO_OFFSET_VAL(cur_proto);
		cur_proto = cur_proto + cur_offset;
	}

	return cur_proto;
}
EXPORT_SYMBOL(asguard_get_proto);


/* Protocol offsets and protocols_included must be correct before calling this method.
 *
 * Sets protocol id and reserves space in the asguard payload,
 * if the required space is available.
 *
 * returns a pointer to the start of that protocol payload memory area.
 */
char *asguard_reserve_proto(u16 instance_id, struct asguard_payload *spay, u16 proto_size)
{
	int i;
	char *cur_proto;
	int proto_offset = 0;
	int cur_offset = 0;

	u16 *pid, *poff;

	if(!spay){
	    asguard_error("payload is NULL\N");
	    return NULL;
	}

	cur_proto = spay->proto_data;

	// Check if protocol instance already exists in payload
	for (i = 0; i < spay->protocols_included; i++) {

		// if (instance_id == GET_PROTO_TYPE_VAL(cur_proto))
		// 	goto reuse; // reuse existing payload part for this instance id

		cur_offset = GET_PROTO_OFFSET_VAL(cur_proto);
		cur_proto = cur_proto + cur_offset;
		proto_offset += cur_offset;

	}
	spay->protocols_included++;

	if (unlikely(proto_offset + proto_size > MAX_ASGUARD_PAYLOAD_BYTES)) {
		asguard_error("Not enough space in asguard payload\n");
		spay->protocols_included--;
		return NULL;
	}

	pid =  GET_PROTO_TYPE_PTR(cur_proto);
	poff = GET_PROTO_OFFSET_PTR(cur_proto);

	*pid = instance_id;
	*poff = proto_size;


	return cur_proto;
}
EXPORT_SYMBOL(asguard_reserve_proto);


int _check_target_id(struct consensus_priv *priv, int target_id)
{

	if (target_id < 0)
		goto error;

	if (target_id > priv->sdev->pminfo.num_of_targets)
		goto error;

	return 1;
error:
	asguard_dbg("Target ID is not valid: %d\n", target_id);
	return 0;
}

s32 _get_match_idx(struct consensus_priv *priv, int target_id)
{
	s32 match_index;

	if (_check_target_id(priv, target_id))
		match_index = priv->sm_log.match_index[target_id];
	else
		match_index = -1;

	return match_index;
}
EXPORT_SYMBOL(_get_match_idx);

s32 _get_next_idx(struct consensus_priv *priv, int target_id)
{
	s32 next_index;

	if (_check_target_id(priv, target_id)){
        next_index = priv->sm_log.next_index[target_id];
        if(next_index > priv->sm_log.last_idx)
            next_index = -2;
	} else
		next_index = -2;

	return next_index;
}

s32 _get_last_idx_safe(struct consensus_priv *priv)
{
	if (priv->sm_log.last_idx < -1)
		priv->sm_log.last_idx = -1; // repair last index!

	return priv->sm_log.last_idx;
}

int _log_is_faulty(struct consensus_priv *priv)
{

	if (!priv)
		return 1;

	if (!priv->sm_log.entries)
		return 1;

	//  if required add more consistency checks here..

	return 0;
}

s32 _get_prev_log_term(struct consensus_priv *cur_priv, s32 con_idx)
{

    s32 idx;


	if (con_idx == -1) {
		// Logs are empty, because next index points to first element.
		// Thus, there was no prev log term. And therefore we can use the
		// current term.
		return cur_priv->term;
	}

	if (con_idx < -1) {
		asguard_dbg("invalid value - idx=%d\n", idx);
		return -1;
	}

    idx = consensus_idx_to_buffer_idx(&cur_priv->sm_log, con_idx);

	if(idx < 0) {
	    asguard_dbg("Error converting consensus idx to buffer in %s", __FUNCTION__);
	    return -1;
	}

	if (idx > cur_priv->sm_log.last_idx) {
		asguard_dbg("idx %d > cur_priv->sm_log.last_id (%d) \n", idx, cur_priv->sm_log.last_idx);

		return -1;
	}

	if (idx > MAX_CONSENSUS_LOG) {
		asguard_dbg("BUG! idx (%d) > MAX_CONSENSUS_LOG (%d) \n", idx, MAX_CONSENSUS_LOG);
		return -1;
	}

	if (!cur_priv->sm_log.entries[idx]) {
		asguard_dbg("BUG! entries is null at index %d\n", idx);
		return -1;
	}

	return cur_priv->sm_log.entries[idx]->term;
}

int setup_cluster_join_advertisement(struct asguard_payload *spay, int advertise_id, u32 ip, unsigned char *mac)
{
    char *pkt_payload_sub;

    pkt_payload_sub = asguard_reserve_proto(1, spay, ASGUARD_PROTO_CON_PAYLOAD_SZ);

    if(!pkt_payload_sub)
        return -1;

    set_le_opcode_ad((unsigned char *)pkt_payload_sub, ADVERTISE, advertise_id, ip, mac );

    return 0;
}
EXPORT_SYMBOL(setup_cluster_join_advertisement);


int setup_alive_msg(struct consensus_priv *cur_priv, struct asguard_payload *spay, int instance_id)
{
	char *pkt_payload_sub;

	pkt_payload_sub = asguard_reserve_proto(instance_id, spay, ASGUARD_PROTO_CON_PAYLOAD_SZ);

	if(!pkt_payload_sub)
	    return -1;

	set_le_opcode((unsigned char *)pkt_payload_sub, ALIVE, cur_priv->term, cur_priv->sm_log.commit_idx, cur_priv->sm_log.stable_idx, cur_priv->sdev->hb_interval);

	return 0;
}
EXPORT_SYMBOL(setup_alive_msg);

int setup_append_msg(struct consensus_priv *cur_priv, struct asguard_payload *spay, int instance_id, int target_id, s32 next_index, int retrans)
{
	s32 local_last_idx;
	s32 prev_log_term, leader_commit_idx;
	s32 num_entries = 0;
	char *pkt_payload_sub;
	int more = 0;

	// Check if entries must be appended
	local_last_idx = _get_last_idx_safe(cur_priv);


	/* Update next_index inside next_lock critical section */
    if(!retrans){
        mutex_lock(&cur_priv->sm_log.next_lock);
        next_index = _get_next_idx(cur_priv, target_id);

        if(next_index == -2) {
            mutex_unlock(&cur_priv->sm_log.next_lock);
            return -2;
        }

    }


    if (next_index == -1) {
		asguard_dbg("Invalid target id resulted in invalid next_index!\n");
        if(!retrans)
            mutex_unlock(&cur_priv->sm_log.next_lock);
        return -1;
	}

	// asguard_dbg("PREP AE: local_last_idx=%d, next_index=%d\n", local_last_idx, next_index);
	prev_log_term = _get_prev_log_term(cur_priv, next_index - 1);

	if (prev_log_term < 0) {
		asguard_error("BUG! - prev_log_term is invalid. next_index=%d, retrans=%d, target_id=%d\n", next_index, retrans, target_id );
        if(!retrans)
            mutex_unlock(&cur_priv->sm_log.next_lock);
        return -1;
	}

	leader_commit_idx = cur_priv->sm_log.commit_idx;

	if (local_last_idx >= next_index) {
		// Facts:
		//	- local_last_idx >= next_index
		//  - Must include entries in next consensus append message
		//  - thus, num_of_entries will not be 0

		// Decide how many entries to update for the current target

		if (cur_priv->max_entries_per_pkt < local_last_idx - next_index + 1) {
			num_entries = cur_priv->max_entries_per_pkt;
			more = 1;
		} else {
			num_entries = (local_last_idx - next_index + 1);
			more = 0;
		}

		if(num_entries <= 0) {
		    asguard_dbg("No entries to replicate\n");
            if(!retrans)
                mutex_unlock(&cur_priv->sm_log.next_lock);
            return -1;
		}


		/* Update next_index without receiving the response from the target.
		 *
		 * If the receiver rejects this append command, this node will set
		 * the next_index to the last known safe index of the receivers log.
		 *
		 * The receiver sends the last known safe index with the append reply.
		 *
		 * next_index must be read and increased in within critical section of next_lock!
		 * next_index is only updated if this is not a retransmission!
		 */
		if(!retrans)
			cur_priv->sm_log.next_index[target_id] += num_entries;

	} else {
        if(!retrans)
            mutex_unlock(&cur_priv->sm_log.next_lock);
        return -2;
	}

    if(!retrans)
        mutex_unlock(&cur_priv->sm_log.next_lock);

   /* asguard_dbg("retrans=%d, target_id=%d, leader_last_idx=%d, next_idx=%d, prev_log_term=%d, num_entries=%d\n",
                retrans,
                target_id,
                local_last_idx,
                next_index,
                prev_log_term,
                num_entries);*/

	// reserve space in asguard heartbeat for consensus LEAD
	pkt_payload_sub =
		asguard_reserve_proto(instance_id, spay,
						ASGUARD_PROTO_CON_AE_BASE_SZ + (num_entries * AE_ENTRY_SIZE));

	if (!pkt_payload_sub) {
        return -1;
    }

	set_ae_data(pkt_payload_sub,
		cur_priv->term,
		cur_priv->node_id,
		// previous is one before the "this should be send next" index
		next_index,
		prev_log_term,
		leader_commit_idx,
		cur_priv,
		num_entries,
		more);

	return more;
}
EXPORT_SYMBOL(setup_append_msg);

int setup_append_multicast_msg(struct asguard_device *sdev, struct asguard_payload *spay)
{
    s32 local_last_idx;
    s32 prev_log_term, leader_commit_idx;
    s32 num_entries = 0;
    s32 next_index;
    char *pkt_payload_sub;
    int more = 0;

    // Check if entries must be appended
    local_last_idx = _get_last_idx_safe(sdev->consensus_priv);

    if (sdev->multicast.nextIdx == -1) {
        asguard_dbg("Invalid target id resulted in invalid next_index!\n");
        return -1;
    }

    // asguard_dbg("PREP AE: local_last_idx=%d, next_index=%d\n", local_last_idx, next_index);
    prev_log_term = _get_prev_log_term(sdev->consensus_priv, sdev->multicast.nextIdx - 1);

    if (prev_log_term < 0) {
        asguard_error("BUG! - prev_log_term is invalid. next_index=%d\n", next_index);
        return -1;
    }

    mutex_lock(&sdev->consensus_priv->sm_log.next_lock);
    next_index = sdev->multicast.nextIdx;

    if (local_last_idx >= next_index) {
        // Facts:
        //	- local_last_idx >= next_index
        //  - Must include entries in next consensus append message
        //  - thus, num_of_entries will not be 0

        // Decide how many entries to update for the current target

        if (sdev->consensus_priv->max_entries_per_pkt < local_last_idx - next_index + 1) {
            num_entries = sdev->consensus_priv->max_entries_per_pkt;
            more = 1;
        } else {
            num_entries = (local_last_idx - next_index + 1);
            more = 0;
        }

        if(num_entries <= 0) {
            asguard_dbg("No entries to replicate\n");
            mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);
            return -1;
        }

    } else {
        mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);
        return -2;
    }

    /* asguard_dbg("retrans=%d, target_id=%d, leader_last_idx=%d, next_idx=%d, prev_log_term=%d, num_entries=%d\n",
                 retrans,
                 target_id,
                 local_last_idx,
                 next_index,
                 prev_log_term,
                 num_entries);*/

    // reserve space in asguard heartbeat for consensus LEAD

    pkt_payload_sub =
            asguard_reserve_proto(sdev->consensus_priv->ins->instance_id, spay,
                                  ASGUARD_PROTO_CON_AE_BASE_SZ + (num_entries * AE_ENTRY_SIZE));

    if (!pkt_payload_sub) {
        mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);
        return -1;
    }

    leader_commit_idx = next_index + num_entries - 1;

    set_ae_data(pkt_payload_sub,
                sdev->consensus_priv->term,
                sdev->consensus_priv->node_id,
            // previous is one before the "this should be send next" index
                next_index,
                prev_log_term,
                leader_commit_idx,
                sdev->consensus_priv,
                num_entries,
                more);

    sdev->multicast.nextIdx += num_entries;
    sdev->consensus_priv->sm_log.commit_idx += num_entries - 1;
    mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);

    return more;
}
EXPORT_SYMBOL(setup_append_multicast_msg);

/* Must be called after the asguard packet has been emitted.
 */
void invalidate_proto_data(struct asguard_device *sdev, struct asguard_payload *spay)
{

	// free previous piggybacked protocols
	spay->protocols_included = 0;

}
EXPORT_SYMBOL(invalidate_proto_data);


int _do_prepare_log_replication(struct asguard_device *sdev, int target_id, s32 next_index, int retrans)
{
	struct consensus_priv *cur_priv = NULL;
	int j, ret;
	struct pminfo *spminfo = &sdev->pminfo;
	int more = 0;
	struct asguard_async_pkt *apkt = NULL;

	if(sdev->is_leader == 0)
		return -1; // node lost leadership

	if(spminfo->pm_targets[target_id].alive == 0) {
		return -1; // Current target is not alive!
	}

    // iterate through consensus protocols and include LEAD messages if node is leader
    for (j = 0; j < sdev->num_of_proto_instances; j++) {

        if (sdev->protos[target_id] != NULL && sdev->protos[j]->proto_type == ASGUARD_PROTO_CONSENSUS) {

            // get corresponding local instance data for consensus
            cur_priv =
                    (struct consensus_priv *)sdev->protos[j]->proto_data;

            if (cur_priv->nstate != LEADER)
                continue;

            apkt = create_async_pkt(sdev->ndev,
                                    spminfo->pm_targets[target_id].pkt_data.naddr.dst_ip,
                                    &spminfo->pm_targets[target_id].pkt_data.naddr.dst_mac[0]);

            if(!apkt) {
                asguard_error("Could not allocate async pkt!\n");
                continue;
            }

            // asguard_dbg("Calling setup_append_msg with next_index=%d, retrans=%d, target_id=%d", next_index, retrans, target_id);

            ret = setup_append_msg(cur_priv,
                                   get_payload_ptr(apkt),
                                   sdev->protos[j]->instance_id,
                                   target_id,
                                   next_index,
                                   retrans);

            // handle errors
            if(ret < 0) {
                //kfree(apkt->payload);
                kfree(apkt);
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

int _do_prepare_log_replication_multicast(struct asguard_device *sdev, u32 dst_ip, unsigned char *dst_mac){
    int ret;

    struct asguard_async_pkt *apkt = NULL;

    if (!sdev->is_leader)
        return -1;

    apkt = create_async_pkt(sdev->ndev, dst_ip, dst_mac);

    if(!apkt) {
        asguard_error("Could not allocate async pkt!\n");
        return -1;
    }

    // asguard_dbg("Calling setup_append_msg with next_index=%d, retrans=%d, target_id=%d", next_index, retrans, target_id);

    ret = setup_append_multicast_msg(sdev, get_payload_ptr(apkt));
        // handle errors
    if(ret < 0) {
        //kfree(apkt->payload);
        kfree(apkt);
        return ret;
    }

    enqueue_async_pkt(sdev->multicast.aapriv, apkt);

    ring_aa_doorbell(sdev->multicast.aapriv);

    return ret;
}

void pull_consensus_requests_from_rb( struct work_struct *w);


void _schedule_update_from_userspace(struct asguard_device *sdev, struct synbuf_device *syndev) {

    struct asguard_ringbuf_read_work_data *work = NULL;

    if(!sdev->is_leader || sdev->pminfo.state != ASGUARD_PM_EMITTING)
        return;

    // freed by pull_consensus_requests_from_rb
    work = kmalloc(sizeof(struct asguard_ringbuf_read_work_data), GFP_KERNEL);

    if(!work) {
        asguard_error("Could not allocate memory for work item in %s\n", __FUNCTION__);
        return;
    }

    work->rb = (struct asg_ring_buf *) syndev->ubuf;
    work->sdev = sdev;

    INIT_DELAYED_WORK(&work->dwork, pull_consensus_requests_from_rb);

    //if(!queue_delayed_work(sdev->asguard_ringbuf_reader_wq, &work->dwork, 0)) {
    if(!schedule_delayed_work_on(17,  &work->dwork, 0)) {
        asguard_dbg("Work item not put in queue ..");
        sdev->bug_counter++;
        if(work)
            kfree(work); //right?
        return;
    }

}
EXPORT_SYMBOL(_schedule_update_from_userspace);

/*
 * Copies ringbuffer data from user space to asgard logs for consensus
 */
void pull_consensus_requests_from_rb(struct work_struct *w) {
    struct asguard_ringbuf_read_work_data *aw = NULL;
    struct consensus_priv *priv;
    int cur_nxt_idx;
    u8 num_of_chunks, i;
    struct data_chunk *new_chunk;
    int err = 0;
    struct asguard_ringbuf_read_work_data *next_work = NULL;
    struct delayed_work *dw;

    dw = container_of(w, struct delayed_work, work);
    aw = container_of(dw, struct asguard_ringbuf_read_work_data, dwork);
    priv = aw->sdev->consensus_priv;

    if(!priv) {
        asguard_dbg("consensus priv is NULL \n");
        goto cleanup;
    }

    /* Get next free slot in ASGARD log */
    cur_nxt_idx = priv->sm_log.last_idx + 1;

    /* Make sure that this node is still the leader! */
    if(priv->nstate != LEADER) {
        goto cleanup;
    }

    /* Read until the buffer is empty or consensus stopped.
     * Exit on Error
     */
    while(1) {

        if(is_rb_empty(aw->rb)) {
            break;
        }

        /* Check header, read without read_idx increment
         *
         * Do we have enough data (considering the write idx)?
         *
         * Is the validation key correct in the header?
         */
        if(check_entry(aw->rb)) {

            break;
        }

        num_of_chunks = get_num_of_chunks((char*) &aw->rb->ring[aw->rb->read_idx].data);
        //print_hex_dump(KERN_DEBUG, "Header: ", DUMP_PREFIX_OFFSET, 64, 1,
        //               &aw->rb->ring[aw->rb->read_idx].data, sizeof(struct data_chunk), 0);

        /* +1 because we also transmit the header itself and the number of chunks */
        for (i = 0; i < num_of_chunks + 1; i++) {
            new_chunk = kmalloc(sizeof(struct data_chunk), GFP_KERNEL);
            /* Write from ringbuffer to ASGARD log slot */
            err = read_rb(aw->rb, new_chunk);
            if(err) {
                kfree(new_chunk);
                asguard_dbg("Failed to read from ring buffer\n");
                break;
            }
           // asguard_dbg("asgObj append %u of %u chunks\n", i, num_of_chunks);

            /*print_hex_dump(KERN_DEBUG, "after ringbuffer: ", DUMP_PREFIX_OFFSET, 64, 1,
                           new_chunk, sizeof(struct data_chunk), 0);*/

            err = append_command(priv, new_chunk, priv->term, cur_nxt_idx, 0);
            if(err) {
                asguard_error("Failed to append new chunk to ASGARD log\n");
                break;
            }
            cur_nxt_idx++;
        }

    }

    /* ASGARD can now start transmitting ..
     * TODO: we already know that there are pending log replications!
     *         .. are all the check ups triggered by check_pending_log_rep
     *         .. really necessary?
     *
     *
     * */
    check_pending_log_rep(priv->sdev);

cleanup:

    /* Schedule next Worker Item, but with a delay and only if the system is still running */
    if(aw->sdev->is_leader && aw->sdev->pminfo.state == ASGUARD_PM_EMITTING) {
        // freed by pull_consensus_requests_from_rb
        next_work = kmalloc(sizeof(struct asguard_ringbuf_read_work_data), GFP_KERNEL);

        if(!next_work) {
            asguard_error("Could not allocate memory for work item in %s\n", __FUNCTION__);
            return;
        }
        next_work->rb = (struct asg_ring_buf *) aw->rb;
        next_work->sdev = aw->sdev;
        INIT_DELAYED_WORK(&next_work->dwork, pull_consensus_requests_from_rb);

        /* Delay is in jiffies and depends on the configured HZ for the Linux Kernel.
         */
        schedule_delayed_work_on(17, &next_work->dwork, 5);
       // queue_delayed_work(aw->sdev->asguard_ringbuf_reader_wq, &next_work->dwork, 5);
    }

    kfree(aw);

}


void prepare_log_replication_handler(struct work_struct *w);
void prepare_log_replication_multicast_handler (struct asguard_device *sdev);


void _schedule_log_rep(struct asguard_device *sdev, int target_id, int next_index, s32 retrans, int multicast_enabled)
{
	struct asguard_leader_pkt_work_data *work = NULL;
    int more = 0;

	// if leadership has been dropped, do not schedule leader work
	if(sdev->is_leader == 0) {
		asguard_dbg("node is not a leader\n");
        return;
	}

	// if pacemaker has been stopped, do not schedule leader tx work
	if(sdev->pminfo.state != ASGUARD_PM_EMITTING) {
		asguard_dbg("pacemaker not in emitting state\n");
		return;
	}


    if (multicast_enabled > 0 ){
        do{
            more = _do_prepare_log_replication_multicast(sdev, sdev->multicast_ip, sdev->multicast_mac);
        }
        while(more > 0);
    }
    //prepare_log_replication_multicast_handler(sdev);
    else{
        // freed by prepare_log_replication_handler
        work = kmalloc(sizeof(struct asguard_leader_pkt_work_data), GFP_KERNEL);
        work->sdev = sdev;
        work->next_index = next_index;
        work->retrans = retrans;


        work->target_id = target_id;
        INIT_WORK(&work->work, prepare_log_replication_handler);

        if(!queue_work(sdev->asguard_leader_wq, &work->work)) {
            asguard_dbg("Work item not put in queue ..");
            sdev->bug_counter++;
            if(work)
                kfree(work); //right?
            return;
        }
    }
}
/*
 *
 */
void check_pending_log_rep_for_multicast(struct asguard_device *sdev)
{
    s32 next_index, match_index;
    int retrans;

    if(sdev->is_leader == 0)
        return;

    if(sdev->pminfo.state != ASGUARD_PM_EMITTING)
        return;

    next_index = sdev->multicast.nextIdx;

    if(next_index < 0)
        return;

    _schedule_log_rep(sdev, 0, next_index, retrans, 1);
}
EXPORT_SYMBOL(check_pending_log_rep_for_multicast);

void prepare_log_replication_multicast_handler(struct asguard_device *sdev)
{
    int more = 0;

    more = _do_prepare_log_replication_multicast(sdev, sdev->multicast_ip, sdev->multicast_mac);

    /* not ready to prepare log replication */
    if(more < 0)
        return;

    if(more)
        check_pending_log_rep_for_multicast(sdev);
}

void prepare_log_replication_handler(struct work_struct *w)
{
	struct asguard_leader_pkt_work_data *aw = NULL;
	int more = 0;

	aw = container_of(w, struct asguard_leader_pkt_work_data, work);

	more = _do_prepare_log_replication(aw->sdev, aw->target_id, aw->next_index, aw->retrans);

	/* not ready to prepare log replication */
	if(more < 0){
		goto cleanup;
	}

	if(!list_empty(&aw->sdev->consensus_priv->sm_log.retrans_head[aw->target_id]) || more) {
		check_pending_log_rep_for_target(aw->sdev, aw->target_id);
	}

cleanup:
    if(aw)
	    kfree(aw);
}

s32 get_next_idx_for_target(struct consensus_priv *cur_priv, int target_id, int *retrans)
{
	s32 next_index = -1;
	struct retrans_request *cur_rereq = NULL;

    rmb();

	write_lock(&cur_priv->sm_log.retrans_list_lock[target_id]);

	if(cur_priv->sm_log.retrans_entries[target_id] > 0) {

		cur_rereq = list_first_entry_or_null(
				&cur_priv->sm_log.retrans_head[target_id],
				struct retrans_request,
				retrans_req_head);

		if(!cur_rereq) {
			asguard_dbg("entry is null!");
			goto unlock;
		}

		cur_priv->sm_log.retrans_entries[target_id]--;

		next_index = cur_rereq->request_idx;
        list_del(&cur_rereq->retrans_req_head);
		kfree(cur_rereq);
		*retrans = 1;
	} else {
		next_index = _get_next_idx(cur_priv, target_id);
		*retrans = 0;
	}

unlock:
	write_unlock(&cur_priv->sm_log.retrans_list_lock[target_id]);

	return next_index;
}
/*
 *
 */
void check_pending_log_rep_for_target(struct asguard_device *sdev, int target_id)
{
	s32 next_index, match_index;
	int retrans;
	struct consensus_priv *cur_priv = NULL;
	int j;

	// TODO: utilise all protocol instances, currently only support for one consensus proto instance - the first
	for (j = 0; j < sdev->num_of_proto_instances; j++) {
		if (sdev->protos[target_id] != NULL && sdev->protos[j]->proto_type == ASGUARD_PROTO_CONSENSUS) {
			// get corresponding local instance data for consensus
			cur_priv =
				(struct consensus_priv *)sdev->protos[j]->proto_data;
			break;
		}
	}

	if(cur_priv == NULL){
		asguard_error("BUG. Invalid local proto data");
		return;
	}

	if(sdev->is_leader == 0)
		return;

	if(sdev->pminfo.state != ASGUARD_PM_EMITTING)
		return;

	next_index = get_next_idx_for_target(cur_priv, target_id, &retrans);

	if(next_index < 0)
	    return;

	match_index = _get_match_idx(cur_priv, target_id);

	if(next_index == match_index) {
		asguard_dbg("nothing to send for target %d\n", target_id);
		return;
	}

	_schedule_log_rep(sdev, target_id, next_index, retrans, 0);
}
EXPORT_SYMBOL(check_pending_log_rep_for_target);




void check_pending_log_rep(struct asguard_device *sdev)
{
	int i;

	if(sdev->is_leader == 0)
		return;

	if(sdev->pminfo.state != ASGUARD_PM_EMITTING)
		return;

    if (sdev->multicast.enable)
        check_pending_log_rep_for_multicast(sdev);
    else{
        for(i = 0; i < sdev->pminfo.num_of_targets; i++)
            check_pending_log_rep_for_target(sdev, i);
    }


}
EXPORT_SYMBOL(check_pending_log_rep);
