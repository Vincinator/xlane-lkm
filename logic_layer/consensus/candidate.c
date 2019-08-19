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

	sassy_dbg("Initialization finished.\n");

	// Broadcast nomination
	broadcast_nomination();

	sassy_dbg("Candidate started.\n");

	return 0;
}
EXPORT_SYMBOL(start_candidate);


