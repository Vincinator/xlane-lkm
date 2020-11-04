#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/timer.h>

#include <asgard/payload_helper.h>
#include <asgard/consensus.h>
#include <asgard/logger.h>
#include <asgard/asgard.h>

#include "include/follower.h"
#include "include/consensus_helper.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][LE][FOLLOWER]"

void reply_append(struct proto_instance *ins,  struct pminfo *spminfo, int remote_lid, int rcluster_id, int param1, int append_success, u32 logged_idx)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	struct asgard_payload *pkt_payload;
	char *pkt_payload_sub;

#if 0
	if(priv->sdev->verbose)
		asgard_log_le("%s, %llu, %d: REPLY APPEND state=%d, param1=%d, param3=%d, param4=%d\n, CPU=%d",
			nstate_string(priv->nstate),
			RDTSC_ASGARD,
			priv->term,
			append_success,
			param1,
			logged_idx,
			priv->sm_log.stable_idx,
			smp_processor_id());
#endif

    spin_lock(&spminfo->pm_targets[remote_lid].pkt_data.lock);

	pkt_payload =
		spminfo->pm_targets[remote_lid].pkt_data.payload;

	pkt_payload_sub =
		asgard_reserve_proto(ins->instance_id, pkt_payload, ASGARD_PROTO_CON_PAYLOAD_SZ);


	if (!pkt_payload_sub) {
		asgard_error("asgard packet full!\n");
        spin_unlock(&spminfo->pm_targets[remote_lid].pkt_data.lock);
        return;
    }

	set_le_opcode((unsigned char *)pkt_payload_sub, APPEND_REPLY, param1, append_success, logged_idx, priv->sm_log.stable_idx);

    spin_unlock(&spminfo->pm_targets[remote_lid].pkt_data.lock);

	if (append_success)
		write_log(&ins->logger, REPLY_APPEND_SUCCESS, RDTSC_ASGARD);
	else
		write_log(&ins->logger, REPLY_APPEND_FAIL, RDTSC_ASGARD);

}
void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, s32 param1, s32 param2)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asgard_log_le("%s, %llu, %d: voting for cluster node %d with term %d\n",
			nstate_string(priv->nstate),
			RDTSC_ASGARD,
			priv->term,
			rcluster_id,
			param1);
#endif

	setup_le_msg(ins, &priv->sdev->pminfo, VOTE, remote_lid, param1, param2, 0, 0);
	priv->voted = param1;

	priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;


	write_log(&ins->logger, VOTE_FOR_CANDIDATE, RDTSC_ASGARD);

}



int append_commands(struct consensus_priv *priv, unsigned char *pkt, int num_entries, int pkt_size, int start_log_idx, int unstable)
{
	int i, err, new_last;
    struct data_chunk *cur_ptr;

	new_last = start_log_idx + num_entries - 1;

	// check if entries would exceed pkt
	if ((num_entries * AE_ENTRY_SIZE + ASGARD_PROTO_CON_AE_BASE_SZ) > pkt_size) {
		err = -EINVAL;
		asgard_dbg("Claimed num of log entries (%d) would exceed packet size!\n", num_entries);
		goto error;
	}

	cur_ptr = (struct data_chunk *) GET_CON_PROTO_ENTRIES_START_PTR(pkt);

	for (i = start_log_idx; i <= new_last; i++) {

	    /* Directly pass data ptr into pkt location */
	    err = append_command(priv, (struct data_chunk *) cur_ptr, priv->term, i, unstable);

		if (err) {
			goto error;
		}
        cur_ptr++;

	}

	if(!unstable) {
		update_stable_idx(priv);
	}

    /*asgard_dbg("Handled APPEND RPC\n"
                "\t stable_idx = %d\n"
                "\t commit_idx = %d\n"
                "\t received entries [ %d - %d ]\n"
                "\t CPU = %d\n",
                priv->sm_log.stable_idx,
                priv->sm_log.commit_idx,
                start_log_idx,
                new_last,
                smp_processor_id());*/

	return 0;

error:
	asgard_error("Failed to append commands to log\n");
	return err;
}


u32 _check_prev_log_match(struct consensus_priv *priv, u32 prev_log_term, s32 prev_log_idx)
{
	struct sm_log_entry *entry;
	u32 buf_prevlogidx;


	if (prev_log_idx == -1) {
		if( prev_log_term < priv->term) {
			asgard_error(" received append RPC with lower prev term");
			// TODO: handle this case.
		}
		return 0;
	}

	if (prev_log_idx < -1) {
		asgard_error("prev log idx is invalid! %d\n", prev_log_idx);
		return 1;
	}

    buf_prevlogidx = consensus_idx_to_buffer_idx(&priv->sm_log, prev_log_idx);

	if(buf_prevlogidx < 0 ) {
        asgard_dbg("Error converting consensus idx to buffer in %s", __FUNCTION__);
        return -1;
	}

	if (prev_log_idx > priv->sm_log.last_idx) {
		asgard_error("Entry at index %d does not exist. Expected stable append.\n", prev_log_idx);
		return 1;
	}

	entry = priv->sm_log.entries[buf_prevlogidx];

	if (entry == NULL) {
		asgard_dbg("Unstable commit at index %d was not detected previously! ", prev_log_idx);
		return 1;
	}

	if (entry->term != prev_log_term) {
		asgard_error("prev_log_term does not match %d != %d", entry->term, prev_log_term);

		// Delete entries from prev_log_idx to last_idx
		//remove_from_log_until_last(&priv->sm_log, prev_log_idx);
		return 1;
	}

	return 0;
}

int check_append_rpc(u16 pkt_size, u32 prev_log_term, s32 prev_log_idx, int max_entries_per_pkt)
{


	if (pkt_size < 0 || pkt_size > ASGARD_PROTO_CON_AE_BASE_SZ + (max_entries_per_pkt * AE_ENTRY_SIZE))
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
	int start_idx;

	num_entries = GET_CON_AE_NUM_ENTRIES_VAL(pkt);

	priv->sdev->pminfo.pm_targets[remote_lid].received_log_replications++;

	if (num_entries == 0)
        return;    // no reply if nothing to append!

	pkt_size = GET_PROTO_OFFSET_VAL(pkt);
	prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
	prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);

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
			asgard_dbg("Case unhandled!\n");
		}
	}

	start_idx = (*prev_log_idx) + 1;

	if (_check_prev_log_match(priv, *prev_log_term, priv->sm_log.stable_idx)) {
		asgard_dbg("Log inconsitency detected. prev_log_term=%d, prev_log_idx=%d\n",
				*prev_log_term, *prev_log_idx);

		print_log_state(&priv->sm_log);

		goto reply_false_unlock;
	}

	if (append_commands(priv, pkt, num_entries, pkt_size, start_idx, unstable)) {
		asgard_dbg("append commands failed. start_idx=%d, unstable=%d\n", start_idx, unstable);
		goto reply_false_unlock;
	}

	// update_next_retransmission_request_idx(priv);

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
    if (priv->sdev->multicast.enable)
        return;
	reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 1, priv->sm_log.stable_idx);
	priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;
    return;
reply_retransmission:
	mutex_unlock(&priv->sm_log.mlock);

	// TODO: wait until other pending workers are done, and check again if we need a retransmission!
    asgard_dbg("suppressed retransmission\n");
	//reply_append(ins, &priv->sdev->pminfo, remote_lid, rcluster_id, priv->term, 2, priv->sm_log.next_retrans_req_idx);
	//priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;
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
	struct asgard_device *sdev = priv->sdev;

    u8 opcode = GET_CON_PROTO_OPCODE_VAL(pkt);
	s32 param1, param2, param3, param4, *commit_idx;

	param1 = GET_CON_PROTO_PARAM1_VAL(pkt);

	//log_le_rx(sdev->verbose, priv->nstate, RDTSC_ASGARD, priv->term, opcode, rcluster_id, param1);

	switch (opcode) {
		// param1 interpreted as term
		// param2 interpreted as candidateID
		// param3 interpreted as lastLogIndex of Candidate
		// param4 interpreted as lastLogTerm of Candidate
    case ADVERTISE:
        break;
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
			write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, RDTSC_ASGARD);

#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asgard_dbg("Received ALIVE op from new leader with higher term=%d local term=%d\n", param1, priv->term);
#endif
		}

		// Current Leader is commiting
		if(param1 == priv->term && param2 != -1)
            commit_log(priv, param2);

		/* Ignore other cases for ALIVE operation*/
		break;
	case APPEND:

/*        asgard_dbg("APPEND from %d with prev_log_idx=%d leader_commit_idx=%d\n",
                   rcluster_id,
                   *GET_CON_AE_PREV_LOG_IDX_PTR(pkt),
                   *GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt));*/

        if(priv->leader_id != rcluster_id) {

            // asgard_error("received APPEND from a node that is not accepted as leader \n");
			break;
		}
		/* Received a LEAD operation from a node with a higher term,
		 * thus this node was accecpted
		 */
		if (param1 > priv->term) {
			// accept_leader(ins, remote_lid, rcluster_id, param1);
			// write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, RDTSC_ASGARD);

			_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asgard_dbg("Received APPEND op from leader with higher term=%d local term=%d\n", param1, priv->term);
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

                commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);
                commit_log(priv, *commit_idx);

			} else {
#if VERBOSE_DEBUG

                if (sdev->verbose >= 1)
					asgard_dbg("Received message from new leader (%d)! Leader changed but term did not update! term=%u\n",
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
				asgard_dbg("Received APPEND from leader (%d) with lower term=%u\n", rcluster_id, param1);
#endif
			if (priv->leader_id == rcluster_id) {
				asgard_error("BUG! Current accepted LEADER (%d) has lower Term=%u\n", rcluster_id,  param1);
			}

			// Ignore this LEAD message, let the ftimer continue.
		}

        break;
	default:
		asgard_dbg("Unknown opcode received from host: %d - opcode: %d\n", rcluster_id, opcode);

	}

	return 0;
}

void validate_log(struct state_machine_cmd_log *log)
{
    int i, err = 0;

    asgard_dbg("Log Validation Results: \n");

    for(i = 0; i < log->max_entries; i++) {

        if(log->entries[i] == NULL) {

            if(log->stable_idx > i) {
                asgard_error("\t Stable Index is incorrect.\n");
                asgard_error("\t\t stable_idx=%d, but log entry %d is NULL\n", log->stable_idx, i);
                err++;
            }

            if(log->commit_idx > i) {
                asgard_error("\t Commit Index is incorrect. \n");
                asgard_error("\t\t commit_idx=%d, but log entry %d is NULL\n", log->commit_idx, i);
                err++;
            }

        } else {
            if(log->last_idx < i) {
                asgard_error("\t No entries allowed after last index\n");
                asgard_error("\t\t last index =%d, but log entry %d exists\n", log->last_idx, i);
                err++;
            }
        }
    }

    if(log->stable_idx > log->commit_idx) {
        asgard_error("\t Commit index did not keep up with Stable Index \n");
        asgard_error("\t\t commit_idx=%d, stable_idx=%d\n", log->commit_idx, log->stable_idx);
    }

    if(log->stable_idx < log->commit_idx) {
        asgard_error("\t Commit index points to an unstable log entry \n");
        asgard_error("\t\t commit_idx=%d, stable_idx=%d\n", log->commit_idx, log->stable_idx);
    }

    asgard_dbg("\t Detected %d Errors in Log \n", err);

}

void print_follower_stats(struct consensus_priv *priv)
{
	int i;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++){
		asgard_dbg("Target infos %d:", i);
		asgard_dbg("\t pkt TX counter: %d\n",  priv->sdev->pminfo.pm_targets[i].pkt_tx_counter);
		asgard_dbg("\t pkt RX counter: %d\n", priv->sdev->pminfo.pm_targets[i].pkt_rx_counter);
		asgard_dbg("\t received log reps(all): %d\n", priv->sdev->pminfo.pm_targets[i].received_log_replications);
	}

	asgard_dbg("unstable commits %d\n", priv->sm_log.unstable_commits );
	asgard_dbg("last_idx %d \n", priv->sm_log.last_idx );
	asgard_dbg("stable_idx %d\n", priv->sm_log.stable_idx );
	asgard_dbg("next_retrans_req_idx %d\n", priv->sm_log.next_retrans_req_idx );
	asgard_dbg("max_entries %d\n", priv->sm_log.max_entries );

	/* TODO: redo for ring buffer implementation */
	//validate_log(&priv->sm_log);

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
	priv->sm_log.turn = 0;
	mutex_init(&priv->sm_log.mlock);
    mutex_init(&priv->sm_log.next_lock);

    mutex_init(&priv->accept_vote_lock);

	priv->votes = 0;
	priv->nstate = FOLLOWER;
	priv->candidate_counter = 0;

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asgard_dbg("Node became a follower\n");
#endif

	return 0;

error:
	asgard_error("Failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);
