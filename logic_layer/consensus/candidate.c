#include <sassy/logger.h>
#include <sassy/sassy.h>


#include "include/candidate.h"

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
	
	return 0;
}
EXPORT_SYMBOL(start_candidate);


