#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include <sassy/payload_helper.h>

#include "include/follower.h"
#include "include/candidate.h"
#include "include/sassy_consensus.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][CANDIDATE]"

#define CANDIDATURE_RETRY_LIMIT 100

/* Factor the retry timeout grows after each retry
 * Factor grows until:
 * CANDIDATURE_RETRY_LIMIT * CANDIDATE_RETRY_TIMEOUT_GROWTH
 */
#define CANDIDATE_RETRY_TIMEOUT_GROWTH 20

static enum hrtimer_restart _handle_candidate_timeout(struct hrtimer *timer)
{
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ctimer);
	struct sassy_device *sdev = priv->sdev;
	ktime_t timeout;

	if(priv->ctimer_init == 0 || priv->nstate != CANDIDATE)
		return HRTIMER_NORESTART;

	priv->c_retries++;
	write_log(&priv->ins->logger, CANDIDATE_TIMEOUT, rdtsc());
	
	/* The candidate can not get a majority from the cluster.
	 * Probably less than the required majority of nodes are alive. 
	 * 
	 * Option 1: start the process over and retry as follower
	 * Option 2: become Leader without majority (interim Leader)
	 * Option 3: Unreachable, retry only manually - shutdown leader election.
	 *
	 */
	if(priv->c_retries >= CANDIDATURE_RETRY_LIMIT){
#if 0
		sassy_log_le("%s, %llu, %d: reached maximum of candidature retries\n",
			nstate_string(priv->nstate),
			rdtsc());
#endif
		// (Option 1)
		//node_transition(sdev, FOLLOWER);

		// (Option 2)
		// node_transition(sdev, LEADER);

		// (Option 3)
		le_state_transition_to(priv, LE_READY);


		return HRTIMER_NORESTART;
	}

	setup_nomination(sdev);

	timeout = get_rnd_timeout(priv->c_retries * priv->ct_min, priv->c_retries * priv->ct_max);

	hrtimer_forward_now(timer, timeout);
#if 0
	sassy_log_le("%s, %llu, %d: Restart candidate timer with %lld ms timeout - Candidature retry %d.\n",
			nstate_string(priv->nstate),
			rdtsc(),
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
	timeout = get_rnd_timeout(priv->ct_min, priv->ct_max);

	hrtimer_cancel(&priv->ctimer);
	hrtimer_set_expires_range_ns(&priv->ctimer, timeout, TOLERANCE_CTIMEOUT_NS);
	hrtimer_start_expires(&priv->ctimer, HRTIMER_MODE_REL_PINNED);
#if 0
	sassy_log_le("%s, %llu, %d: Set candidate timeout to %lld ms\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			ktime_to_ms(timeout));
#endif

}

void init_ctimeout(struct proto_instance *ins)
{
	int ftime_ns;
	ktime_t timeout;

	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
	
	if(priv->ctimer_init == 1) {
		reset_ctimeout(sdev);
		return;
	}

	priv->c_retries = 0;
	timeout = get_rnd_timeout(priv->ct_min, priv->ct_max);

	hrtimer_init(&priv->ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ctimer_init = 1;

	priv->ctimer.function = &_handle_candidate_timeout;

#if 0
	sassy_log_le("%s, %llu, %d: Init Candidate timeout to %lld ms.\n",
		nstate_string(priv->nstate),
		rdtsc(),
		priv->term,
		 ktime_to_ms(timeout));
#endif

	hrtimer_start_range_ns(&priv->ctimer, timeout, HRTIMER_MODE_REL_PINNED, TOLERANCE_CTIMEOUT_NS);
}

int setup_nomination(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	priv->term++;
	priv->votes = 1; // start with selfvote

	setup_le_broadcast_msg(ins, NOMI);
	return 0;
}

void accept_vote(struct proto_instance *ins, int remote_lid, unsigned char *pkt) 
{
	int err;

	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	priv->votes++;

#if 0
	sassy_log_le("%s, %llu, %d: received %d votes for this term. (%d possible total votes)\n",
					nstate_string(priv->nstate),
					rdtsc(),
					priv->term,
					priv->votes,
					sdev->pminfo.num_of_targets + 1);
#endif

	write_log(&ins->logger, CANDIDATE_ACCEPT_VOTE, rdtsc());

	if (priv->votes * 2 >= (priv->sdev->pminfo.num_of_targets + 1)) {

#if 0
		sassy_log_le("%s, %llu, %d: got majority with %d from %d possible votes \n",
				nstate_string(priv->nstate),
				rdtsc(),
				priv->term,
				priv->votes,
				sdev->pminfo.num_of_targets);
#endif

		err = node_transition(ins, LEADER);
		write_log(&ins->logger, CANDIDATE_BECOME_LEADER, rdtsc());

		if (err) {
			sassy_error("Error occured during the transition to leader role\n");
			return;
		}
	}else {
		reset_ctimeout(priv);
	}

}

int candidate_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	u8 opcode = GET_LE_PAYLOAD(pkt, opcode);
	u32 param1 = GET_LE_PAYLOAD(pkt, param1);
	u32 param2 = GET_LE_PAYLOAD(pkt, param2);
	
#if 0

	log_le_rx(sdev->verbose, priv->nstate, rdtsc(), priv->term, opcode, rcluster_id, param1);
#endif
	switch(opcode){
	case VOTE:
		accept_vote(ins, remote_lid, pkt);
		break;
	case NOMI:

		// Nomination from Node with higher term - cancel own candidature and vote for higher term
		if(param1 > priv->term){
			node_transition(ins, FOLLOWER);
			reply_vote(ins, remote_lid, rcluster_id, param1, param2);
		}

		break;		
	case NOOP:
		break;
	case LEAD:
		if(param1 >= priv->term){

#if 0
			if(sdev->verbose >= 2)
				sassy_dbg("Received message from new leader with higher or equal term=%u\n", param1);
#endif
			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, CANDIDATE_ACCEPT_NEW_LEADER, rdtsc());

		} else {
#if 0

			if(sdev->verbose >= 2)
				sassy_dbg("Received LEAD from leader with lower term=%u\n", param1);
#endif
			// Ignore this LEAD message, continue to wait for votes 
	
		}
	
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n", remote_lid, opcode);
	}

	return 0;
}

int stop_candidate(struct proto_instance *ins)
{
	
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	if(priv->ctimer_init == 0)
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

	sassy_dbg("Initialization finished.\n");

	setup_nomination(ins);
	init_ctimeout(ins);

	sassy_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


