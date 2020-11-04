#include <asgard/logger.h>
#include <asgard/asgard.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/proc_fs.h>

#include <asgard/consensus.h>
#include <synbuf-chardev.h>
#include <asgard/ringbuffer.h>

#include "include/asgard_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"




void generate_asgard_eval_uuid(unsigned char uuid[16])
{
    generate_random_uuid(uuid);
    asgard_dbg("===================== Start of Run: %pUB ====================\n", uuid);
}


int consensus_init(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;
	char name_buf[MAX_ASGARD_PROC_NAME];
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

	/* Pre Allocate Buffer Entries */
	for(i = 0; i < MAX_CONSENSUS_LOG; i++){
	    // Both freed on consensus clean
        priv->sm_log.entries[i] = kmalloc(sizeof(struct sm_log_entry), GFP_KERNEL);
        priv->sm_log.entries[i]->dataChunk = kmalloc(sizeof(struct data_chunk), GFP_KERNEL);
        priv->sm_log.entries[i]->valid = 0;
    }

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/proto_instances/%d",
			 priv->sdev->ifindex, ins->instance_id);

	proc_mkdir(name_buf, NULL);

	// requires "proto_instances/%d"
	init_le_config_ctrl_interfaces(priv);

	// requires "proto_instances/%d"
	init_eval_ctrl_interfaces(priv);

	// requires "proto_instances/%d"
    init_logger(&ins->logger, ins->instance_id, priv->sdev->ifindex, "consensus_le", 0);

    init_logger(&ins->user_a, ins->instance_id, priv->sdev->ifindex, "user_a", 1);
    init_logger(&ins->user_b, ins->instance_id, priv->sdev->ifindex, "user_b",1);
    init_logger(&ins->user_c, ins->instance_id, priv->sdev->ifindex, "user_c",1);
    init_logger(&ins->user_d, ins->instance_id, priv->sdev->ifindex, "user_d",1);

    init_logger(&priv->throughput_logger, ins->instance_id, priv->sdev->ifindex, "consensus_throughput", 0 );
    priv->throughput_logger.state = LOGGER_RUNNING;
    priv->throughput_logger.first_ts = 0;
    priv->throughput_logger.applied = 0;
    priv->throughput_logger.last_ts = 0;

    /* Initialize synbuf for Follower (RX) Buffer */
    priv->synbuf_rx = create_synbuf("rx", 250 * 20);

    if(!priv->synbuf_rx) {
        asgard_error("could not initialize synbuf for rx buffer\n");
        return -1;
    }

    /* Initialize RingBuffer in sybuf */
    setup_asg_ring_buf((struct asg_ring_buf*) priv->synbuf_rx->ubuf, 1000000 );

    /* Initialize synbuf for Leader (TX) Buffer */
    priv->synbuf_tx = create_synbuf("tx", 250 * 20);

    if(!priv->synbuf_tx) {
        asgard_error("could not initialize synbuf for tx buffer\n");
        return -1;
    }

    /* Initialize RingBuffer in sybuf */
    setup_asg_ring_buf((struct asg_ring_buf*) priv->synbuf_tx->ubuf, 1000000 );

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
		asgard_dbg("Consensus is already running!\n");
		return 0;
	}

	le_state_transition_to(priv, LE_RUNNING);

	err = node_transition(ins, FOLLOWER);


	if (err)
		goto error;

    asgard_dbg("===================== Start of Run: %pUB ====================\n", priv->uuid.b);

	return 0;

error:
	asgard_error(" %s failed\n", __func__);
	return err;
}



int consensus_stop(struct proto_instance *ins)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	if (!consensus_is_alive(priv))
		return 0;

    asgard_dbg("===================== End of Run: %pUB ====================\n", priv->uuid.b);


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
	char name_buf[MAX_ASGARD_PROC_NAME];

    le_state_transition_to(priv, LE_READY);

	if (consensus_is_alive(priv)) {
		asgard_dbg("Consensus is running, stop it first.\n");
		return 0;
	}

	le_state_transition_to(priv, LE_UNINIT);

	remove_eval_ctrl_interfaces(priv);

	remove_le_config_ctrl_interfaces(priv);

	clear_logger(&ins->logger);

	clear_logger(&priv->throughput_logger);
    clear_logger(&ins->user_a);
    clear_logger(&ins->user_b);
    clear_logger(&ins->user_c);
    clear_logger(&ins->user_d);


    snprintf(name_buf, sizeof(name_buf), "asgard/%d/proto_instances/%d",
		 priv->sdev->ifindex, ins->instance_id);

	remove_proc_entry(name_buf, NULL);

    for (i = 0; i < priv->sm_log.max_entries; i++){
        if (priv->sm_log.entries[i] != NULL){
            if(priv->sm_log.entries[i]->dataChunk != NULL){
                kfree(priv->sm_log.entries[i]->dataChunk);
            }
            kfree(priv->sm_log.entries[i]);
        }
    }

    /* Clean Follower (RX) Synbuf */
    synbuf_chardev_exit(priv->synbuf_rx);

    /* Clean Leader (TX) Synbuf  */
    synbuf_chardev_exit(priv->synbuf_tx);


	return 0;
}

int consensus_info(struct proto_instance *ins)
{
	asgard_dbg("consensus info\n");
	return 0;
}


int consensus_us_update(struct proto_instance *ins, void *payload)
{
	asgard_dbg("consensus update\n");


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
		asgard_dbg("proto instance is null!\n");
		return 0;
	}

	// safety check during debugging and development
	if (unlikely(!priv)) {
		asgard_dbg("private consensus data is null!\n");
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
		asgard_error("Unknown state - BUG\n");
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