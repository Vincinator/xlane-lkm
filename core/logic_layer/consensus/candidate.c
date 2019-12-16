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

static enum hrtimer_restart _handle_candidate_timeout(struct hrtimer *timer)
{
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ctimer);
	ktime_t timeout;

	if (priv->ctimer_init == 0 || priv->nstate != CANDIDATE)
		return HRTIMER_NORESTART;

	priv->c_retries++;
	write_log(&priv->ins->logger, CANDIDATE_TIMEOUT, RDTSC_ASGUARD);

	/* The candidate can not get a majority from the cluster.
	 * Probably less than the required majority of nodes are alive.
	 *
	 * Option 1: start the process over and retry as follower
	 * Option 2: become Leader without majority (interim Leader)
	 * Option 3: Unreachable, retry only manually - shutdown leader election.
	 *
	 */
	if (priv->c_retries >= CANDIDATURE_RETRY_LIMIT) {
#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu: reached maximum of candidature retries\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD);
#endif
		// (Option 1)
		//node_transition(sdev, FOLLOWER);

		// (Option 2)
		// node_transition(sdev, LEADER);

		// (Option 3)
		le_state_transition_to(priv, LE_READY);


		return HRTIMER_NORESTART;
	}

	setup_nomination(priv->ins);

	timeout = ktime_set(0, priv->c_retries * priv->ct_min);
	// ... get_rnd_timeout(priv->c_retries * priv->ct_min, priv->c_retries * priv->ct_max);

	hrtimer_forward_now(timer, timeout);
#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: Restart candidate timer with %lld ms timeout - Candidature retry %d.\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term,
			ktime_to_ms(timeout),
			priv->c_retries);
#endif

	return HRTIMER_RESTART;

}

void reset_ctimeout(struct proto_instance *ins)
{
	ktime_t timeout;
	struct consensus_priv *priv =
			(struct consensus_priv *)ins->proto_data;

	priv->c_retries = 0;
	timeout = ktime_set(0, priv->ct_min);
	// 	timeout = get_rnd_timeout(priv->ct_min, priv->ct_max);

	hrtimer_cancel(&priv->ctimer);
	hrtimer_set_expires_range_ns(&priv->ctimer, timeout, TOLERANCE_CTIMEOUT_NS);
	hrtimer_start_expires(&priv->ctimer, HRTIMER_MODE_REL_PINNED);
#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: Set candidate timeout to %lld ms\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term,
			ktime_to_ms(timeout));
#endif

}

void init_ctimeout(struct proto_instance *ins)
{
	ktime_t timeout;

	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	if (priv->ctimer_init == 1) {
		reset_ctimeout(ins);
		return;
	}

	priv->c_retries = 0;
	timeout = ktime_set(0, priv->ct_min);
	// 	timeout = get_rnd_timeout(priv->ct_min, priv->ct_max);

	hrtimer_init(&priv->ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ctimer_init = 1;

	priv->ctimer.function = &_handle_candidate_timeout;

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: Init Candidate timeout to %lld ms.\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term,
			ktime_to_ms(timeout));
#endif

	hrtimer_start_range_ns(&priv->ctimer, timeout, TOLERANCE_CTIMEOUT_NS, HRTIMER_MODE_REL_PINNED);
}

int setup_nomination(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	priv->term++;
	priv->votes = 1; // start with selfvote

	setup_le_broadcast_msg(ins, NOMI);
	priv->sdev->fire = 1;
	return 0;
}

void accept_vote(struct proto_instance *ins, int remote_lid, unsigned char *pkt)
{
	int err;

	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	priv->votes++;

#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: received %d votes for this term. (%d possible total votes)\n",
					nstate_string(priv->nstate),
					RDTSC_ASGUARD,
					priv->term,
					priv->votes,
					priv->sdev->pminfo.num_of_targets + 1);
#endif

	write_log(&ins->logger, CANDIDATE_ACCEPT_VOTE, RDTSC_ASGUARD);

	if (priv->votes * 2 >= (priv->sdev->pminfo.num_of_targets + 1)) {

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
		reset_ctimeout(ins);
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
		accept_vote(ins, remote_lid, pkt);
		break;
	case NOMI:
		// param1 interpreted as term
		// param2 interpreted as candidateID
		// param3 interpreted as lastLogIndex
		// param4 interpreted as lastLogTerm

		param2 = GET_CON_PROTO_PARAM2_VAL(pkt);
		param3 = GET_CON_PROTO_PARAM3_VAL(pkt);
		param4 = GET_CON_PROTO_PARAM4_VAL(pkt);

		if (check_handle_nomination(priv, param1, param2, param3, param4)) {
			node_transition(ins, FOLLOWER);
			reply_vote(ins, remote_lid, rcluster_id, param1, param2);
		}

		break;
	case NOOP:
		break;
	case APPEND:

		if (param1 >= priv->term) {

#if VERBOSE_DEBUG
			if (sdev->verbose >= 2)
				asguard_dbg("Received message from new leader (%d) with higher or equal term=%u\n",
							rcluster_id,  param1);
#endif
			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, CANDIDATE_ACCEPT_NEW_LEADER, RDTSC_ASGUARD);

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

	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	if (priv->ctimer_init == 0)
		return 0;

	priv->ctimer_init = 0;

	return hrtimer_cancel(&priv->ctimer) == 1;
}

int start_candidate(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	priv->votes = 0;
	priv->nstate = CANDIDATE;

	asguard_dbg("Initialization finished.\n");

	priv->sdev->tx_port = 3319;

	setup_nomination(ins);
	init_ctimeout(ins);

	asguard_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


