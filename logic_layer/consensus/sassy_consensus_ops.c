#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/proc_fs.h>

#include <sassy/consensus.h>
#include "include/sassy_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"



int consensus_init(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
	char name_buf[MAX_SASSY_PROC_NAME];

	priv->ctimer_init = 0;
	priv->ftimer_init = 0;
	priv->voted = -1;
	priv->term = 0;
	priv->state = LE_READY;

	priv->sm_log.last_idx = -1;
	priv->sm_log.commit_idx = -1;
	priv->sm_log.last_applied = -1;
	priv->sm_log.max_entries = MAX_CONSENSUS_LOG;

	ins->logger.name = "consensus_le";

	ins->logger.instance_id = ins->instance_id;
	
	priv->throughput_logger.instance_id = ins->instance_id;
	
	priv->throughput_logger.name = "consensus_throughput";

	priv->throughput_logger.events = kmalloc_array(MAX_THROUGPUT_LOGGER_EVENTS, sizeof(struct logger_event *), GFP_KERNEL);
	

	if(!priv->throughput_logger.events){
		sassy_dbg("Not enough memory for throughput_logger of size %d", MAX_THROUGPUT_LOGGER_EVENTS);
		//BUG();
	}

	priv->sm_log.entries = kmalloc_array(MAX_CONSENSUS_LOG, sizeof(struct sm_log_entry *), GFP_KERNEL);
	
	if(!priv->sm_log.entries){
		sassy_dbg("Not enough memory for log of size %d", MAX_CONSENSUS_LOG);
		//BUG();
	}

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/%d", 
			 priv->sdev->ifindex, ins->instance_id);
	
	proc_mkdir(name_buf, NULL);	

	// requires "proto_instances/%d"
	init_le_config_ctrl_interfaces(priv);
	
	// requires "proto_instances/%d"
	init_eval_ctrl_interfaces(priv);
	
	// requires "proto_instances/%d"
	init_logger(&ins->logger);

	init_logger(&priv->throughput_logger);


	
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

	le_state_transition_to(priv, LE_RUNNING);

	err = node_transition(ins, FOLLOWER);

	write_log(&ins->logger, START_CONSENSUS, rdtsc());

	if (err)
		goto error;

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
	
	return 0;
}

int consensus_clean(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
	u32 i;
	char name_buf[MAX_SASSY_PROC_NAME];


	if(consensus_is_alive(priv)){
		sassy_dbg("Consensus is running, stop it first.\n");
		return 0;
	}

	sassy_dbg("cleaning consensus\n");

	le_state_transition_to(priv, LE_UNINIT);

	remove_eval_ctrl_interfaces(priv);

	remove_le_config_ctrl_interfaces(priv);

	clear_logger(&ins->logger);

	clear_logger(&priv->throughput_logger);

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/%d", 
		 priv->sdev->ifindex, ins->instance_id);
	
	remove_proc_entry(name_buf, NULL);	

	sassy_dbg("last_idx of sm log is %d\n", priv->sm_log.last_idx);

	if(priv->sm_log.last_idx != -1 && priv->sm_log.last_idx < MAX_CONSENSUS_LOG) {
		for(i = 0; i < priv->sm_log.last_idx; i++) {
			if(priv->sm_log.entries[i] != NULL)
				kfree(priv->sm_log.entries[i]);
		}
	} else {
		sassy_dbg("last_idx is -1, no logs to clean.\n");
	}

	kfree(priv->sm_log.entries);


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

	// safety check during debugging and development
	if(!ins){
		sassy_dbg("proto instance is null!\n");
		return 0;
	}
	
	// safety check during debugging and development
	if(!priv){
		sassy_dbg("private consensus data is null!\n");
		return 0;
	}

	get_cluster_ids(priv->sdev, remote_mac, &remote_lid, &rcluster_id);
	
	if(remote_lid == -1 || rcluster_id == -1)
		return -1;
	

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
