#include <sassy/logger.h>
#include <sassy/sassy.h>


#include "include/sassy_consensus.h"
#include "include/sassy_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"

// Default Values for timeouts
#define MIN_FTIMEOUT_NS 10000000
#define MAX_FTIMEOUT_NS 20000000
#define MIN_CTIMEOUT_NS 20000000
#define MAX_CTIMEOUT_NS 40000000


int consensus_init(struct sassy_device *sdev)
{
	
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	priv->sdev = sdev;
	priv->ctimer_init = 0;
	priv->ftimer_init = 0;
	priv->voted = -1;
	priv->term = 0;
	priv->warms = 0;
	priv->warmup_state = WARMING_UP;
	priv->state = LE_RUNNING;
	priv->ft_min = MIN_FTIMEOUT_NS;
	priv->ft_max = MAX_FTIMEOUT_NS;
	priv->ct_min = MIN_CTIMEOUT_NS;
	priv->ct_max = MAX_CTIMEOUT_NS;

	init_le_config_ctrl_interfaces(sdev);

	return 0;
}

int consensus_init_payload(struct sassy_payload *payload)
{

	return 0;
}

int consensus_start(struct sassy_device *sdev)
{
	int err;
	struct consensus_priv *priv = (struct consensus_priv *) sdev->le_proto->priv;

	if(consensus_is_alive(sdev)){
		sassy_dbg("Consensus is already running!\n");
		return 0;
	}

	if (err)
		goto error;

	le_state_transition_to(sdev, LE_RUNNING);

	return 0;

error:
	sassy_error(" %s failed\n", __FUNCTION__);
	return err;
}

int consensus_stop(struct sassy_device *sdev)
{
	struct consensus_priv *priv;	


	if(!consensus_is_alive(sdev))
		return 0;

	priv = (struct consensus_priv *)sdev->le_proto->priv;

	if(!priv)
		return 0;
	
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

	le_state_transition_to(sdev, LE_READY);
	priv->warmup_state = WARMING_UP;
	priv->warms = 0;
	set_all_targets_dead(sdev);

	return 0;
}

int consensus_clean(struct sassy_device *sdev)
{
	sassy_dbg("consensus clean\n");
	le_state_transition_to(sdev, LE_UNINIT);

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
	struct consensus_priv *priv =
		(struct consensus_priv *)sproto->priv;
	int remote_lid, rcluster_id;
	int err;
	struct pminfo *spminfo = &sdev->pminfo;

	if(!consensus_is_alive(sdev))
		return 0;


	if (sdev->verbose >= 3)
			sassy_dbg("consensus payload received\n");

	get_cluster_ids(sdev, remote_mac, &remote_lid, &rcluster_id);

	if(remote_lid == -1 || rcluster_id == -1)
		return -1;

	// Handle Warmup
	if(priv->warmup_state == WARMING_UP){

		if(spminfo->pm_targets[remote_lid].alive == 0){
			spminfo->pm_targets[remote_lid].alive = 1;
			priv->warms += 1;
		}

		sassy_log_le("%s, %llu, %d: Received Message from node %d \n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			rcluster_id);

		// Do not start Leader Election until all targets have send a message to this node.
		if(priv->warms != spminfo->num_of_targets)
			return 0;

		priv->warmup_state = WARMED_UP;
		write_log(&sdev->le_logger, START_CONSENSUS, rdtsc());

		// Transition to Follower State
		err = node_transition(sdev, FOLLOWER);

		sassy_log_le("%s, %llu, %d: Warmup done! \n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term);
		
		if(err)
			return err;
	}


	switch (priv->nstate) {
	case FOLLOWER:
		follower_process_pkt(sdev, remote_lid, rcluster_id, payload);
		break;
	case CANDIDATE:
		candidate_process_pkt(sdev, remote_lid, rcluster_id,  payload);
		break;
	case LEADER:
		leader_process_pkt(sdev, remote_lid, rcluster_id, payload);
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
	if(consensus_is_alive(sdev))
		return 0;

	if (sdev->verbose >= 3)
		sassy_dbg("consensus optimistical timestamp received.\n");
}
