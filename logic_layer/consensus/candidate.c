#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>

#include "include/candidate.h"
#include "include/sassy_consensus.h"


int broadcast_nomination(void)
{
	//compose_skb();
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


