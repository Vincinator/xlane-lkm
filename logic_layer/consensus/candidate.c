#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include "include/candidate.h"
#include "include/sassy_consensus.h"

static enum hrtimer_restart _handle_candidate_timeout(struct hrtimer *timer)
{
	sassy_dbg("Candidate Timeout occured - starting new nomination broadcast\n");
	//broadcast_nomination();
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

void set_le_opcode(struct sassy_payload *pkt_payload, enum le_opcode opcode, int p1, int p2)
{
	pkt_payload->lep.opcode = opcode;
	pkt_payload->lep.param1 = p1;
	pkt_payload->lep.param2 = p2;
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

		set_le_opcode(pkt_payload, NOMI, priv->term, priv->node_id);

		spminfo->pm_targets[i].pkt_data.hb_active_ix = hb_passive_ix;
	}

	priv->votes = 1; // selfvote

	init_ctimeout();

	return 0;
}


int candidate_process_pkt(struct sassy_device *sdev, struct sassy_payload * pkt)
{

	switch(pkt->lep.opcode){
		case VOTE:
			sassy_dbg("received vote! term=%d\n", pkt->lep.param1);
			break;
		case NOMI:
			sassy_dbg("received nomination! term=%d\n", pkt->lep.param1);
			break;		
		case NOOP:
			sassy_dbg("received NOOP! term=%d\n", pkt->lep.param1);
			break;
		default:
			sassy_dbg("Unknown opcode received: %d\n", pkt->lep.opcode);
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
	int i;

	priv->nstate = CANDIDATE;
	priv->term++;

	sassy_dbg("Initialization finished.\n");

	broadcast_nomination(sdev);

	sassy_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


