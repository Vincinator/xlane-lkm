#include <asguard/logger.h>
#include <asguard/asguard.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include <asguard/payload_helper.h>

#include "include/consensus_helper.h"
#include "include/follower.h"
#include "include/candidate.h"
#include <asguard/consensus.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][LE][CANDIDATE]"

#define CANDIDATURE_RETRY_LIMIT 100

/* Factor the retry timeout grows after each retry
 * Factor grows until:
 * CANDIDATURE_RETRY_LIMIT * CANDIDATE_RETRY_TIMEOUT_GROWTH
 */
#define CANDIDATE_RETRY_TIMEOUT_GROWTH 20



int setup_nomination(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;
	int i;

	priv->term++;
	//priv->votes = 1; // start with selfvote

	setup_le_broadcast_msg(ins, NOMI);


	sdev->hold_fire = 1;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++)
		priv->sdev->pminfo.pm_targets[i].fire = 1;

	sdev->hold_fire = 0;

	return 0;
}

void accept_vote(struct proto_instance *ins, int remote_lid, unsigned char *pkt)
{
	int err;

	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	priv->votes++;
	write_log(&ins->logger, CANDIDATE_ACCEPT_VOTE, RDTSC_ASGUARD);

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: received %d votes for this term. (%d possible total votes)\n",
					nstate_string(priv->nstate),
					RDTSC_ASGUARD,
					priv->term,
					priv->votes,
					priv->sdev->pminfo.num_of_targets);
#endif


	if (priv->votes == priv->sdev->pminfo.num_of_targets ) {

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: got majority with %d from %d possible votes\n",
				nstate_string(priv->nstate),
				RDTSC_ASGUARD,
				priv->term,
				priv->votes,
				priv->sdev->pminfo.num_of_targets);
#endif

		// DEBUG: Check if Leader code has an issue..
		err = node_transition(ins, LEADER);
		write_log(&ins->logger, CANDIDATE_BECOME_LEADER, RDTSC_ASGUARD);
		priv->accu_rand = 0;
		priv->sdev->cur_leader_lid = -1;

		if (err) {
			asguard_error("Error occured during the transition to leader role\n");

			return;
		}
	} else {
		// TODO: reset candidature interval counter after every vote?
		//       .... Or count the total intervals required for majority?
		priv->candidate_counter = 0;
	}

}

int candidate_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
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
	case VOTE:

		mutex_lock(&priv->accept_vote_lock);
		accept_vote(ins, remote_lid, pkt);
		mutex_unlock(&priv->accept_vote_lock);

		break;
	case NOMI:
		// param1 interpreted as term
		// param2 interpreted as candidateID
		// param3 interpreted as lastLogIndex
		// param4 interpreted as lastLogTerm

		param2 = GET_CON_PROTO_PARAM2_VAL(pkt);
		param3 = GET_CON_PROTO_PARAM3_VAL(pkt);
		param4 = GET_CON_PROTO_PARAM4_VAL(pkt);

		if (check_handle_nomination(priv, param1, param2, param3, param4, rcluster_id, remote_lid)) {
			node_transition(ins, FOLLOWER);
			reply_vote(ins, remote_lid, rcluster_id, param1, param2);
		}

		break;
	case NOOP:
		break;
	case ALIVE:
		/* Received an ALIVE operation from a node that claims to be the new leader
		 */
		if (param1 >= priv->term) {

#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asguard_dbg("Received message from new leader (%d) with higher or equal term=%u\n",
							rcluster_id,  param1);
#endif
			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, CANDIDATE_ACCEPT_NEW_LEADER, RDTSC_ASGUARD);

		}

		/* Ignore other cases for ALIVE operation*/
		break;
	case APPEND:

		if(priv->leader_id != rcluster_id) {
			// asguard_error("received APPEND from a node that is not accepted as leader \n");
			break;
		}

		if (param1 >= priv->term) {
			_handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);
			commit_log(priv);
		} else {
#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asguard_dbg("Received LEAD from leader (%d) with lower term=%u\n", rcluster_id, param1);
#endif
			// Ignore this LEAD message, continue to wait for votes

		}

		break;
	default:
		asguard_dbg("Unknown opcode received from host: %d - opcode: %d\n", remote_lid, opcode);
	}

	return 0;
}

int stop_candidate(struct proto_instance *ins)
{

	// struct consensus_priv *priv =
	// 	(struct consensus_priv *)ins->proto_data;

	return 0;
}

int start_candidate(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	priv->votes = 0;
	priv->nstate = CANDIDATE;
	priv->candidate_counter = 0;
	priv->sdev->is_leader = 0;

	priv->sdev->tx_port = 3319;

	setup_nomination(ins);


	return 0;
}
EXPORT_SYMBOL(start_candidate);


