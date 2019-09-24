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

	if (proto_offset + proto_size > MAX_SASSY_PAYLOAD_BYTES) {
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


/* Must be called after the sassy packet has been emitted. 
 */
void invalidate_proto_data(struct sassy_device *sdev, struct sassy_payload *spay, int target_id)
{
	u16 *opcode;
	u32 *param1, *param2;
	char *pkt_payload_sub;
	struct consensus_priv *cur_priv;
	int i;
	u32 num_entries, next_index, cur_index, prev_log_idx, prev_log_term, leader_commit_idx;
	struct pminfo *spminfo = &sdev->pminfo;
	struct sm_command *cmd_array;
	int num_of_entries = 0;

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



			// set opcode to LEAD
			opcode = GET_CON_PROTO_OPCODE_PTR(pkt_payload_sub);
			*opcode = (u16) APPEND;



	 		// Check if entries must be appendedS
	 		cur_index = cur_priv->sm_log.last_idx;
	 		next_index = cur_priv->sm_log.next_index[target_id]; 
	 		match_index = cur_priv->sm_log.match_index[target_id]; 
	 		prev_log_term = cur_priv->sm_log.entries[match_index]->term;
	 		leader_commit_idx = cur_priv->sm_log.commit_idx;

	 		// only append entries if the leader has something fresh to append
	 		if(cur_index > next_index) {

		 		// Decide how many entries to update for the current target
			

	 		}

	 		// reserve space in sassy heartbeat for consensus LEAD
	 		pkt_payload_sub =
	 				sassy_reserve_proto(sdev->protos[i]->instance_id, spay, SASSY_PROTO_CON_AE_BASE_SZ + (num_of_entries * AE_ENTRY_SIZE);
		

	 		set_ae_data(pkt_payload_sub, 
						cur_priv->term, 
			 	 		cur_priv->node_id,
			 	 		cur_priv->sm_log.
				 		match_index,
				 		prev_log_term,
				 		leader_commit_idx,
				 		cmd_array, 
				 		num_of_entries)
		}
	}
}
EXPORT_SYMBOL(invalidate_proto_data);


handle_payload_fun get_payload_handler(enum sassy_protocol_type ptype) 
{
	switch (ptype) 
	{
		case SASSY_PROTO_ECHO:
		
			break;
		case SASSY_PROTO_FD:

			break;
		case SASSY_PROTO_CONSENSUS:

			break;
		default:
			sassy_error("unknwon protocol. \n");
	}

	return NULL;
}
