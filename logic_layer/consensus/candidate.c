#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include "include/candidate.h"
#include "include/sassy_consensus.h"


static struct sk_buff **nom_broad_skbs;

struct nomination_pkt_data *setup_broadcast_payload(void) {
	struct nomination_pkt_data *payload;
	struct consensus_priv *priv = con_priv();

	sassy_dbg("Composing Candidate Payload.\n");

	payload = (struct nomination_pkt_data *) kzalloc(sizeof(struct nomination_pkt_data),GFP_KERNEL);

	payload->candidate_id = priv->node_id;
	payload->term = priv->term;
	payload->msg_type = NOMINATION;

	sassy_dbg("Candidate Payload Composed.\n");

	return payload;
}

static enum hrtimer_restart _handle_candidate_timeout(struct hrtimer *timer)
{
	sassy_dbg("Candidate Timeout occured - starting new nomination broadcast\n");
	//broadcast_nomination();
	return HRTIMER_NORESTART;
}

void init_ctimeout(void)
{
	int ftime_ns;
	ktime_t timeout;
	struct consensus_priv *priv = con_priv();
	
	sassy_dbg("Initializing candidate timeout \n");

	timeout = get_rnd_timeout();

	hrtimer_init(&priv->ctimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ctimer_init = 1;
	priv->ctimer.function = &_handle_candidate_timeout;

	hrtimer_start(&priv->ctimer, timeout, HRTIMER_MODE_REL_PINNED);
	sassy_dbg("candidate timeout initialized and started\n");

}




int broadcast_nomination(void)
{
	void *payload;
	struct consensus_priv *priv = con_priv();
	struct node_addr *naddr;
	struct pminfo *spminfo = &priv->sdev->pminfo;
	int i;

	sassy_dbg("broadcast of self nomination started.\n");

	payload = (void *) setup_broadcast_payload();

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++) {
		naddr = &spminfo->pm_targets[i].pkt_data.naddr;
		nom_broad_skbs[i] = compose_skb(priv->sdev, naddr, payload);	
		if(!nom_broad_skbs[i]) {
			sassy_error("composing error detected\n");
		}
	}
	sassy_dbg("starting broadcast.\n");
	send_pkts(priv->sdev, nom_broad_skbs, priv->sdev->pminfo.num_of_targets);
	sassy_dbg("broadcast done.\n");

	priv->votes = 1; // selfvote

	init_ctimeout();
	sassy_dbg("broadcast of self nomination finished.\n");


	return 0;
}


int candidate_process_pkt(struct sassy_device *sdev, void* pkt)
{

	return 0;
}

int stop_candidate(void)
{
	struct consensus_priv *priv = con_priv();

	if(priv->ctimer_init == 0)
		return 0;
	priv->ctimer_init = 0;
	return hrtimer_try_to_cancel(&priv->ctimer) != -1;
}

int start_candidate(void)
{
	struct consensus_priv *priv = con_priv();
	int i;

	if(nom_broad_skbs){
		sassy_dbg("Free old skbs\n");
		for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++)
			if(nom_broad_skbs[i])
				kfree(nom_broad_skbs[i]);
	}

	nom_broad_skbs = kmalloc_array(priv->sdev->pminfo.num_of_targets,
			      				   sizeof(struct sk_buff *), GFP_KERNEL);

	for (i = 0; i < priv->sdev->pminfo.num_of_targets; i++)
		nom_broad_skbs[i] = kmalloc(sizeof(struct sk_buff), GFP_KERNEL);

	priv->nstate = CANDIDATE;
	priv->term++;

	sassy_dbg("Initialization finished.\n");

	// Broadcast nomination
	broadcast_nomination();

	sassy_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


