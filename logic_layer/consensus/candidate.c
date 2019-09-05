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
	write_log(&sdev->le_logger, CANDIDATE_TIMEOUT, rdtsc());
	
	/* The candidate can not get a majority from the cluster.
	 * Probably less than the required majority of nodes are alive. 
	 * 
	 * Option 1: start the process over and retry as follower
	 * Option 2: become Leader without majority (interim Leader)
	 * Option 3: Unreachable, retry only manually - shutdown leader election.
	 *
	 */
	if(priv->c_retries >= CANDIDATURE_RETRY_LIMIT){

		sassy_log_le("%s, %llu, %d: reached maximum of candidature retries\n",
			nstate_string(priv->nstate),
			rdtsc());

		// (Option 1)
		//node_transition(sdev, FOLLOWER);

		// (Option 2)
		// node_transition(sdev, LEADER);

		// (Option 3)
		le_state_transition_to(sdev, LE_READY);


		return HRTIMER_NORESTART;
	}

	setup_nomination(sdev);

	timeout = get_rnd_timeout_candidate_plus(priv->c_retries * CANDIDATE_RETRY_TIMEOUT_GROWTH);

	hrtimer_forward_now(timer, timeout);

	sassy_log_le("%s, %llu, %d: Restart candidate timer with %lld ms timeout - Candidature retry %d.\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			ktime_to_ms(timeout),
			priv->c_retries);

	return HRTIMER_RESTART;
	
}

void reset_ctimeout(struct sassy_device *sdev)
{
	ktime_t timeout;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->c_retries = 0;
	timeout = get_rnd_timeout_candidate();

	hrtimer_cancel(&priv->ctimer);
	hrtimer_set_expires_range_ns(&priv->ctimer, timeout, TOLERANCE_CTIMEOUT_NS);
	hrtimer_start_expires(&priv->ctimer, HRTIMER_MODE_REL_PINNED);

	sassy_log_le("%s, %llu, %d: Set candidate timeout to %lld ms\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			ktime_to_ms(timeout));
}

void init_ctimeout(struct sassy_device *sdev)
{
	int ftime_ns;
	ktime_t timeout;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
	
	if(priv->ctimer_init == 1) {
		reset_ctimeout(sdev);
		return;
	}

	priv->c_retries = 0;
	timeout = get_rnd_timeout();

	hrtimer_init(&priv->ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ctimer_init = 1;

	priv->ctimer.function = &_handle_candidate_timeout;

	sassy_log_le("%s, %llu, %d: Init Candidate timeout to %lld ms.\n",
		nstate_string(priv->nstate),
		rdtsc(),
		priv->term,
		 ktime_to_ms(timeout));

	hrtimer_start_range_ns(&priv->ctimer, timeout, HRTIMER_MODE_REL_PINNED, TOLERANCE_CTIMEOUT_NS);
}

int setup_nomination(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->term++;
	priv->votes = 1; // start with selfvote

	setup_le_broadcast_msg(sdev, NOMI);
	return 0;
}

void accept_vote(struct sassy_device *sdev, int remote_lid, unsigned char *pkt) 
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
	int err;

	priv->votes++;

	sassy_log_le("%s, %llu, %d: received %d votes for this term. (%d possible total votes)\n",
					nstate_string(priv->nstate),
					rdtsc(),
					priv->term,
					priv->votes,
					sdev->pminfo.num_of_targets + 1);

	write_log(&sdev->le_logger, CANDIDATE_ACCEPT_VOTE, rdtsc());

	if (priv->votes * 2 >= (sdev->pminfo.num_of_targets + 1)) {
		
		sassy_log_le("%s, %llu, %d: got majority with %d from %d possible votes \n",
				nstate_string(priv->nstate),
				rdtsc(),
				priv->term,
				priv->votes,
				sdev->pminfo.num_of_targets);

		err = node_transition(sdev, LEADER);
		write_log(&sdev->le_logger, CANDIDATE_BECOME_LEADER, rdtsc());

		if (err) {
			sassy_error("Error occured during the transition to leader role\n");
			return;
		}
	}else {
		reset_ctimeout(sdev);
	}

}

int candidate_process_pkt(struct sassy_device *sdev, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	u8 opcode = GET_LE_PAYLOAD(pkt, opcode);
	u32 param1 = GET_LE_PAYLOAD(pkt, param1);
	u32 param2 = GET_LE_PAYLOAD(pkt, param2);
	log_le_rx(sdev->verbose, priv->nstate, rdtsc(), priv->term, opcode, rcluster_id, param1);

	switch(opcode){
	case VOTE:
		accept_vote(sdev, remote_lid, pkt);
		break;
	case NOMI:

		// Nomination from Node with higher term - cancel own candidature and vote for higher term
		if(param1 > priv->term){
			node_transition(sdev, FOLLOWER);
			reply_vote(sdev, remote_lid, rcluster_id, param1, param2);
		}

		break;		
	case NOOP:
		break;
	case LEAD:
		if(param1 >= priv->term){

			if(sdev->verbose >= 2)
				sassy_dbg("Received message from new leader with higher or equal term=%u\n", param1);

			accept_leader(sdev, remote_lid, rcluster_id, param1);
			write_log(&sdev->le_logger, CANDIDATE_ACCEPT_NEW_LEADER, rdtsc());

		} else {

			if(sdev->verbose >= 2)
				sassy_dbg("Received LEAD from leader with lower term=%u\n", param1);
	
			// Ignore this LEAD message, continue to wait for votes 
	
		}
	
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n", remote_lid, opcode);
	}

	return 0;
}

int stop_candidate(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	if(priv->ctimer_init == 0)
		return 0;

	priv->ctimer_init = 0;

	return hrtimer_cancel(&priv->ctimer) == 1;
}

int start_candidate(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->votes = 0;
	priv->nstate = CANDIDATE;

	sassy_dbg("Initialization finished.\n");

	setup_nomination(sdev);
	init_ctimeout(sdev);

	sassy_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


