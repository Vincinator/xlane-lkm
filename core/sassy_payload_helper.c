#include <sassy/sassy.h>
#include <sassy/payload_helper.h>
#include <sassy/consensus.h>



/* Returns a pointer to the <n>th protocol of <spay>
 * 
 * If less than n protocols are included, a NULL ptr is returned
 */
char *sassy_get_proto(struct sassy_payload *spay, int n) 
{
	int i;
	char *cur_proto;
	int cur_offset = 0;

	cur_proto = spay->proto_data;

	if(spay->protocols_included < n) {
		sassy_error("only %d protocols are included, requested %d",spay->protocols_included, n);
		return NULL;
	}

	// Iterate through existing protocols
	for (i = 0; i < n; i++) {
		cur_offset = GET_PROTO_OFFSET_VAL(cur_proto);
		cur_proto = cur_proto + cur_offset;
	}

	return cur_proto;
}
EXPORT_SYMBOL(sassy_get_proto);


/* Protocol offsets and protocols_included must be correct before calling this method. 
 *
 * Sets protocol id and reserves space in the sassy payload,
 * if the required space is available.
 * 
 * returns a pointer to the start of that protocol payload memory area.
 */
char *sassy_reserve_proto(u16 instance_id, struct sassy_payload *spay, u16 proto_size)
{
	int i;
	char *cur_proto;
	int proto_offset = 0;
	int cur_offset = 0;

	u16 *pid, *poff;

	cur_proto = spay->proto_data;

	// Iterate through existing protocols
	for (i = 0; i < spay->protocols_included; i++) {
		if (instance_id == GET_PROTO_TYPE_VAL(cur_proto))
			break;
		cur_offset = GET_PROTO_OFFSET_VAL(cur_proto);
		cur_proto = cur_proto + cur_offset;

		proto_offset += cur_offset;
	}

	if (unlikely(proto_offset + proto_size > MAX_SASSY_PAYLOAD_BYTES)) {
		sassy_error("Not enough space in sassy payload for protocol\n");
		return NULL;
	}

	pid =  GET_PROTO_TYPE_PTR(cur_proto);
	poff = GET_PROTO_OFFSET_PTR(cur_proto);

	*pid = instance_id;
	*poff = proto_size;

	//sassy_dbg("pid: %hu, poff: %hu", *pid, *poff);

	spay->protocols_included++;

	return cur_proto;
}
EXPORT_SYMBOL(sassy_reserve_proto);


int _check_target_id(struct consensus_priv *priv, int target_id)
{

	if(target_id < 0)
		goto error;

	if(target_id > MAX_NODE_ID)
		goto error;

	return 1;
error:
	sassy_dbg("Target ID is not valid: %d\n", target_id);
	return 0;
}

s32 _get_match_idx(struct consensus_priv *priv, int target_id)
{
	s32 match_index;

	if(_check_target_id(priv, target_id))
		match_index = priv->sm_log.match_index[target_id];
	else
		match_index = -1;

	return match_index;
}

s32 _get_next_idx(struct consensus_priv *priv, int target_id)
{
	s32 next_index;

	if(_check_target_id(priv, target_id))
		next_index = priv->sm_log.next_index[target_id];
	else
		next_index = -1;

	return next_index;
}

s32 _get_last_idx_safe(struct consensus_priv *priv)
{
	if(priv->sm_log.last_idx < -1)
		priv->sm_log.last_idx = -1; // repair last index!

	return priv->sm_log.last_idx;
}

int _log_is_faulty(struct consensus_priv *priv) 
{

	if(!priv)
		return 1;

	if(!priv->sm_log.entries)
		return 1;

	//  if required add more consistency checks here..

	return 0;
}

s32 _get_prev_log_term(struct consensus_priv *cur_priv, s32 idx)
{

	if(idx < 0)
		return -1;

	if(idx > cur_priv->sm_log.last_idx)
		return -1;

	if(idx > MAX_CONSENSUS_LOG){
		sassy_dbg("BUG! idx > MAX_CONSENSUS_LOG. %d\n", idx);
		return -1;
	}

	if(!cur_priv->sm_log.entries[idx]){
		sassy_dbg("BUG! entries is null at index %d\n", idx);
		return -1;
	}

	return cur_priv->sm_log.entries[idx]->term;
}


void setup_append_msg(struct consensus_priv *cur_priv, struct sassy_payload *spay, int instance_id, int target_id)
{
	s32 num_entries, match_index, next_index, cur_index;
	s32 prev_log_idx, prev_log_term, leader_commit_idx;
	int num_of_entries = 0;
	char *pkt_payload_sub;

	// if(unlikely(_log_is_faulty(cur_priv))) {
	// 	sassy_dbg("Log is faulty or not initialized.\n");
	// 	return;
	// }

	// Check if entries must be appended
	cur_index = _get_last_idx_safe(cur_priv);
	next_index = _get_next_idx(cur_priv, target_id); 

	// if(next_index == -1){
	// 	sassy_dbg("Invalid target id resulted in invalid next_index!\n");
	// 	return;
	// }
	//sassy_dbg("cur_index=%d, next_index=%d\n", cur_index, next_index);
	prev_log_term = _get_prev_log_term(cur_priv, next_index == -1 ? -1 : next_index - 1 );

	leader_commit_idx = cur_priv->sm_log.commit_idx;

	if(cur_index >= next_index){
		sassy_dbg("cur_index=%d, next_index=%d, match_index=%d, prev_log_term=%d\n", 
			cur_index, next_index, match_index, prev_log_term);
			
		// Facts:
		//	- cur_index >= next_index
		//  - Must include entries in next consensus append message
		//  - thus, num_of_entries will not be 0

		// Decide how many entries to update for the current target
		num_entries = (MAX_ENTRIES_PER_PKT < next_index - cur_index) ? 
				MAX_ENTRIES_PER_PKT : next_index - cur_index ;

		// update next_index without receiving the response from the target
		// .. If the receiver rejects this append command, this node will set the 
		// .. the next_index to the last known safe index of the receivers log.
		// .. In this implementation the receiver sends the last known safe index
		// .. with the append reply.
		cur_priv->sm_log.next_index[target_id] += num_entries + 1;
	}

	// reserve space in sassy heartbeat for consensus LEAD
	pkt_payload_sub =
		sassy_reserve_proto(instance_id, spay,
						SASSY_PROTO_CON_AE_BASE_SZ + (num_of_entries * AE_ENTRY_SIZE));

	set_ae_data(pkt_payload_sub, 
			cur_priv->term, 
 	 		cur_priv->node_id,
 	 		// previous is one before the "this should be send next" index
	 		next_index == -1 ? -1 : next_index - 1 ,
	 		prev_log_term,
	 		leader_commit_idx,
	 		cur_priv, 
	 		num_of_entries);
}
EXPORT_SYMBOL(setup_append_msg);

/* Must be called after the sassy packet has been emitted. 
 */
void invalidate_proto_data(struct sassy_device *sdev, struct sassy_payload *spay, int target_id)
{
	struct consensus_priv *cur_priv;	
	int i;

	// free previous piggybacked protocols
	spay->protocols_included = 0;
	
	// iterate through consensus protocols and include LEAD messages if node is leader
	for(i = 0; i < sdev->num_of_proto_instances; i++){
		
		if(sdev->protos[i] != NULL && sdev->protos[i]->proto_type == SASSY_PROTO_CONSENSUS){
	 		
	 		// get corresponding local instance data for consensus
			cur_priv = 
				(struct consensus_priv *)sdev->protos[i]->proto_data;
	 		
	 		if(cur_priv->nstate != LEADER)
	 			continue;

	 		// TODO: optimize append calls that do not contain any log updates
	 		setup_append_msg(cur_priv, spay, sdev->protos[i]->instance_id, target_id);
	 		

		}
	}
}
EXPORT_SYMBOL(invalidate_proto_data);