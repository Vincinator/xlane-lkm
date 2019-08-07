#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_consensus_ops.h"


int consensus_init(struct sassy_device *sdev)
{
	// Transition to Follower State
}

int consensus_init_payload(void *payload)
{
	
	return 0;
}

int consensus_start(struct sassy_device *sdev)
{
	sassy_dbg("consensus start\n");
	return 0;
}

int consensus_stop(struct sassy_device *sdev)
{
	sassy_dbg("consensus stop\n");
	return 0;
}

int consensus_clean(struct sassy_device *sdev)
{
	sassy_dbg("consensus clean\n");
	return 0;
}

int consensus_info(struct sassy_device *sdev)
{
	sassy_dbg("consensus info\n");
	return 0;
}


int consensus_us_update(struct sassy_device *sdev, void *payload)
{
	sassy_dbg("consensus update\n");


	return 0;
}

int consensus_post_payload(struct sassy_device *sdev, unsigned char *remote_mac,
		    void *payload)
{

	if (sdev->verbose)
		sassy_dbg("consensus payload received\n");
}

int consensus_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
	       uint64_t ts)
{
	if (sdev->verbose) {
		sassy_dbg("consensus optimistical timestamp received. \n");
	}
}
