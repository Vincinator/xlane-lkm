#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include <sassy/payload_helper.h>

#include "include/candidate.h"
#include "include/sassy_consensus.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][CANDIDATE]"

static enum hrtimer_restart _handle_candidate_timeout(struct hrtimer *timer)
{
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ftimer);
	struct sassy_device *sdev = priv->sdev;

	if(priv->ctimer_init == 0 || priv->nstate != CANDIDATE)
		return HRTIMER_NORESTART;

	write_le_log(sdev, CANDIDATE_TIMEOUT, rdtsc());


	sassy_dbg("Candidate Timeout occured - starting new nomination broadcast\n");
	
	sassy_log_le("%s, %llu, %d: Follower timeout occured - starting candidature\n",
				nstate_string(priv->nstate),
				rdtsc(),
				priv->term);

	setup_nomination(sdev);

	return HRTIMER_NORESTART;
}

void reset_ctimeout(struct sassy_device *sdev)
{
	ktime_t now;
	ktime_t timeout;
	s64 delta;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	timeout = get_rnd_timeout();
	delta = ktime_to_ms(timeout);

	hrtimer_cancel(&priv->ctimer);
	hrtimer_set_expires(&priv->ctimer, timeout);
	hrtimer_start_expires(&priv->ctimer, HRTIMER_MODE_REL_PINNED);

	sassy_log_le("%s, %llu, %d: Set candidate timeout to %lld ms\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			delta);
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

	sassy_dbg("Initializing candidate timeout \n");

	timeout = get_rnd_timeout();

	hrtimer_init(&priv->ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ctimer_init = 1;
	priv->ctimer.function = &_handle_candidate_timeout;

	hrtimer_start(&priv->ctimer, timeout, HRTIMER_MODE_REL_PINNED);
	sassy_dbg("candidate timeout initialized and started\n");

}

int setup_nomination(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->term++;
	priv->votes = 1; // start with selfvote

	setup_le_broadcast_msg(sdev, NOMI);

	init_ctimeout(sdev);

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

	if (priv->votes * 2 >= (sdev->pminfo.num_of_targets + 1)) {
		
		sassy_log_le("%s, %llu, %d: got majority with %d from %d possible votes \n",
				nstate_string(priv->nstate),
				rdtsc(),
				priv->term,
				priv->votes,
				sdev->pminfo.num_of_targets);

		err = node_transition(sdev, LEADER);
		write_le_log(sdev, CANDIDATE_BECOME_LEADER, rdtsc());


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
		break;		
	case NOOP:
		break;
	case LEAD:
		if(param1 >= priv->term){

			if(sdev->verbose >= 2)
				sassy_dbg("Received message from new leader with higher or equal term=%u\n", param1);

			accept_leader(sdev, remote_lid, rcluster_id, param1);
			write_le_log(sdev, CANDIDATE_ACCEPT_NEW_LEADER, rdtsc());


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

	sassy_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


