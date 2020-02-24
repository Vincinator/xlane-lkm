#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/timer.h>

#include <asguard/payload_helper.h>
#include <asguard/consensus.h>
#include <asguard/logger.h>
#include <asguard/asguard.h>

#include "include/follower.h"
#include "include/consensus_helper.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][LE][FOLLOWER]"

void reply_append(struct proto_instance *ins,  struct pminfo *spminfo, int remote_lid, int rcluster_id, int param1, int append_success, u32 logged_idx)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;
	struct asguard_payload *pkt_payload;
	char *pkt_payload_sub;
	int hb_passive_ix;

#if 1
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: REPLY APPEND state=%d, param1=%d, param3=%d, param4=%d\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term,
			append_success,
			param1,
			logged_idx,
			priv->sm_log.stable_idx);
#endif

	pkt_payload =
		spminfo->pm_targets[remote_lid].pkt_data.pkt_payload;

	pkt_payload_sub =
		asguard_reserve_proto(ins->instance_id, pkt_payload, ASGUARD_PROTO_CON_PAYLOAD_SZ);


	if (!pkt_payload_sub) {
		asguard_error("Sassy packet full!\n");
		return;
	}

	set_le_opcode((unsigned char *)pkt_payload_sub, APPEND_REPLY, param1, append_success, logged_idx, priv->sm_log.stable_idx);

	if (append_success)
		write_log(&ins->logger, REPLY_APPEND_SUCCESS, RDTSC_ASGUARD);
	else
		write_log(&ins->logger, REPLY_APPEND_FAIL, RDTSC_ASGUARD);

}
void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, s32 param1, s32 param2)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;



#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: voting for cluster node %d with term %d\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term,
			rcluster_id,
			param1);
#endif

	setup_le_msg(ins, &priv->sdev->pminfo, VOTE, remote_lid, param1, param2, 0, 0);
	priv->voted = param1;

	priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;


	write_log(&ins->logger, VOTE_FOR_CANDIDATE, RDTSC_ASGUARD);

}



int append_commands(struct consensus_priv *priv, unsigned char *pkt, int num_entries, int pkt_size, int start_log_idx, int unstable)
{
	int i, err, new_last;
	u32 *cur_ptr;
	struct sm_command *cur_cmd;

	new_last = start_log_idx + num_entries - 1;

	if (new_last >= MAX_CONSENSUS_LOG) {
		asguard_dbg("Local log is full!\n");
		err = -ENOMEM;
		goto error;
	}

	// check if entries would exceed pkt
	if ((num_entries * AE_ENTRY_SIZE + ASGUARD_PROTO_CON_AE_BASE_SZ) > pkt_size) {
		err = -EINVAL;
		asguard_dbg("Claimed num of log entries (%d) would exceed packet size!\n", num_entries);
		goto error;
	}

	cur_ptr = GET_CON_PROTO_ENTRIES_START_PTR(pkt);

	for (i = start_log_idx; i <= new_last; i++) {

		if(priv->sm_log.entries[i] != NULL) // do not (re-)apply redundant retransmissions
			continue; // TODO: can we return?

        // freed by consensus_clean
		cur_cmd = kmalloc(sizeof(struct sm_command), GFP_KERNEL);

		if (!cur_cmd) {
			err = -ENOMEM;
			asguard_dbg("Out of Memory\n");
			goto error;
		}

		cur_cmd->sm_logvar_id = *cur_ptr;
		cur_ptr++;
		cur_cmd->sm_logvar_value = *cur_ptr;
		cur_ptr++;

		err = append_command(priv, cur_cmd, priv->term, i, unstable);

		if (err) {
			// kfree(cur_cmd); // free memory of failed log entry.. others from this loop (?)
			goto error;
		}
	}

	if(!unstable) {
		update_stable_idx(priv);
	}

	return 0;

error:
	asguard_error("Failed to append commands to log\n");
	return err;
}

void remove_from_log_until_last(struct state_machine_cmd_log *log, int start_idx)
{
	int i;

	if (log->last_idx < 0) {
		asguard_dbg("Log already empty.");
		return;
	}

	if (start_idx > log->last_idx) {
		asguard_dbg("No Items at index of %d", start_idx);
		return;
	}

	if(start_idx < 0) {
		asguard_error("start_idx=%d is invalid\n", start_idx);
		return;
	}

	for (i = start_idx; i <= log->last_idx; i++){
		if (log->entries[i]) // entries are NULL initialized
			kfree(log->entries[i]);
	}

	log->last_idx = start_idx - 1;

}


u32 _check_prev_log_match(struct consensus_priv *priv, u32 prev_log_term, s32 prev_log_idx)
{
	struct sm_log_entry *entry;

	if (prev_log_idx == -1) {
		if( prev_log_term < priv->term) {
			asguard_error(" received append RPC with lower prev term");
			// TODO: handle this case.
		}
		return 0;
	}

	if (prev_log_idx < -1) {
		asguard_error("prev log idx is invalid! %d\n", prev_log_idx);
		return 1;
	}

	if(prev_log_idx > MAX_CONSENSUS_LOG) {
		asguard_error("Entry at index %d does not exist. Expected stable append.\n", prev_log_idx);
		return 1;
	}

	if (prev_log_idx > priv->sm_log.last_idx) {
		asguard_error("Entry at index %d does not exist. Expected stable append.\n", prev_log_idx);
		return 1;
	}

	entry = priv->sm_log.entries[prev_log_idx];

	if (entry == NULL) {
		asguard_dbg("Unstable commit at index %d was not detected previously! ", prev_log_idx);
		return 1;
	}

	if (entry->term != prev_log_term) {
		asguard_error("prev_log_term does not match %d != %d", entry->term, prev_log_term);

		// Delete entries from prev_log_idx to last_idx
		//remove_from_log_until_last(&priv->sm_log, prev_log_idx);
		return 1;
	}

	return 0;
}

int check_append_rpc(u16 pkt_size, u32 prev_log_term, s32 prev_log_idx, int max_entries_per_pkt)
{

	if (prev_log_idx >= MAX_CONSENSUS_LOG)
		return 1;

	if (pkt_size < 0 || pkt_size > ASGUARD_PROTO_CON_AE_BASE_SZ + (max_entries_per_pkt * AE_ENTRY_SIZE))
		return 1;

	return 0;
}
EXPORT_SYMBOL(check_append_rpc);




void _handle_append_rpc(struct proto_instance *ins, struct consensus_priv *priv, unsigned char *pkt,  int remote_lid, int rcluster_id)
{
	u32 *prev_log_term, num_entries;
	s32 *prev_log_idx;
	s32 *prev_log_commit_idx;
	u16 pkt_size;
	int unstable = 0;
	int start_idx;

	num_entries = GET_CON_AE_NUM_ENTRIES_VAL(pkt);

	priv->sdev->pminfo.pm_targets[remote_lid].received_log_replications++;

	if (num_entries == 0)
		return;	// no reply if nothing to append!

	pkt_size = GET_PROTO_OFFSET_VAL(pkt);
	prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
	prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);
	prev_log_commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);


	// TODO: check!
	if(priv->sm_log.commit_idx > *prev_log_commit_idx)
		asguard_dbg("Recevied commit index is lower than local commit idx! %d\n ", *prev_log_commit_idx);
	else if(priv->sm_log.stable_idx < *prev_log_commit_idx)
		asguard_dbg("Recevied commit index is higher than local stable idx! %d\n ", *prev_log_commit_idx);
	else
		priv->sm_log.commit_idx = *prev_log_commit_idx;

	/*   If we receive a log replication from the current leader,
	 * 	 we can continue to store it even if a previous part is missing.
	 *
	 * We call log replications with missing parts "unstable" replications.
	 * A stable index points to the last entry in the the log, where
	 * all previous entries exist (stable is not necessarily commited!).
	 */
	mutex_lock(&priv->sm_log.mlock);

	if(*prev_log_idx < priv->sm_log.stable_idx){
		mutex_unlock(&priv->sm_log.mlock);
		return;
	}

	if(*prev_log_idx > priv->sm_log.stable_idx) {
		/* Only accept unstable entries if leader and term did not change!
		 *
		 *   If we have a leader change, we must reset last index to the stable index,
		 *   and continue to build the log from there (throw away unstable items).
		 */
		if(*prev_log_term == priv->term && priv->leader_id == rcluster_id){

			unstable = 1;

		} else {
			asguard_dbg("Case unhandled!\n");
		}
	}

	start_idx = (*prev_log_idx) + 1;

	if (_check_prev_log_match(priv, *prev_log_term, priv->sm_log.stable_idx)) {
		asguard_dbg("Log inconsitency detected. prev_log_term=%d, prev_log_idx=%d\n",
				*prev_log_term, *prev_log_idx);

		print_log_state(&priv->sm_log);

		goto reply_false_unlock;
	}

	if (append_commands(priv, pkt, num_entries, pkt_size, start_idx, unstable)) {
		asguard_dbg("append commands failed. start_idx=%d, unstable=%d\n", start_idx, unstable);
		goto reply_false_unlock;
	}

	update_next_retransmission_request_idx(priv);

	if (unstable){
		// printk(KERN_INFO "[Unstable] appending entries %d - %d | re_idx=%d | stable_idx=%d\n",
		// 	start_idx, start_idx + num_entries, priv->sm_log.next_retrans_req_idx, priv->sm_log.stable_idx);
		priv->sm_log.unstable_commits++;
	}
	// else {
	// 		printk(KERN_INFO "[Stable] appending entries %d - %d\n", start_idx, start_idx + num_entries);
	// }

	if(priv->sm_log.next_retrans_req_idx != -2)
		goto reply_retransmission;

// default: reply success
	mutex_unlock(&priv->sm_log.mlock);
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 1, priv->sm_log.stable_idx);
	priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;
	return;
reply_retransmission:
	mutex_unlock(&priv->sm_log.mlock);
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 2, priv->sm_log.next_retrans_req_idx);
	priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;
	return;
reply_false_unlock:
	mutex_unlock(&priv->sm_log.mlock);
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 0, priv->sm_log.stable_idx);
	priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;


}
EXPORT_SYMBOL(_handle_append_rpc);


int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;
	struct asguard_device *sdev = priv->sdev;

	u8 opcode = GET_CON_PROTO_OPCODE_VAL(pkt);
	s32 param1, param2, param3, param4;

	param1 = GET_CON_PROTO_PARAM1_VAL(pkt);


#if VERBOSE_DEBUG
	log_le_rx(sdev->verbose, priv->nstate, RDTSC_ASGUARD, priv->term, opcode, rcluster_id, param1);
#endif

	switch (opcode) {
		// param1 interpreted as term
		// param2 interpreted as candidateID
		// param3 interpreted as lastLogIndex of Candidate
		// param4 interpreted as lastLogTerm of Candidate
	case VOTE:
		break;
	case NOMI:
		param2 = GET_CON_PROTO_PARAM2_VAL(pkt);
		param3 = GET_CON_PROTO_PARAM3_VAL(pkt);
		param4 = GET_CON_PROTO_PARAM4_VAL(pkt);
		if (check_handle_nomination(priv, param1, param2, param3, param4, rcluster_id, remote_lid)) {
			reply_vote(ins, remote_lid, rcluster_id, param1, param2);
		}
		break;
	case NOOP:
		break;
	case ALIVE:
		param2 = GET_CON_PROTO_PARAM2_VAL(pkt);

		/* Received an ALIVE operation from a node that claims to be the new leader
		 */
		if (param1 > priv->term) {
			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, RDTSC_ASGUARD);

#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asguard_dbg("Received ALIVE op from new leader with higher term=%d local term=%d\n", param1, priv->term);
#endif
		}

		// Current Leader is commiting
		if(param1 == priv->term && param2 != -1){
			// Check if commit index must be updated
			if (param2 > priv->sm_log.commit_idx) {
				if(param2 > priv->sm_log.stable_idx){
					asguard_dbg("detected consensus BUG. commit idx is greater than local stable idx\n");
					asguard_dbg("\t leader commit idx: %d, local stable idx: %d\n", param2, priv->sm_log.stable_idx);
				} else {
					priv->sm_log.commit_idx = param2;
					commit_log(priv);
				}
			}
		}

		/* Ignore other cases for ALIVE operation*/

		break;
	case APPEND:

		if(priv->leader_id != rcluster_id) {
			// asguard_error("received APPEND from a node that is not accepted as leader \n");
			break;
		}
		/* Received a LEAD operation from a node with a higher term,
		 * thus this node was accecpted
		 */
		if (param1 > priv->term) {
			// accept_leader(ins, remote_lid, rcluster_id, param1);
			// write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, RDTSC_ASGUARD);

			_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);
			commit_log(priv);
#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asguard_dbg("Received APPEND op from leader with higher term=%d local term=%d\n", param1, priv->term);
#endif
		}

		/* Received a LEAD operation from a node with the same term,
		 * thus, this node has to check whether it is already following
		 * that node (continue to follow), or if it is a LEAD operation
		 * from a node that is currently not the follower (Ignore and let
		 * the timeout continue to go down).
		 */
		else if (param1 == priv->term) {

			if (priv->leader_id == rcluster_id) {
				_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

				// Commit log!
				commit_log(priv);

// #if VERBOSE_DEBUG
// 				if (sdev->verbose >= 5)
// 					asguard_dbg("Received message from known leader (%d) term=%u\n", rcluster_id, param1);
// #endif

			} else {
#if VERBOSE_DEBUG
				if (sdev->verbose >= 1)
					asguard_dbg("Received message from new leader (%d)! Leader changed but term did not update! term=%u\n",
					rcluster_id, param1);
#endif
				// Ignore this LEAD message, let the ftimer continue.. because: "this is not my leader!"
			}
		}
		/* Received a LEAD operation from a node with a lower term.
		 * Ignoring this LEAD operation and let the countdown continue to go down.
		 */
		else {
#if VERBOSE_DEBUG
			if (sdev->verbose >= 5)
				asguard_dbg("Received APPEND from leader (%d) with lower term=%u\n", rcluster_id, param1);
#endif
			if (priv->leader_id == rcluster_id) {
				asguard_error("BUG! Current accepted LEADER (%d) has lower Term=%u\n", rcluster_id,  param1);
			}

			// Ignore this LEAD message, let the ftimer continue.
		}
		break;
	default:
		asguard_dbg("Unknown opcode received from host: %d - opcode: %d\n", rcluster_id, opcode);

	}

	return 0;
}

void print_follower_stats(struct consensus_priv *priv)
{
	int i;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++){
		asguard_dbg("Target infos %d:", i);
		asguard_dbg("\t pkt TX counter: %d\n",  priv->sdev->pminfo.pm_targets[i].pkt_tx_counter);
		asguard_dbg("\t pkt RX counter: %d\n", priv->sdev->pminfo.pm_targets[i].pkt_rx_counter);
		asguard_dbg("\t received log reps(all): %d\n", priv->sdev->pminfo.pm_targets[i].received_log_replications);
	}

	asguard_dbg("unstable commits %d\n", priv->sm_log.unstable_commits );
	asguard_dbg("last_idx %d \n", priv->sm_log.last_idx );
	asguard_dbg("stable_idx %d\n", priv->sm_log.stable_idx );
	asguard_dbg("next_retrans_req_idx %d\n", priv->sm_log.next_retrans_req_idx );
	asguard_dbg("max_entries %d\n", priv->sm_log.max_entries );
}


int stop_follower(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	print_follower_stats(priv);

	return 0;
}

int start_follower(struct proto_instance *ins)
{
	int err;
	struct consensus_priv *priv =
			(struct consensus_priv *)ins->proto_data;

	err = setup_le_broadcast_msg(ins, NOOP);

	if (err)
		goto error;

	priv->sdev->tx_port = 3319;
	priv->sdev->is_leader = 0;
	priv->sm_log.unstable_commits = 0;
	mutex_init(&priv->sm_log.mlock);
	mutex_init(&priv->accept_vote_lock);

	priv->votes = 0;
	priv->nstate = FOLLOWER;
	priv->candidate_counter = 0;

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_dbg("Node became a follower\n");
#endif

	return 0;

error:
	asguard_error("Failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);
