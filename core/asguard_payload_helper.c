
#include <linux/slab.h>


#include <asguard/consensus.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>
#include <asguard/logger.h>

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

reuse:

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

s32 _get_next_idx(struct consensus_priv *priv, int target_id)
{
	s32 next_index;

	if (_check_target_id(priv, target_id))
		next_index = priv->sm_log.next_index[target_id];
	else
		next_index = 0;

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

s32 _get_prev_log_term(struct consensus_priv *cur_priv, s32 idx)
{

	if (idx == -1) {
		// Logs are empty, because next index points to first element.
		// Thus, there was no prev log term. And therefore we can use the
		// current term.
		return cur_priv->term;
	}

	if (idx < -1) {
		asguard_dbg("invalid value - idx=%d\n", idx);
		return -1;
	}

	if (idx > cur_priv->sm_log.last_idx) {
		asguard_dbg("idx > cur_priv->sm_log.last_id %d\n", idx);

		return -1;
	}

	if (idx > MAX_CONSENSUS_LOG) {
		asguard_dbg("BUG! idx > MAX_CONSENSUS_LOG. %d\n", idx);
		return -1;
	}

	if (!cur_priv->sm_log.entries[idx]) {
		asguard_dbg("BUG! entries is null at index %d\n", idx);
		return -1;
	}

	return cur_priv->sm_log.entries[idx]->term;
}
int setup_alive_msg(struct consensus_priv *cur_priv, struct asguard_payload *spay, int instance_id)
{
	char *pkt_payload_sub;

	pkt_payload_sub = asguard_reserve_proto(instance_id, spay, ASGUARD_PROTO_CON_PAYLOAD_SZ);

	set_le_opcode((unsigned char *)pkt_payload_sub, ALIVE, cur_priv->term, cur_priv->sm_log.commit_idx, 0, 0);

	return 0;
}
EXPORT_SYMBOL(setup_alive_msg);

int setup_append_msg(struct consensus_priv *cur_priv, struct asguard_payload *spay, int instance_id, int target_id, int hb_passive_ix)
{
	s32 next_index, local_last_idx;
	s32 prev_log_term, leader_commit_idx;
	s32 num_entries = 0;
	char *pkt_payload_sub;
	int more = 0;
	int retrans = 0;
	struct retrans_request *cur_rereq;

	write_lock(&cur_priv->sm_log.retrans_list_lock[target_id]);

	cur_rereq = list_first_entry_or_null(
					&cur_priv->sm_log.retrans_head[target_id],
					struct retrans_request,
					retrans_req_head);

	if (cur_rereq != NULL) {
		next_index = cur_rereq->request_idx;

		list_del(&cur_rereq->retrans_req_head);

		kfree(cur_rereq);
		retrans = 1;
	} else {
		next_index = _get_next_idx(cur_priv, target_id);
	}

	write_unlock(&cur_priv->sm_log.retrans_list_lock[target_id]);

	// Check if entries must be appended
	local_last_idx = _get_last_idx_safe(cur_priv);

	if (next_index == -1) {
		asguard_dbg("Invalid target id resulted in invalid next_index!\n");
		return more;
	}

	// asguard_dbg("PREP AE: local_last_idx=%d, next_index=%d\n", local_last_idx, next_index);
	prev_log_term = _get_prev_log_term(cur_priv, next_index - 1);

	if (prev_log_term < 0) {
		asguard_error("BUG! - prev_log_term is invalid\n");
		return more;
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
		}

		// update next_index without receiving the response from the target
		// .. If the receiver rejects this append command, this node will set the
		// .. the next_index to the last known safe index of the receivers log.
		// .. In this implementation the receiver sends the last known safe index
		// .. with the append reply.
		if(!retrans)
			cur_priv->sm_log.next_index[target_id] += num_entries;

		// asguard_dbg("retrans=%d, target_id=%d, leader_last_idx=%d, next_idx=%d, prev_log_term=%d, num_entries=%d\n",
		// 			retrans, target_id, local_last_idx, next_index, prev_log_term, num_entries);
	}

	// reserve space in asguard heartbeat for consensus LEAD
	pkt_payload_sub =
		asguard_reserve_proto(instance_id, spay,
						ASGUARD_PROTO_CON_AE_BASE_SZ + (num_entries * AE_ENTRY_SIZE));

	if (!pkt_payload_sub)
		return 0;

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

	// wait until pkt has been emited
	cur_priv->sdev->pminfo.pm_targets[target_id].pkt_data.contains_log_rep[hb_passive_ix] = 1;
	// fire as soon as possible
	cur_priv->sdev->fire = 1;
	// fire only for the target with targetid
	cur_priv->sdev->pminfo.pm_targets[target_id].fire = 1;

	return more;
}
EXPORT_SYMBOL(setup_append_msg);

/* Must be called after the asguard packet has been emitted.
 */
void invalidate_proto_data(struct asguard_device *sdev, struct asguard_payload *spay, int target_id)
{

	// free previous piggybacked protocols
	spay->protocols_included = 0;

}
EXPORT_SYMBOL(invalidate_proto_data);


int _do_prepare_log_replication(struct asguard_device *sdev, int target_id)
{
	struct consensus_priv *cur_priv = NULL;
	int j;
	struct pminfo *spminfo = &sdev->pminfo;
	int hb_passive_ix;
	struct asguard_payload *pkt_payload;
	int more = 0;

	if(sdev->is_leader == 0)
		return -1; // node lost leadership

	if(spminfo->pm_targets[target_id].pkt_data.active_dirty){
		return -1; // previous pkt has not been emitted yet, thus we can not switch buffer at the end of this function
	}

	spminfo->pm_targets[target_id].pkt_data.active_dirty = 1;

	if(spminfo->pm_targets[target_id].alive == 0) {
		return -1; // Current target is not alive!
	}

	hb_passive_ix =
			!!!spminfo->pm_targets[target_id].pkt_data.hb_active_ix;

	pkt_payload =
			spminfo->pm_targets[target_id].pkt_data.pkt_payload[hb_passive_ix];

	// iterate through consensus protocols and include LEAD messages if node is leader
	for (j = 0; j < sdev->num_of_proto_instances; j++) {

		if (sdev->protos[target_id] != NULL && sdev->protos[j]->proto_type == ASGUARD_PROTO_CONSENSUS) {

			// get corresponding local instance data for consensus
			cur_priv =
				(struct consensus_priv *)sdev->protos[j]->proto_data;

			if (cur_priv->nstate != LEADER)
				continue;

			// TODO: optimize append calls that do not contain any log updates
			more += setup_append_msg(cur_priv, pkt_payload, sdev->protos[j]->instance_id, target_id, hb_passive_ix);

			sdev->pminfo.pm_targets[target_id].scheduled_log_replications++;


		}
	}

	spminfo->pm_targets[target_id].pkt_data.hb_active_ix = hb_passive_ix;

	return more;

}

void prepare_log_replication_handler(struct work_struct *w);

void _schedule_log_rep(struct asguard_device *sdev, int target_id)
{
	struct asguard_leader_pkt_work_data *work = NULL;

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

	work = kmalloc(sizeof(struct asguard_leader_pkt_work_data), GFP_ATOMIC);
	work->sdev = sdev;
	work->target_id = target_id;

	INIT_WORK(&work->work, prepare_log_replication_handler);
	if(!queue_work(sdev->asguard_leader_wq, &work->work)) {
		asguard_dbg("Work item not put in queue ..");
	}

}


void prepare_log_replication_handler(struct work_struct *w)
{
	struct asguard_leader_pkt_work_data *aw = NULL;
	int more = 0;


	// wait until the previous packet has been sent.
	// ... do not wait for reply from target

	aw = (struct asguard_leader_pkt_work_data *) container_of(w, struct asguard_leader_pkt_work_data, work);

	mutex_lock(&aw->sdev->pminfo.pm_targets[aw->target_id].pkt_data.active_dirty_lock);

	more = _do_prepare_log_replication(aw->sdev, aw->target_id);

	aw->sdev->pminfo.pm_targets[aw->target_id].pkt_data.updating = 0;

	/* not ready to prepare log replication */
	if(more < 0)
		goto cleanup;

	if(!list_empty(&aw->sdev->consensus_priv->sm_log.retrans_head[aw->target_id]) || more) {
		prepare_log_replication_for_target(aw->sdev, aw->target_id);
		asguard_dbg("schedule more\n");
	} else
		asguard_dbg("nothing to schedule\n");


cleanup:
	kfree(aw);
}

void prepare_log_replication_for_target(struct asguard_device *sdev, int target_id)
{

	if(sdev->is_leader == 0)
		return;

	if(sdev->pminfo.state != ASGUARD_PM_EMITTING)
		return;

	if (sdev->pminfo.pm_targets[target_id].pkt_data.updating) {
		//asguard_dbg("Work not scheduled: Only schedule one log rep work item per target in parallel \n");
		return;
	}

	sdev->pminfo.pm_targets[target_id].pkt_data.updating = 1;

	_schedule_log_rep(sdev, target_id);

}
EXPORT_SYMBOL(prepare_log_replication_for_target);

void prepare_log_replication(struct asguard_device *sdev)
{
	int i;

	for(i = 0; i < sdev->pminfo.num_of_targets; i++)
		prepare_log_replication_for_target(sdev, i);

}
EXPORT_SYMBOL(prepare_log_replication);