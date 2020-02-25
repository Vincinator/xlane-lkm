#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/proc_fs.h>

#include <asguard/consensus.h>
#include "include/asguard_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"



int consensus_init(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;
	char name_buf[MAX_ASGUARD_PROC_NAME];
	int i;

	priv->voted = -1;
	priv->term = 0;
	priv->state = LE_READY;

	priv->sm_log.last_idx = -1;
	priv->sm_log.commit_idx = -1;
	priv->sm_log.stable_idx = -1;
	priv->sm_log.next_retrans_req_idx = -2;
	priv->sm_log.last_applied = -1;
	priv->sm_log.max_entries = MAX_CONSENSUS_LOG;
	priv->sm_log.lock = 0;
    priv->sdev->consensus_priv = priv; /* reference for pacemaker */


	priv->sm_log.entries = kmalloc_array(MAX_CONSENSUS_LOG, sizeof(struct sm_log_entry *), GFP_KERNEL);

	if (!priv->sm_log.entries)
		BUG();

	for(i = 0; i < MAX_CONSENSUS_LOG; i++)
		priv->sm_log.entries[i] = NULL;

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances/%d",
			 priv->sdev->ifindex, ins->instance_id);

	proc_mkdir(name_buf, NULL);

	// requires "proto_instances/%d"
	init_le_config_ctrl_interfaces(priv);

	// requires "proto_instances/%d"
	init_eval_ctrl_interfaces(priv);

	// requires "proto_instances/%d"
    init_logger(&ins->logger, ins->instance_id, priv->sdev->ifindex, "consensus_le");

   // init_logger(&priv->throughput_logger, ins->instance_id, priv->sdev->ifindex, "consensus_throughput");


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

	if (consensus_is_alive(priv)) {
		asguard_dbg("Consensus is already running!\n");
		return 0;
	}

	le_state_transition_to(priv, LE_RUNNING);

	err = node_transition(ins, FOLLOWER);

	write_log(&ins->logger, START_CONSENSUS, RDTSC_ASGUARD);

	if (err)
		goto error;

	return 0;

error:
	asguard_error(" %s failed\n", __func__);
	return err;
}

int consensus_stop(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	if (!consensus_is_alive(priv))
		return 0;

	switch (priv->nstate) {
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
	char name_buf[MAX_ASGUARD_PROC_NAME];

    le_state_transition_to(priv, LE_READY);

	if (consensus_is_alive(priv)) {
		asguard_dbg("Consensus is running, stop it first.\n");
		return 0;
	}

	le_state_transition_to(priv, LE_UNINIT);

	remove_eval_ctrl_interfaces(priv);

	remove_le_config_ctrl_interfaces(priv);

	clear_logger(&ins->logger);

	//clear_logger(&priv->throughput_logger);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances/%d",
		 priv->sdev->ifindex, ins->instance_id);

	remove_proc_entry(name_buf, NULL);
    asguard_error("%s - %s\n",__FUNCTION__,  __LINE__);

	if (priv->sm_log.last_idx != -1 && priv->sm_log.last_idx < MAX_CONSENSUS_LOG) {
		for (i = 0; i < priv->sm_log.last_idx; i++){

            // entries are NULL initialized
            if (priv->sm_log.entries[i] != NULL){
                if(priv->sm_log.entries[i]->cmd != NULL){
                    kfree(priv->sm_log.entries[i]->cmd);
                }
                kfree(priv->sm_log.entries[i]);
            }
		}

	} else {
		asguard_dbg("last_idx is -1, no logs to clean.\n");
	}

	return 0;
}

int consensus_info(struct proto_instance *ins)
{
	asguard_dbg("consensus info\n");
	return 0;
}


int consensus_us_update(struct proto_instance *ins, void *payload)
{
	asguard_dbg("consensus update\n");


	return 0;
}

int consensus_post_payload(struct proto_instance *ins, int remote_lid, int rcluster_id,
		    void *payload)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	if (unlikely(!consensus_is_alive(priv)))
		return 0;

	// safety check during debugging and development
	if (unlikely(!ins)) {
		asguard_dbg("proto instance is null!\n");
		return 0;
	}

	// safety check during debugging and development
	if (unlikely(!priv)) {
		asguard_dbg("private consensus data is null!\n");
		return 0;
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
		asguard_error("Unknown state - BUG\n");
		break;
	}

    return 0;
}


int consensus_post_ts(struct proto_instance *ins, unsigned char *remote_mac,
	       uint64_t ts)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	 if (!consensus_is_alive(priv))
		return 0;

	return 0;
}
