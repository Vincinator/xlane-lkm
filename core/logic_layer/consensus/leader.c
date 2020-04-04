#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>
#include <linux/slab.h>

#include "include/consensus_helper.h"
#include "include/leader.h"
#include <asguard/consensus.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][LE][LEADER]"

struct consensus_priv;

void initialize_indices(struct consensus_priv *priv)
{
	int i;
    struct asguard_payload *pkt_payload;

	for (i = 0; i < priv->sdev->pminfo.num_of_targets; i++) {

		// initialize to leader last log index + 1
		priv->sm_log.next_index[i] = priv->sm_log.stable_idx + 1;
		priv->sm_log.match_index[i] = -1;
		priv->sm_log.num_retransmissions[i] = 0;

		rwlock_init(&priv->sm_log.retrans_list_lock[i]);

		INIT_LIST_HEAD(&priv->sm_log.retrans_head[i]);

		priv->sm_log.retrans_entries[i] = 0;

        pkt_payload =
                priv->sdev->pminfo.pm_targets[i].pkt_data.hb_pkt_payload;

        update_alive_msg(priv->sdev, pkt_payload, i);


    }

}

int _is_potential_commit_idx(struct consensus_priv *priv, int N)
{
	int i, hits;

	hits = 0;

	for (i = 0; i < priv->sdev->pminfo.num_of_targets; i++) {

		if(!priv->sdev->pminfo.pm_targets[i].alive){
			hits++; // If node is dead, count as hit
			asguard_dbg("Did not consider dead node for consensus\n");
			continue;
		}

		if (priv->sm_log.match_index[i] >= N)
			hits++;
	}
	return hits >= priv->sdev->pminfo.num_of_targets;
}

void update_commit_idx(struct consensus_priv *priv)
{
	s32 N, current_N, i;
	int buf_idx_current_N;

	if (!priv) {
		asguard_error("priv is NULL!\n");
		return;
	}

	N = -1;

	// each match_index is a potential new commit_idx candidate
	for (i = 0; i < priv->sdev->pminfo.num_of_targets; i++) {

		current_N = priv->sm_log.match_index[i];

		if(current_N == -1)
			return; // nothing to commit yet.

		if (current_N >= MAX_CONSENSUS_LOG) {
			asguard_error("current_N=%d is invalid\n", current_N);
			return;
		}

		if(current_N > priv->sm_log.last_idx){
			asguard_dbg("BUG! current_N (%d) > priv->sm_log.last_idx(%d) \n",
					current_N, priv->sm_log.last_idx );
			return;
		}

        buf_idx_current_N = consensus_idx_to_buffer_idx(&priv->sm_log, current_N);

		if(!priv->sm_log.entries[buf_idx_current_N]) {
			asguard_dbg("BUG! log entry at %d is NULL\n",
                        buf_idx_current_N );
			return;
		}
		if (priv->sm_log.entries[buf_idx_current_N]->term == priv->term)
			if (_is_potential_commit_idx(priv, current_N))
				if (current_N > N)
					N = current_N;
	}

	if (priv->sm_log.commit_idx < N) {
		priv->sm_log.commit_idx = N;
        priv->sm_log.last_applied = N; // Only valid for leader!

    }

}

void queue_retransmission(struct consensus_priv *priv, int remote_lid, s32 retrans_idx){

	struct retrans_request *new_req, *entry, *tmp_entry;

	rmb();
	write_lock(&priv->sm_log.retrans_list_lock[remote_lid]);

	list_for_each_entry_safe(entry, tmp_entry, &priv->sm_log.retrans_head[remote_lid], retrans_req_head)
    {
		if(entry->request_idx == retrans_idx){
			write_unlock(&priv->sm_log.retrans_list_lock[remote_lid]);
			return;
		}
    }

    // freed by clean_request_transmission_lists
	new_req = (struct retrans_request *)
		kmalloc(sizeof(struct retrans_request), GFP_KERNEL);

	if(!new_req) {
		asguard_error("Could not allocate mem for new retransmission request list item\n");
		return;
	}

	new_req->request_idx = retrans_idx;

	asguard_dbg(" Added request idx %d to list for target=%d \n", retrans_idx, remote_lid);

	list_add_tail(&(new_req->retrans_req_head), &priv->sm_log.retrans_head[remote_lid]);
	priv->sm_log.num_retransmissions[remote_lid]++;
	priv->sm_log.retrans_entries[remote_lid]++;
	write_unlock(&priv->sm_log.retrans_list_lock[remote_lid]);

}

int leader_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	struct asguard_device *sdev = priv->sdev;

	u8 opcode = GET_CON_PROTO_OPCODE_VAL(pkt);
	s32 param1, param2, param3, param4;
	//s32 param4 = GET_CON_PROTO_PARAM4_VAL(pkt);

	switch (opcode) {
	case VOTE:
		break;
	case NOMI:
		break;
	case NOOP:
		break;
	case APPEND_REPLY:
		// param1 intepreted as last term of follower
		// param2 interpreted as success
		// param3 contains last idx in follower log
		param2 = GET_CON_PROTO_PARAM2_VAL(pkt);
		param3 = GET_CON_PROTO_PARAM3_VAL(pkt);
		param4 = GET_CON_PROTO_PARAM4_VAL(pkt);

		// check if success

		if (param2 == 1) {
			// append rpc success!
			//asguard_dbg("Received Reply with State=success param3=%d\n",param3);
			// update match Index for follower with <remote_lid>

			// Asguard can potentially send multiple appendEntries RPCs, and after each RPC
			// the next_index must be updated to indicate wich entries to send next..
			// But the replies to the appendEntries RPC will indicate that only to certain index the
			// follower log was updated. Thus, the follower must include the information to which
			// index it has updated the follower log. As an alternative, the leader could remember a state
			// including the index after emitting the udp packet..
			//priv->sm_log.match_index[remote_lid] = priv->sm_log.next_index[remote_lid] - 1;
			// asguard_dbg("Received Reply with State=success.. rcluster_id=%d, param4=%d\n", rcluster_id, param4);

			if(priv->sm_log.last_idx < param4) {
				asguard_error("(1) match index (%d) is greater than local last idx (%d)!\n", param4, priv->sm_log.last_idx);
				break; // ignore this faulty packet
			}

			priv->sm_log.match_index[remote_lid] = param4;
			update_commit_idx(priv);

			// check for unstable logs at remote & init resubmission of missing part

		} else if (param2 == 2){
			// asguard_dbg("Received Reply with State=retransmission.. rcluster_id=%d, param3=%d, param4=%d\n",rcluster_id, param3, param4);

			if(priv->sm_log.last_idx < param4) {
				asguard_error("(2) match index (%d) is greater than local last idx (%d)!\n", param4, priv->sm_log.last_idx);
				break; // ignore this faulty packet
			}

			if(priv->sm_log.last_idx < param3) {
				asguard_error("(2) follower requested log with invalid index (%d)\n", param3);
				break; // ignore this faulty packet
			}

			/* store start index of entries to be retransmitted.
			 * Will only transmit one packet, receiver may drop entry duplicates.
			 */
			queue_retransmission(priv, remote_lid, param3);

			check_pending_log_rep(sdev);
			// priv->sm_log.match_index[remote_lid] = param4;
			// update_commit_idx(priv);

		} else if(param2 == 0) {
			// append rpc failed!
			// asguard_dbg("Received Reply with State=failed..rcluster_id=%d, param3=%d\n",rcluster_id, param3);

			if(priv->sm_log.last_idx < param3) {
				asguard_error("(0) Received invalid next index from follower (%d)\n", param3);
				break; // ignore this faulty packet
			}

			// decrement nextIndex for follower with <remote_lid>
			priv->sm_log.next_index[remote_lid] = param3 + 1;

		}

		break;
	case ALIVE:

		param1 = GET_CON_PROTO_PARAM1_VAL(pkt);
		/* Received an ALIVE operation from a node that claims to be the new leader
		 */
		if (param1 > priv->term) {
			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, RDTSC_ASGUARD);

#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asguard_dbg("Received message from new leader with higher or equal term=%u\n", param1);
#endif
		}

		/* Ignore other cases for ALIVE operation*/

		break;
	case APPEND:
#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asguard_dbg("received APPEND but node is leader BUG\n");
#endif

		break;
	default:
		asguard_dbg("Unknown opcode received from host: %d - opcode: %d\n", remote_lid, opcode);

	}

	return 0;
}


void clean_request_transmission_lists(struct consensus_priv *priv)
{
	struct retrans_request *entry, *tmp_entry;
	int i;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++) {
		write_lock(&priv->sm_log.retrans_list_lock[i]);
		asguard_dbg("deleting retransmission list for target %d\n", i);

		if(list_empty(&priv->sm_log.retrans_head[i])) {
			write_unlock(&priv->sm_log.retrans_list_lock[i]);
			continue;
		}

		list_for_each_entry_safe(entry, tmp_entry, &priv->sm_log.retrans_head[i], retrans_req_head)
		{
			if(entry) {
				list_del(&entry->retrans_req_head);
				kfree(entry);
			}
			priv->sm_log.retrans_entries[i]--;


		}
		write_unlock(&priv->sm_log.retrans_list_lock[i]);

	}

}
EXPORT_SYMBOL(clean_request_transmission_lists);

void print_leader_stats(struct consensus_priv *priv)
{
	int i;
	struct retrans_request *entry, *tmp_entry;

    asguard_dbg("Bug Counter: %d", priv->sdev->bug_counter);

	for (i = 0; i < priv->sdev->pminfo.num_of_targets; i++){
		asguard_dbg("Stats for target %d \n", i);
		asguard_dbg("\t pkt TX counter: %d\n", priv->sdev->pminfo.pm_targets[i].pkt_tx_counter);
		asguard_dbg("\t scheduled log reps: %d\n", priv->sdev->pminfo.pm_targets[i].scheduled_log_replications);

		asguard_dbg("\t number of retransmissions: %d\n", priv->sm_log.num_retransmissions[i] );
		asguard_dbg("\t match index: %d \n", priv->sm_log.match_index[i] );
		asguard_dbg("\t next index: %d \n", priv->sm_log.match_index[i] );

		asguard_dbg("\t pending retransmissions: \n" );

		read_lock(&priv->sm_log.retrans_list_lock[i]);
		if(!list_empty(&priv->sdev->consensus_priv->sm_log.retrans_head[i])) {
			list_for_each_entry_safe(entry, tmp_entry, &priv->sm_log.retrans_head[i], retrans_req_head)
			{
				asguard_dbg("\t\t %d \n", entry->request_idx);
			}
		} else
			asguard_dbg("\t\t empty \n");

		read_unlock(&priv->sm_log.retrans_list_lock[i]);

	}

	asguard_dbg("last_idx %d \n", priv->sm_log.last_idx );
	asguard_dbg("stable_idx %d\n", priv->sm_log.stable_idx );
	asguard_dbg("next_retrans_req_idx %d\n", priv->sm_log.next_retrans_req_idx );
	asguard_dbg("max_entries %d\n", priv->sm_log.max_entries );

}


int stop_leader(struct proto_instance *ins)
{
	struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;
    int i;

	priv->sdev->is_leader = 0;

	// Stop Current send tasks!

	print_leader_stats(priv);

	clean_request_transmission_lists(priv);

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i ++) {
        async_clear_queue(priv->sdev->pminfo.pm_targets[i].aapriv);
    }

	return 0;
}

int start_leader(struct proto_instance *ins)
{
	struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;

    initialize_indices(priv);

	priv->sdev->is_leader = 1;
	priv->sdev->tx_port = 3320;
	priv->candidate_counter = 0;

	check_pending_log_rep(priv->sdev);

    /* Trigger poll from user space */
    _schedule_update_from_userspace(priv->sdev, priv->synbuf_tx);

    return 0;
}
EXPORT_SYMBOL(start_leader);
