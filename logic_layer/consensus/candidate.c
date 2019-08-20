#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include "include/candidate.h"
#include "include/sassy_consensus.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][CANDIDATE]"

static enum hrtimer_restart _handle_candidate_timeout(struct hrtimer *timer)
{
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ftimer);
	struct sassy_device *sdev = priv->sdev;

	sassy_dbg("Candidate Timeout occured - starting new nomination broadcast\n");
	
	broadcast_nomination(sdev);

	return HRTIMER_NORESTART;
}

void init_ctimeout(struct sassy_device *sdev)
{
	int ftime_ns;
	ktime_t timeout;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
	
	sassy_dbg("Initializing candidate timeout \n");

	timeout = get_rnd_timeout();

	hrtimer_init(&priv->ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ctimer_init = 1;
	priv->ctimer.function = &_handle_candidate_timeout;

	hrtimer_start(&priv->ctimer, timeout, HRTIMER_MODE_REL_PINNED);
	sassy_dbg("candidate timeout initialized and started\n");

}

int broadcast_nomination(struct sassy_device *sdev)
{
	void *payload;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
	struct node_addr *naddr;
	struct pminfo *spminfo = &priv->sdev->pminfo;
	int i;
	struct sassy_payload *pkt_payload;
	int hb_passive_ix;

	sassy_dbg("Preparing broadcast of nomination for next hb interval.\n");

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++) {

		hb_passive_ix =
		     !!!spminfo->pm_targets[i].pkt_data.hb_active_ix;

		pkt_payload =
	     	spminfo->pm_targets[i].pkt_data.pkt_payload[hb_passive_ix];

		set_le_opcode(pkt_payload, NOMI, priv->term, sdev->cluster_id);
		
		if(sdev->verbose)
			print_hex_dump(KERN_DEBUG, "NOMI payload: ", DUMP_PREFIX_NONE, 16, 1, 
				pkt_payload, SASSY_PAYLOAD_BYTES, 0);

		spminfo->pm_targets[i].pkt_data.hb_active_ix = hb_passive_ix;
	}

	priv->votes = 1; // selfvote

	init_ctimeout(sdev);

	return 0;
}
void accept_vote(struct sassy_device *sdev, int remote_lid, struct sassy_payload * pkt) 
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->votes++;
	sassy_dbg("Got Vote/n");

	if (priv->votes * 2 >= sdev->pminfo.num_of_targets){
		
		sassy_dbg("Got Majority of votes, transition to become the new leader/n");
		
		err = node_transition(sdev, LEADER);

		if (err) {
			sassy_dbg("Error occured during the transition to leader role\n");
			return;
		}
	}

}


int candidate_process_pkt(struct sassy_device *sdev, int remote_lid, struct sassy_payload * pkt)
{
	if(sdev->verbose >= 2)
		sassy_dbg("received packet to process\n");

	switch(pkt->lep.opcode){
	case VOTE:
		sassy_dbg("received VOTE from host: %d - term=%u \n",remote_lid, pkt->lep.param1);
		accept_vote(sdev, remote_lid, pkt);
		break;
	case NOMI:
		sassy_dbg("received NOMI from host: %d - term=%u \n",remote_lid, pkt->lep.param1);
		break;		
	case NOOP:
		if(sdev->verbose >= 2)
			sassy_dbg("received NOOP from host: %d - term=%u \n", remote_lid, pkt->lep.param1);
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n",remote_lid, pkt->lep.opcode);
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
	return hrtimer_try_to_cancel(&priv->ctimer) != -1;
}

int start_candidate(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->votes = 0;
	priv->nstate = CANDIDATE;
	priv->term++;

	sassy_dbg("Initialization finished.\n");

	broadcast_nomination(sdev);

	sassy_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


