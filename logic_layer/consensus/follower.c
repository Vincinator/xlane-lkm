#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/timer.h>

#include <sassy/payload_helper.h>
#include <sassy/consensus.h>
#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/follower.h"
#include "include/consensus_helper.h"


#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][FOLLOWER]"


static enum hrtimer_restart _handle_follower_timeout(struct hrtimer *timer)
{
	int err;
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ftimer);
	// struct sassy_device *sdev = priv->sdev;

	if(priv->ftimer_init == 0 || priv->nstate != FOLLOWER)
		return HRTIMER_NORESTART;

	write_log(&priv->ins->logger, FOLLOWER_TIMEOUT, rdtsc());

#if 0

	sassy_log_le("%s, %llu, %d: Follower timer timed out\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term);
#endif

	err = node_transition(priv->ins, CANDIDATE);

	write_log(&priv->ins->logger, FOLLOWER_BECOME_CANDIDATE, rdtsc());

	if (err){
		sassy_dbg("Error occured during the transition to candidate role\n");
		return HRTIMER_NORESTART;
	}

	return HRTIMER_NORESTART;
}

void reply_append(struct proto_instance *ins,  struct pminfo *spminfo, int remote_lid, int rcluster_id, int param1, int append_success, u32 logged_idx)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
	struct sassy_payload *pkt_payload;
	char *pkt_payload_sub;
	int hb_passive_ix;

#if 0
	sassy_log_le("%s, %llu, %d: REPLY APPEND append_success=%d, param1=%d,logged_idx=%d \n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			append_success,
			param1,
			logged_idx);
#endif

	hb_passive_ix =
	     !!!spminfo->pm_targets[remote_lid].pkt_data.hb_active_ix;

	pkt_payload =
     	spminfo->pm_targets[remote_lid].pkt_data.pkt_payload[hb_passive_ix];

	pkt_payload_sub = 
 		sassy_reserve_proto(ins->instance_id, pkt_payload, SASSY_PROTO_CON_PAYLOAD_SZ);
	

 	if(!pkt_payload_sub) {
 		sassy_error("Sassy packet full! This error is not handled - not implemented\n");
 		return -1;
 	}

	set_le_opcode((unsigned char*)pkt_payload_sub, APPEND_REPLY, param1, append_success, logged_idx, 0);
	
	spminfo->pm_targets[remote_lid].pkt_data.hb_active_ix = hb_passive_ix;

	if(append_success)
		write_log(&ins->logger, REPLY_APPEND_SUCCESS, rdtsc());
	else
		write_log(&ins->logger, REPLY_APPEND_FAIL, rdtsc());

}
void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, s32 param1, s32 param2)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
#if 0

	sassy_log_le("%s, %llu, %d: voting for cluster node %d with term %d\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			rcluster_id,
			param1);
#endif

	setup_le_msg(ins, &priv->sdev->pminfo, VOTE, remote_lid, param1, param2, 0, 0);
	priv->voted = param1;
	priv->sdev->fire = 1;

	write_log(&ins->logger, VOTE_FOR_CANDIDATE, rdtsc());

}

int append_commands(struct consensus_priv *priv, unsigned char *pkt, int num_entries, int pkt_size)
{
	int i, err, new_last;
	u32 *cur_ptr;
	struct sm_command *cur_cmd;
	struct state_machine_cmd_log *log = &priv->sm_log;


	new_last = log->last_idx + num_entries;

	if(new_last >= MAX_CONSENSUS_LOG) {
		sassy_dbg("Local log is full!\n");
		err = -ENOMEM;
		goto error;
	}

	// check if entries would exceed pkt
	if((num_entries * AE_ENTRY_SIZE + SASSY_PROTO_CON_AE_BASE_SZ) > pkt_size) {
		err = -EINVAL;
		sassy_dbg("Claimed num of log entries would exceed packet size!\n");
		goto error;
	}

	cur_ptr = GET_CON_PROTO_ENTRIES_START_PTR(pkt);

	for(i = log->last_idx + 1; i <= new_last; i++){
 		
		cur_cmd = kmalloc(sizeof(struct sm_command), GFP_KERNEL);

		if(!cur_cmd){
			err = -ENOMEM;
			sassy_dbg("Out of Memory\n");
			goto error;
		}

		cur_cmd->sm_logvar_id = *cur_ptr;
		cur_ptr++;
		cur_cmd->sm_logvar_value = *cur_ptr;
		cur_ptr++;
		
		err = append_command(&priv->sm_log, cur_cmd, priv->term);

		if(err){
			// kfree(cur_cmd); // free memory of failed log entry.. others from this loop (?)
			goto error;
		}
	}


	return 0;

error:
	sassy_error("Failed to append commands to log\n");
	return err;
}

void remove_from_log_until_last(struct state_machine_cmd_log *log, int start_idx) 
{
	int i;

	if(log->last_idx < 0 ){
		sassy_dbg("Log already empty.");
		return;
	}

	if(start_idx > log->last_idx ){
		sassy_dbg("No Items at index of %d", start_idx);
		return;
	}

	for(i = start_idx; i <= log->last_idx; i++) {
		kfree(log->entries[i]);
	}

	log->last_idx = start_idx - 1;


}


u32 _check_prev_log_match(struct state_machine_cmd_log *log, u32 prev_log_term, s32 prev_log_idx) 
{
	struct sm_log_entry *entry;

	if(prev_log_idx == -1){
		sassy_dbg("prev log idx is -1\n");
		return 0;
	}

	if(prev_log_idx < -1){
		sassy_dbg("prev log idx is invalid! %d\n", prev_log_idx);
		return 1;
	}

	if(prev_log_idx > log->last_idx ){
		sassy_dbg("Entry at index %d does not exist\n", prev_log_idx);
		return 1;
	}
	
	entry = log->entries[prev_log_idx];

	if(entry == NULL) {
		sassy_dbg("BUG! Entry is NULL at index %d", prev_log_idx);
		return 1;
	}

	if(entry->term != prev_log_term) {
		sassy_dbg("prev_log_term does not match %d != %d", entry->term, prev_log_term);
		
		// Delete entries from prev_log_idx to last_idx
		remove_from_log_until_last(log, prev_log_idx);
		return 1;
	}

	return 0; 
}


int _check_append_rpc(u16 pkt_size, u32 prev_log_term, s32 prev_log_idx, int max_entries_per_pkt)
{

	if(prev_log_idx > MAX_CONSENSUS_LOG)
		return 1;

	if(pkt_size < 0 || pkt_size > SASSY_PROTO_CON_AE_BASE_SZ + (max_entries_per_pkt * AE_ENTRY_SIZE))
		return 1;

	return 0;
}

void _handle_append_rpc(struct proto_instance *ins, struct consensus_priv *priv, unsigned char *pkt,  int remote_lid, int rcluster_id)
{
	u32 *prev_log_term, num_entries;
	s32 *prev_log_idx, *leader_commit_idx;
	int append_success;
	u16 pkt_size;
	u32 check;

	num_entries = GET_CON_AE_NUM_ENTRIES_VAL(pkt);

	if(num_entries == 0){
		// no reply if nothing to append!
		return;
	}

	pkt_size = GET_PROTO_OFFSET_VAL(pkt);
	prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
	prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);

	if(priv->sm_log.lock)
		goto reply_false;

	priv->sm_log.lock = 1;

	if(_check_append_rpc(pkt_size, *prev_log_term, *prev_log_idx, priv->max_entries_per_pkt)){
		sassy_dbg("invalid data: pkt_size=%hu, prev_log_term=%d, prev_log_idx=%d\n",
				  pkt_size, *prev_log_term, *prev_log_idx);
		goto reply_false_unlock;
	}

	if(_check_prev_log_match(&priv->sm_log, *prev_log_term, *prev_log_idx)){
		sassy_dbg("Log inconsitency detected. prev_log_term=%d, prev_log_idx=%d, priv->sm_log.last_idx=%d\n", 
				  *prev_log_term, *prev_log_idx, priv->sm_log.last_idx);
		goto reply_false_unlock;
	}

	if(num_entries < 0 || num_entries > priv->max_entries_per_pkt){
		sassy_dbg("invalid num_entries=%d\n", num_entries);
		goto reply_false_unlock;
	}

	// append entries and if it fails, reply false
	if(append_commands(priv, pkt, num_entries, pkt_size)){
		sassy_dbg("append commands failed\n");
		goto reply_false_unlock;
	}

	// check commit index
	leader_commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);

	// check if leader_commit_idx is out of bounds
	if(*leader_commit_idx < -1 || *leader_commit_idx > MAX_CONSENSUS_LOG) {
		sassy_dbg("Out of bounds: leader_commit_idx=%d", *leader_commit_idx);
		goto reply_false_unlock;
	}

	// check if leader_commit_idx points to a valid entry
	if(*leader_commit_idx > priv->sm_log.last_idx){
//		sassy_dbg("Not referencing a valid log entry: leader_commit_idx=%d", *leader_commit_idx);
		goto reply_false_unlock;
	}

	// Check if commit index must be updated
	if(*leader_commit_idx > priv->sm_log.commit_idx) {
		// min(leader_commit_idx, last_idx)
		// note: last_idx of local log can not be null if append_commands was successfully executed
		priv->sm_log.commit_idx = *leader_commit_idx > priv->sm_log.last_idx ? priv->sm_log.last_idx : *leader_commit_idx;
		commit_log(priv);
	}

	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 1, priv->sm_log.last_idx);
	priv->sm_log.lock = 0;

	return;

reply_false_unlock:
	priv->sm_log.lock = 0;
reply_false:
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 0, priv->sm_log.last_idx);

}


int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
	struct sassy_device *sdev = priv->sdev;
	
	u8 opcode = GET_CON_PROTO_OPCODE_VAL(pkt);
	s32 param1, param2, param3, param4;


#if 1
	log_le_rx(sdev->verbose, priv->nstate, rdtsc(), priv->term, opcode, rcluster_id, param1);
#endif

	switch(opcode){
		// param1 interpreted as term 
		// param2 interpreted as candidateID
		// param3 interpreted as lastLogIndex of Candidate
		// param4 interpreted as lastLogTerm of Candidate
	case VOTE:
		break;
	case NOMI:	
	 	param1 = GET_CON_PROTO_PARAM1_VAL(pkt);
		param2 = GET_CON_PROTO_PARAM2_VAL(pkt);
		param3 = GET_CON_PROTO_PARAM3_VAL(pkt);
		param4 = GET_CON_PROTO_PARAM4_VAL(pkt);
	 	if(check_handle_nomination(priv, param1, param2, param3, param4)){
		 	reply_vote(ins, remote_lid, rcluster_id, param1, param2);
			reset_ftimeout(ins);
		 }
		break;	
	case NOOP:
		break;
	case APPEND:
	 	param1 = GET_CON_PROTO_PARAM1_VAL(pkt);
		/* Received a LEAD operation from a node with a higher term, 
		 * thus this node is accepting the node as new leader.
		 */
		if(param1 > priv->term){

#if 0
			if(sdev->verbose >= 2)
				sassy_dbg("Received message from new leader with higher term=%u local term=%u\n", param1, priv->term);
#endif

			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, rdtsc());
			reset_ftimeout(ins);

			_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

		} 

		/* Received a LEAD operation from a node with the same term,
		 * thus, this node has to check whether it is already following 
		 * that node (continue to follow), or if it is a LEAD operation 
		 * from a node that is currently not the follower (Ignore and let 
		 * the timeout continue to go down).
		 */
		else if(param1 == priv->term) {

			if(priv->leader_id == remote_lid){
#if 0
				if(sdev->verbose >= 2)
					sassy_dbg("Received message from known leader term=%u\n", param1);
#endif

				reset_ftimeout(ins);
				_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

			}else {
#if 0
				if(sdev->verbose >= 2)
					sassy_dbg("Received message from new leader term=%u\n", param1);
#endif
				// Ignore this LEAD message, let the ftimer continue.. because: "this is not my leader!"
			}
		}
		/* Received a LEAD operation from a node with a lower term.
		 * Ignoring this LEAD operation and let the countdown continue to go down.
		 */
		else {
#if 0
			if(sdev->verbose >= 2)
				sassy_dbg("Received APPEND from leader with lower term=%u\n", param1);
#endif
			// Ignore this LEAD message, let the ftimer continue. 
		}
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n",remote_lid, opcode);

	}

	return 0;
}

void init_timeout(struct proto_instance *ins)
{
	int ftime_ns;
	ktime_t timeout;	
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	if(priv->ftimer_init == 1) {
		reset_ftimeout(ins);
		return;
	}

	timeout = get_rnd_timeout(priv->ft_min, priv->ft_max);

	hrtimer_init(&priv->ftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ftimer_init = 1;

	priv->ftimer.function = &_handle_follower_timeout;
	
	priv->accu_rand = timeout; // first rand timeout of this follower

#if 0
	sassy_log_le("%s, %llu, %d: Init follower timeout to %lld ms. \n",
		nstate_string(priv->nstate),
		rdtsc(),
		priv->term,
		ktime_to_ms(timeout));
#endif

	hrtimer_start_range_ns(&priv->ftimer, timeout, TOLERANCE_FTIMEOUT_NS, HRTIMER_MODE_REL_PINNED);
}

void reset_ftimeout(struct proto_instance *ins)
{
	ktime_t timeout;
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	 timeout = get_rnd_timeout(priv->ft_min, priv->ft_max);

	 hrtimer_cancel(&priv->ftimer);
	 hrtimer_set_expires_range_ns(&priv->ftimer, timeout, TOLERANCE_FTIMEOUT_NS);
	 hrtimer_start_expires(&priv->ftimer, HRTIMER_MODE_REL_PINNED);
	
	// priv->accu_rand = timeout; // consequetive rand timeout of this follower

#if 0
	sassy_log_le("%s, %llu, %d: reset follower timeout occured.\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			ktime_to_ms(timeout));
#endif
}

int stop_follower(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	if(priv->ftimer_init == 0)
		return 0;

	priv->ftimer_init = 0;

	return hrtimer_cancel(&priv->ftimer) == 1;
}

int start_follower(struct proto_instance *ins)
{
	int err;
	struct consensus_priv *priv = 
			(struct consensus_priv *)ins->proto_data;
	
	err = setup_le_broadcast_msg(ins, NOOP);

	if(err)
		goto error;

	priv->votes = 0;
	priv->nstate = FOLLOWER;


// True Raft Version: 

 	init_timeout(ins);

#if 0
	sassy_dbg("Node became a follower\n");
#endif

	return 0;

error:
	sassy_error("Failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);
