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


static enum hrtimer_restart _handle_follower_timeout(struct hrtimer *timer)
{
	int err;
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ftimer);
	struct asguard_device *sdev = priv->sdev;
	ktime_t timeout;

	if(sdev->verbose)
		asguard_dbg(" follower timeout: llts_before_ftime=%llu and last_leader_ts=%llu\n", priv->llts_before_ftime, sdev->last_leader_ts);

	if (priv->ftimer_init == 0 || priv->nstate != FOLLOWER)
		return HRTIMER_NORESTART;

	write_log(&priv->ins->logger, FOLLOWER_TIMEOUT, RDTSC_ASGUARD);

	// optimistical timestamping from leader pkt -> do not start candidature, restart follower timeout
	if(priv->llts_before_ftime != sdev->last_leader_ts) {
		priv->llts_before_ftime = sdev->last_leader_ts;
		if(sdev->verbose)
			asguard_dbg("optimistical timestamping of leader pkt prevented follower timeout\n");
	 	timeout = get_rnd_timeout(priv->ft_min, priv->ft_max);
		hrtimer_forward_now (timer, timeout);
		return HRTIMER_RESTART;
	}

#if VERBOSE_DEBUG
	if(sdev->verbose)
		asguard_log_le("%s, %llu, %d: Follower timer timed out\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term);
#endif

	err = node_transition(priv->ins, CANDIDATE);

	write_log(&priv->ins->logger, FOLLOWER_BECOME_CANDIDATE, RDTSC_ASGUARD);

	if (err) {
		asguard_dbg("Error occured during the transition to candidate role\n");
		return HRTIMER_NORESTART;
	}

	return HRTIMER_NORESTART;
}

void reply_append(struct proto_instance *ins,  struct pminfo *spminfo, int remote_lid, int rcluster_id, int param1, int append_success, u32 logged_idx)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;
	struct asguard_payload *pkt_payload;
	char *pkt_payload_sub;
	int hb_passive_ix;

#if VERBOSE_DEBUG
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

	hb_passive_ix =
	     !!!spminfo->pm_targets[remote_lid].pkt_data.hb_active_ix;

	pkt_payload =
		spminfo->pm_targets[remote_lid].pkt_data.pkt_payload[hb_passive_ix];

	pkt_payload_sub =
		asguard_reserve_proto(ins->instance_id, pkt_payload, ASGUARD_PROTO_CON_PAYLOAD_SZ);


	if (!pkt_payload_sub) {
		asguard_error("Sassy packet full!\n");
		return;
	}

	set_le_opcode((unsigned char *)pkt_payload_sub, APPEND_REPLY, param1, append_success, logged_idx, priv->sm_log.stable_idx);

	spminfo->pm_targets[remote_lid].pkt_data.hb_active_ix = hb_passive_ix;

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
	priv->sdev->fire = 1;

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
		asguard_dbg("Claimed num of log entries would exceed packet size!\n");
		goto error;
	}

	cur_ptr = GET_CON_PROTO_ENTRIES_START_PTR(pkt);

	for (i = start_log_idx; i <= new_last; i++) {

		if(priv->sm_log.entries[i] != NULL) // do not (re-)apply redundant retransmissions
			continue; // TODO: can we return?

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

		err = append_command(&priv->sm_log, cur_cmd, priv->term, i, unstable);

		if (err) {
			// kfree(cur_cmd); // free memory of failed log entry.. others from this loop (?)
			goto error;
		}
	}

	if(!unstable) {
		// fix stable index after stable append
		for (i = 0; i <= priv->sm_log.last_idx; i++) {

			if (!priv->sm_log.entries[i]) // stop at first missing entry
				break;

			priv->sm_log.stable_idx = i;
		}
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
	u16 pkt_size;
	int unstable = 0;
	int start_idx, i;

	num_entries = GET_CON_AE_NUM_ENTRIES_VAL(pkt);

	if (num_entries == 0)
		return;	// no reply if nothing to append!

	pkt_size = GET_PROTO_OFFSET_VAL(pkt);
	prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
	prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);

	// if (check_append_rpc(pkt_size, *prev_log_term, *prev_log_idx, priv->max_entries_per_pkt)) {
	// 	asguard_dbg("invalid data: pkt_size=%hu, prev_log_term=%d, prev_log_idx=%d\n",
	// 			  pkt_size, *prev_log_term, *prev_log_idx);
	// 	goto reply_false;
	// }

	// if (num_entries < 0 || num_entries > priv->max_entries_per_pkt) {
	// 	asguard_dbg("invalid num_entries=%d\n", num_entries);
	// 	goto reply_false;
	// }

	/*   If we receive a log replication from the current leader,
	 * 	 we can continue to store it even if a previous part is missing.
	 *
	 * We call log replications with missing parts "unstable" replications.
	 * A stable index points to the last entry in the the log, where
	 * all previous entries exist (stable is not necessarily commited!).
	 */

	mutex_lock(&priv->sm_log.mlock);

	// unstable append?
	if(*prev_log_idx < priv->sm_log.stable_idx){
		// skip this!
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

			if(priv->sm_log.next_retrans_req_idx == -2) {
				priv->sm_log.next_retrans_req_idx = priv->sm_log.stable_idx;
			}

		} else {
			asguard_dbg("Case unhandled!\n");
		}
	}

	start_idx = (*prev_log_idx) + 1;

	asguard_dbg("prev_log_idx=%d, stable_idx=%d, last_idx=%d\n",
				*prev_log_idx, priv->sm_log.stable_idx, priv->sm_log.last_idx);

	if (unstable){
		printk(KERN_INFO "[Unstable] appending entries %d - %d\n", start_idx, start_idx + num_entries);

	} else if (_check_prev_log_match(priv, *prev_log_term, *prev_log_idx)) {
		asguard_dbg("Log inconsitency detected. prev_log_term=%d, prev_log_idx=%d\n",
				*prev_log_term, *prev_log_idx);

		print_log_state(&priv->sm_log);

		goto reply_false_unlock;
	}else {
		printk(KERN_INFO "[Stable] appending entries %d - %d\n", start_idx, start_idx + num_entries);
	}

	// append entries and if it fails, reply false
	if (append_commands(priv, pkt, num_entries, pkt_size, start_idx, unstable)) {
		asguard_dbg("append commands failed. start_idx=%d, unstable=%d\n", start_idx, unstable);
		goto reply_false_unlock;
	}

	//mutex_unlock(&priv->sm_log.mlock);

	// // check commit index
	// leader_commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);

	// // check if leader_commit_idx is out of bounds
	// if (*leader_commit_idx < -1 || *leader_commit_idx > MAX_CONSENSUS_LOG) {
	// 	asguard_dbg("Out of bounds: leader_commit_idx=%d", *leader_commit_idx);
	// 	goto reply_false;
	// }

	// // check if leader_commit_idx points to a valid entry
	// if (*leader_commit_idx > priv->sm_log.last_idx) {
	// 	asguard_dbg("Not referencing a valid log entry: leader_commit_idx=%d, priv->sm_log.last_idx=%d",
	// 			*leader_commit_idx, priv->sm_log.last_idx);
	// 	goto reply_false;
	// }


	// // Check if commit index must be updated
	// if (*leader_commit_idx > priv->sm_log.commit_idx) {
	// 	// min(leader_commit_idx, last_idx)
	// 	// note: last_idx of local log can not be null if append_commands was successfully executed
	// 	priv->sm_log.commit_idx = *leader_commit_idx > priv->sm_log.last_idx ? priv->sm_log.last_idx : *leader_commit_idx;
	// 	commit_log(priv);
	// }


	/* if unstable => request retransmission
	 */
	for(i = 0 > priv->sm_log.stable_idx ? 0 : priv->sm_log.stable_idx; i < priv->sm_log.last_idx; i++) {
		if(!priv->sm_log.entries[i]){
			priv->sm_log.next_retrans_req_idx = i ;
			break;
		}
		if(i == priv->sm_log.last_idx - 1) {
			priv->sm_log.next_retrans_req_idx = -2;
		}
	}

	if(priv->sm_log.next_retrans_req_idx != -2)
		goto reply_retransmission;



// default: reply success
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 1, priv->sm_log.stable_idx);
	priv->sdev->fire = 1;
	mutex_unlock(&priv->sm_log.mlock);
	return;
reply_retransmission:
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 2, priv->sm_log.next_retrans_req_idx);
	priv->sdev->fire = 1;
	mutex_unlock(&priv->sm_log.mlock);
	return;
reply_false_unlock:
	mutex_unlock(&priv->sm_log.mlock);
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 0, priv->sm_log.stable_idx);
	priv->sdev->fire = 1;

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
			//reset_ftimeout(ins);
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
			//reset_ftimeout(ins);
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
			asguard_error("received APPEND from a node that is not accepted as leader \n");
			break;
		}
		/* Received a LEAD operation from a node with a higher term,
		 * thus this node was accecpted
		 */
		if (param1 > priv->term) {
			// accept_leader(ins, remote_lid, rcluster_id, param1);
			// write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, RDTSC_ASGUARD);

			_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

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
				// follower timeout already handled.
				//reset_ftimeout(ins);
				_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

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

void init_timeout(struct proto_instance *ins)
{
	ktime_t timeout;
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	if (priv->ftimer_init == 1) {
		reset_ftimeout(ins);
		return;
	}

	timeout = get_rnd_timeout(priv->ft_min, priv->ft_max);

	hrtimer_init(&priv->ftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ftimer_init = 1;

	priv->ftimer.function = &_handle_follower_timeout;

	priv->accu_rand = timeout; // first rand timeout of this follower

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: Init follower timeout to %lld ms.\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
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

	 priv->llts_before_ftime = priv->sdev->last_leader_ts;

	 hrtimer_start_expires(&priv->ftimer, HRTIMER_MODE_REL_PINNED);

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: reset follower timeout occured: new timeout is %lld ms\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term,
			ktime_to_ms(timeout));
#endif
}

int stop_follower(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	if (priv->ftimer_init == 0)
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

	if (err)
		goto error;

	priv->sdev->tx_port = 3319;
	priv->sdev->is_leader = 0;
	mutex_init(&priv->sm_log.mlock);
	mutex_init(&priv->accept_vote_lock);

	priv->votes = 0;
	priv->nstate = FOLLOWER;
	priv->candidate_counter = 0;

	//init_timeout(ins);

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
