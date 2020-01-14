
#include <linux/slab.h>


#include <asguard/consensus.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>
#include <asguard/logger.h>




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

	// Check if protocol instance already existis in payload
	for (i = 0; i < spay->protocols_included; i++) {

		if (instance_id == GET_PROTO_TYPE_VAL(cur_proto))
			goto reuse; // reuse existing payload part for this instance id

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

int setup_append_msg(struct consensus_priv *cur_priv, struct asguard_payload *spay, int instance_id, int target_id)
{
	s32 next_index, cur_index;
	s32 prev_log_term, leader_commit_idx;
	s32 num_entries = 0;
	char *pkt_payload_sub;
	int more = 0;

	// if (unlikely(_log_is_faulty(cur_priv))) {
	//	asguard_dbg("Log is faulty or not initialized.\n");
	//	return;
	// }

	// Check if entries must be appended
	cur_index = _get_last_idx_safe(cur_priv);
	next_index = _get_next_idx(cur_priv, target_id);

	if (next_index == -1) {
		asguard_dbg("Invalid target id resulted in invalid next_index!\n");
		return more;
	}

	// asguard_dbg("PREP AE: cur_index=%d, next_index=%d\n", cur_index, next_index);
	prev_log_term = _get_prev_log_term(cur_priv, next_index - 1);

	if (prev_log_term < 0) {
		asguard_error("BUG! - prev_log_term is invalid\n");
		return more;
	}

	leader_commit_idx = cur_priv->sm_log.commit_idx;

	if (cur_index >= next_index) {
		// Facts:
		//	- cur_index >= next_index
		//  - Must include entries in next consensus append message
		//  - thus, num_of_entries will not be 0

		// Decide how many entries to update for the current target

		if (cur_priv->max_entries_per_pkt < cur_index - next_index + 1) {
			num_entries = cur_priv->max_entries_per_pkt;
			more = 1;
		} else {
			num_entries = (cur_index - next_index + 1);
		}

		// update next_index without receiving the response from the target
		// .. If the receiver rejects this append command, this node will set the
		// .. the next_index to the last known safe index of the receivers log.
		// .. In this implementation the receiver sends the last known safe index
		// .. with the append reply.
		cur_priv->sm_log.next_index[target_id] += num_entries;

		asguard_dbg("cur_index=%d, next_index=%d, prev_log_term=%d, num_entries=%d\n",
					cur_index, next_index, prev_log_term, num_entries);
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


int _do_prepare_log_replication(struct asguard_device *sdev)
{
	struct consensus_priv *cur_priv = NULL;
	int i, j;
	struct pminfo *spminfo = &sdev->pminfo;
	int hb_passive_ix;
	struct asguard_payload *pkt_payload;
	int more = 0;
	asguard_dbg("do log rep \n");

	for (i = 0; i < spminfo->num_of_targets; i++) {

		hb_passive_ix =
		     !!!spminfo->pm_targets[i].pkt_data.hb_active_ix;

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.pkt_payload[hb_passive_ix];

		if(spminfo->pm_targets[i].pkt_data.active_dirty){
			asguard_dbg("DEBUG: prev pkt has not been emitted yet.\n");
			continue; // previous pkt has not been emitted yet
		}

		// iterate through consensus protocols and include LEAD messages if node is leader
		for (j = 0; j < sdev->num_of_proto_instances; j++) {

			if (sdev->protos[i] != NULL && sdev->protos[j]->proto_type == ASGUARD_PROTO_CONSENSUS) {

				// get corresponding local instance data for consensus
				cur_priv =
					(struct consensus_priv *)sdev->protos[j]->proto_data;

				if (cur_priv->nstate != LEADER)
					continue;

				// TODO: optimize append calls that do not contain any log updates
				more += setup_append_msg(cur_priv, pkt_payload, sdev->protos[j]->instance_id, i);

			}
		}
		spminfo->pm_targets[i].pkt_data.active_dirty = 1;
		spminfo->pm_targets[i].pkt_data.hb_active_ix = hb_passive_ix;

	}

	return more;

}

void _schedule_log_rep(struct asguard_device *sdev)
{
	struct asguard_leader_pkt_work_data *work = NULL;

	asguard_dbg("schedule log rep \n");

	sdev->block_leader_wq = 1;

	work = kmalloc(sizeof(struct asguard_leader_pkt_work_data), GFP_ATOMIC);
	work->sdev = sdev;

	INIT_WORK(&work->work, prepare_log_replication_handler);
	if(!queue_work(sdev->asguard_leader_wq, &work->work)) {
		asguard_dbg("Work item not put in query..");
	}


}
void prepare_log_replication_handler(struct work_struct *w)
{
	struct asguard_leader_pkt_work_data *aw = NULL;
	int more = 0;

	asguard_dbg("schedule log rep handler\n");

	aw = (struct asguard_leader_pkt_work_data *) container_of(w, struct asguard_leader_pkt_work_data, work);

	more = _do_prepare_log_replication(aw->sdev);

	// Check if new work must be scheduled:
	if(more){
		// do not open block, direct call to schedule
		_schedule_log_rep(aw->sdev);
	} else {
		aw->sdev->block_leader_wq = 0;
	}
	kfree(aw);
}


void prepare_log_replication(struct asguard_device *sdev)
{
	asguard_dbg("prepare_log_rep \n");

	/* Do nothing if work is already queued.
	 * If a work item is finished, and work is to do, the work item itself schedules
	 * the next work item.
	 */
	if(sdev->block_leader_wq)
		return;

	_schedule_log_rep(sdev);
}
EXPORT_SYMBOL(prepare_log_replication);