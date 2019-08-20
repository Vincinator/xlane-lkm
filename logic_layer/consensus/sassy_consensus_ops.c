#include <sassy/logger.h>
#include <sassy/sassy.h>


#include "include/sassy_consensus.h"
#include "include/sassy_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"


int consensus_init(struct sassy_device *sdev)
{
	
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->sdev = sdev;
	priv->ctimer_init = 0;
	priv->ftimer_init = 0;

	return 0;
}

int consensus_init_payload(struct sassy_payload *payload)
{

	return 0;
}

int consensus_start(struct sassy_device *sdev)
{
	int err;

	sassy_dbg("consensus start\n");

	// Transition to Follower State
	err = node_transition(sdev, FOLLOWER);

	if (err)
		goto error;

	return 0;

error:
	sassy_error(" %s failed\n", __FUNCTION__);
	return err;
}

int consensus_stop(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;	
	sassy_dbg("consensus stop\n");

	switch(priv->nstate) {
		case FOLLOWER:
			stop_follower(sdev);
			break;
		case CANDIDATE:
			stop_candidate(sdev);
			break;
		case LEADER:
			stop_leader(sdev);
			break;
	}

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
	struct sassy_protocol *sproto = sdev->le_proto;
	const struct consensus_priv *priv =
		(const struct consensus_priv *)sproto->priv;
	int remote_lid;

	if (sdev->verbose >= 3)
			sassy_dbg("consensus payload received\n");

	remote_lid = get_ltarget_id(sdev, remote_mac);

	if(remote_lid == -1){
		if(sdev->verbose >= 2)
			sassy_error("Unknown host! Abort processing of packet\n");
		return -1;
	}

	switch (priv->nstate) {
	case FOLLOWER:
		follower_process_pkt(sdev, remote_lid, (struct sassy_payload *) payload);
		break;
	case CANDIDATE:
		candidate_process_pkt(sdev, remote_lid, (struct sassy_payload *) payload);
		break;
	case LEADER:
		leader_process_pkt(sdev, remote_lid, (struct sassy_payload *) payload);
		break;
	default:
		sassy_error("Unknown state - BUG\n");
		break;
	}
	return 0;
}

int consensus_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
	       uint64_t ts)
{
	if (sdev->verbose >= 3)
		sassy_dbg("consensus optimistical timestamp received.\n");
}
