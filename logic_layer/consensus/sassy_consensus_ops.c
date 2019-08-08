#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"


int consensus_init(struct sassy_device *sdev)
{
	int err;

	// Transition to Follower State
	err = node_transition(sdev, FOLLOWER);

	if (err)
		goto error;

	return 0;

error:
	sassy_error(" %s failed\n", __FUNCTION__);
	return err;
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
	struct sassy_protocol *sproto = sdev->proto;
	const struct consensus_priv *priv =
		(const struct consensus_priv *)sproto->priv;

	if (sdev->verbose)
		sassy_dbg("consensus payload received\n");

	switch (priv->nstate) {
	case FOLLOWER:
		follower_process_pkt(sdev, payload);
		break;
	case CANDIDATE:
		candidate_process_pkt(sdev, payload);
		break;
	case LEADER:
		leader_process_pkt(sdev, payload);
		break;
	default:
		sassy_error("Unknown state - BUG\n");
		break;
	}
}

int consensus_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
	       uint64_t ts)
{
	if (sdev->verbose)
		sassy_dbg("consensus optimistical timestamp received.\n");
}
