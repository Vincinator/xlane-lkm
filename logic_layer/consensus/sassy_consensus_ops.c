#include <sassy/logger.h>
#include <sassy/sassy.h>


#include "include/sassy_consensus.h"
#include "include/sassy_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"



int consensus_init(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	priv->ctimer_init = 0;
	priv->ftimer_init = 0;
	priv->voted = -1;
	priv->term = 0;
	priv->warms = 0;
	priv->warmup_state = WARMING_UP;
	priv->state = LE_READY;

	if(!priv->le_config_procfs)
		init_le_config_ctrl_interfaces(priv);

	init_logger(&ins->logger);

	return 0;
}

int consensus_init_payload(void *payload)
{

	return 0;
}

int consensus_start(struct proto_instance *ins)
{
	int err;

	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	sassy_dbg("consensus start\n");

	if(consensus_is_alive(priv)){
		sassy_dbg("Consensus is already running!\n");
		return 0;
	}

	if (err)
		goto error;

	le_state_transition_to(priv, LE_RUNNING);

	return 0;

error:
	sassy_error(" %s failed\n", __FUNCTION__);
	return err;
}

int consensus_stop(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	if(!consensus_is_alive(priv))
		return 0;
	
	sassy_dbg("consensus stop\n");

	switch(priv->nstate) {
		case FOLLOWER:
			stop_follower(ins);
			break;
		case CANDIDATE:
			stop_candidate(ins);
			break;
		case LEADER:
			stop_leader(ins);
			break;
	}
	le_state_transition_to(priv, LE_READY);

	set_all_targets_dead(priv->sdev);
	
	priv->warmup_state = WARMING_UP;
	priv->warms = 0;

	return 0;
}

int consensus_clean(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	sassy_dbg("consensus clean\n");
	le_state_transition_to(priv, LE_UNINIT);

	remove_le_config_ctrl_interfaces(priv);
	remove_logger_ifaces(&ins->logger);


	return 0;
}

int consensus_info(struct proto_instance *ins)
{
	sassy_dbg("consensus info\n");
	return 0;
}


int consensus_us_update(struct proto_instance *ins, void *payload)
{
	sassy_dbg("consensus update\n");


	return 0;
}

int consensus_post_payload(struct proto_instance *ins, unsigned char *remote_mac,
		    void *payload)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
	int remote_lid, rcluster_id;
	int err, i;
	struct pminfo *spminfo = &priv->sdev->pminfo;

	if(!consensus_is_alive(priv))
		return 0;

	get_cluster_ids(priv->sdev, remote_mac, &remote_lid, &rcluster_id);

	if(remote_lid == -1 || rcluster_id == -1)
		return -1;

	// Handle Warmup
	if(priv->warmup_state == WARMING_UP){

		if(spminfo->pm_targets[remote_lid].alive == 0){
			spminfo->pm_targets[remote_lid].alive = 1;
		}

		sassy_log_le("%s, %llu, %d: Received Message from node %d\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			rcluster_id);

		// Do not start Leader Election until all targets have send a message to this node.
		for(i = 0; i < spminfo->num_of_targets; i++)
			if(!spminfo->pm_targets[i].alive)
				return 0;

		priv->warmup_state = WARMED_UP;
		write_log(&ins->logger, START_CONSENSUS, rdtsc());

		// Transition to Follower State
		err = node_transition(ins, FOLLOWER);

		sassy_log_le("%s, %llu, %d: Warmup done! \n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term);
		
		if(err)
			return err;
	}


	switch (priv->nstate) {
	case FOLLOWER:
		follower_process_pkt(ins, remote_lid, rcluster_id, payload);
		break;
	case CANDIDATE:
		candidate_process_pkt(ins, remote_lid, rcluster_id,  payload);
		break;
	case LEADER:
		leader_process_pkt(ins, remote_lid, rcluster_id, payload);
		break;
	default:
		sassy_error("Unknown state - BUG\n");
		break;
	}
	return 0;
}

int consensus_post_ts(struct proto_instance *ins, unsigned char *remote_mac,
	       uint64_t ts)
{
	// if(consensus_is_alive(sdev))
	// 	return 0;
}
