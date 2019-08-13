#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include "include/candidate.h"
#include "include/sassy_consensus.h"


static struct sk_buff *nom_broad_skbs[MAX_NODE_ID];

struct nomination_pkt_data *setup_broadcast_payload(void) {
	struct nomination_pkt_data *payload;
	struct consensus_priv *priv = con_priv();

	payload = (struct nomination_pkt_data *) kmalloc(sizeof(struct nomination_pkt_data),GFP_KERNEL);

	payload->candidate_id = priv->node_id;
	payload->term = priv->term;
	payload->msg_type = NOMINATION;

	return payload;
}


int broadcast_nomination(void)
{
	void *payload;
	struct consensus_priv *priv = con_priv();
	struct node_addr *naddr;
	struct pminfo *spminfo = &priv->sdev->pminfo;
	int i;

	payload = (void *) setup_broadcast_payload();

	for(i = 0; i < MAX_NODE_ID; i++) {
		naddr = &spminfo->pm_targets[i].pkt_data.naddr;
		nom_broad_skbs[i] = compose_skb(priv->sdev, naddr, payload);
	}

	send_pkts(priv->sdev, nom_broad_skbs, priv->sdev->pminfo.num_of_targets);

	priv->votes = 1; // selfvote

	// start timeout

	return 0;
}


int candidate_process_pkt(struct sassy_device *sdev, void* pkt)
{

	return 0;
}

int stop_candidate(void)
{

	return 0;
}

int start_candidate(void)
{
	struct consensus_priv *priv = con_priv();

	priv->nstate = CANDIDATE;
	priv->term++;

	// Broadcast nomination
	broadcast_nomination();

	
	return 0;
}
EXPORT_SYMBOL(start_candidate);


