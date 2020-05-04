#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>

#include <asguard/consensus.h>
#include <asguard/asgard_uface.h>
#include "include/leader.h"
#include "include/follower.h"
#include "include/candidate.h"


char *_le_state_name(enum le_state state)
{
	switch (state) {
	case LE_RUNNING: return "RUNNING";
	case LE_READY: return "READY";
	case LE_UNINIT: return "UNINIT";
	default: return "UNKNOWN STATE";
	}
}

int setup_le_msg(struct proto_instance *ins, struct pminfo *spminfo, enum le_opcode opcode, u32 target_id, s32 param1, s32 param2, s32 param3, s32 param4)
{
	struct asguard_payload *pkt_payload;
	char *pkt_payload_sub;

    spin_lock(&spminfo->pm_targets[target_id].pkt_data.lock);

    pkt_payload =
		spminfo->pm_targets[target_id].pkt_data.payload;

	pkt_payload_sub =
		asguard_reserve_proto(ins->instance_id, pkt_payload, ASGUARD_PROTO_CON_PAYLOAD_SZ);

	if (!pkt_payload_sub) {
		asguard_error("Sassy packet full!\n");
		goto unlock;
	}

	set_le_opcode((unsigned char *)pkt_payload_sub, opcode, param1, param2, param3, param4);

unlock:
    spin_unlock(&spminfo->pm_targets[target_id].pkt_data.lock);
	return 0;
}

int setup_le_broadcast_msg(struct proto_instance *ins, enum le_opcode opcode)
{
	int i, buf_stable_idx;
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	s32 term = priv->term;
	s32 candidate_id = priv->node_id;

	s32 last_log_idx = priv->sm_log.stable_idx;
	s32 last_log_term;

	if (priv->sm_log.stable_idx == -1)
		last_log_term = priv->term;
	else{
		asguard_dbg("priv->sm_log.stable_idx=%d\n", priv->sm_log.stable_idx);
        buf_stable_idx = consensus_idx_to_buffer_idx(&priv->sm_log, priv->sm_log.last_applied);

        if(buf_stable_idx == -1) {
            asguard_error("Invalid idx. could not convert to buffer idx in %s",__FUNCTION__);
            return -1;
        }
		last_log_term = priv->sm_log.entries[buf_stable_idx]->term;
	}

	for (i = 0; i < priv->sdev->pminfo.num_of_targets; i++)
		setup_le_msg(ins, &priv->sdev->pminfo, opcode, (u32) i, term, candidate_id, last_log_idx, last_log_term);

	return 0;
}

void accept_leader(struct proto_instance *ins, int remote_lid, int cluster_id, u32 term)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;
#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_log_le("%s, %llu, %d: accept cluster node %d with term %u as new leader\n",
			nstate_string(priv->nstate),
			RDTSC_ASGUARD,
			priv->term,
			cluster_id,
			term);
#endif

	priv->term = term;
	priv->leader_id = cluster_id;
	priv->sdev->cur_leader_lid = remote_lid;
	node_transition(ins, FOLLOWER);

}

void le_state_transition_to(struct consensus_priv *priv, enum le_state state)
{
#if VERBOSE_DEBUG
	if(priv->sdev->verbose)
		asguard_dbg("Leader Election Activation State Transition from %s to %s\n", _le_state_name(priv->state), _le_state_name(state));
#endif
	priv->state = state;

}

int node_transition(struct proto_instance *ins, enum node_state state)
{
	int err = 0;
	struct consensus_priv *priv =
		(struct consensus_priv *)ins->proto_data;

	priv->votes = 0; // start with 0 votes on every transition

	switch (priv->nstate) {
	case FOLLOWER:
		err = stop_follower(ins);
		break;
	case CANDIDATE:
		err = stop_candidate(ins);
		break;
	case LEADER:
		err = stop_leader(ins);
		break;
	default:
		asguard_error("Unknown node state %d\n - abort", state);
		err = -EINVAL;
	}

	switch (state) {
	case FOLLOWER:
		err = start_follower(ins);
		break;
	case CANDIDATE:
		err = start_candidate(ins);
		break;
	case LEADER:
		err = start_leader(ins);
		write_log(&ins->logger, CANDIDATE_BECOME_LEADER, RDTSC_ASGUARD);
		break;
	default:
		asguard_error("Unknown node state %d\n - abort", state);
		err = -EINVAL;
	}

	if (err)
		goto error;
#if VERBOSE_DEBUG
	asguard_log_le("%s, %llu, %d: transition to state %s\n",
				nstate_string(priv->nstate),
				RDTSC_ASGUARD,
				priv->term,
				nstate_string(state));
#endif

	/* Persist node state in kernel space */
	priv->nstate = state;

	/* Update Node State for User Space */
    update_self_state(priv->sdev->ci, state);

    return 0;

error:
	asguard_error(" node transition failed\n");
	return err;
}

