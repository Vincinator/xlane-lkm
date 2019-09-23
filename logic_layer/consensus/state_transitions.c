#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/payload_helper.h>

#include <sassy/consensus.h>
#include "include/leader.h"
#include "include/follower.h"
#include "include/candidate.h"
#include "include/log.h"


static char *node_state_name(enum node_state state)
{
	switch (state) {
	case FOLLOWER: return "Follower";
	case CANDIDATE: return "Candidate";
	case LEADER: return "Leader";
	default: return "UNKNOWN STATE";
	}
}

char *_le_state_name(enum le_state state)
{
	switch (state) {
	case LE_RUNNING: return "RUNNING";
	case LE_READY: return "READY";
	case LE_UNINIT: return "UNINIT";
	default: return "UNKNOWN STATE";
	}
}

int setup_ae_msg(struct proto_instance *ins, struct pminfo *spminfo, u32 target_id, struct sm_command *cmd_array, int num_of_entries)
{
	struct sassy_payload *pkt_payload;
	char *pkt_payload_sub;
	int hb_passive_ix;
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	hb_passive_ix =
	     !!!spminfo->pm_targets[target_id].pkt_data.hb_active_ix;

	pkt_payload =
     	spminfo->pm_targets[target_id].pkt_data.pkt_payload[hb_passive_ix];

	pkt_payload_sub = 
 		sassy_reserve_proto(ins->instance_id, pkt_payload, SASSY_PROTO_CON_AE_BASE_SZ + (num_of_entries * AE_ENTRY_SIZE));

 	if(!pkt_payload_sub) {
 		sassy_error("Sassy packet full! This error is not handled - not implemented\n");
 		return -1;
 	}

	set_ae_data((unsigned char*)pkt_payload_sub,
				 priv->term,
				 priv->node_id,
				 priv->sm_log.last_idx,
				 priv->sm_log.last_term,
				 priv->sm_log.commit_idx,
				 cmd_array,
				 num_of_entries);
	
	spminfo->pm_targets[target_id].pkt_data.hb_active_ix = hb_passive_ix;

	return 0;
}

int setup_le_msg(struct proto_instance *ins, struct pminfo *spminfo, enum le_opcode opcode, u32 target_id, u32 term)
{
	struct sassy_payload *pkt_payload;
	char *pkt_payload_sub;
	int hb_passive_ix;


	hb_passive_ix =
	     !!!spminfo->pm_targets[target_id].pkt_data.hb_active_ix;

	pkt_payload =
     	spminfo->pm_targets[target_id].pkt_data.pkt_payload[hb_passive_ix];

	pkt_payload_sub = 
 		sassy_reserve_proto(ins->instance_id, pkt_payload, SASSY_PROTO_CON_PAYLOAD_SZ);

 	if(!pkt_payload_sub) {
 		sassy_error("Sassy packet full! This error is not handled - not implemented\n");
 		return -1;
 	}

	set_le_opcode((unsigned char*)pkt_payload_sub, opcode, term, 0);
	
	spminfo->pm_targets[target_id].pkt_data.hb_active_ix = hb_passive_ix;

	return 0;
}

int setup_le_broadcast_msg(struct proto_instance *ins, enum le_opcode opcode)
{
	int i;
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++)
		setup_le_msg(ins, &priv->sdev->pminfo, opcode, (u32) i, (u32) priv->term);

	return 0;
}

void accept_leader(struct proto_instance *ins, int remote_lid, int cluster_id, u32 term)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
#if 0
	sassy_log_le("%s, %llu, %d: accept cluster node %d with term %u as new leader\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			cluster_id,
			term);
#endif

	priv->term = term;
	priv->leader_id = remote_lid;
	node_transition(ins, FOLLOWER);

}

void le_state_transition_to(struct consensus_priv *priv, enum le_state state)
{
#if 0
	if(sdev->verbose >= 1)
		sassy_dbg("Leader Election Activation State Transition from %s to %s \n", _le_state_name(priv->state), _le_state_name(state));
#endif
	priv->state = state;

}

int node_transition(struct proto_instance *ins, enum node_state state)
{
	int err = 0;
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	priv->votes = 0; // start with 0 votes on every transition
	
	// Stop old timeouts 
	switch(state){
		case FOLLOWER:
			stop_follower(ins);
			break;
		case CANDIDATE:
			stop_candidate(ins);
			break;
		default:
			break;
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
		break;
	default:
		sassy_error("Unknown node state %d\n - abort", state);
		err = -EINVAL;
	}

	if (err)
		goto error;
#if 0
	sassy_log_le("%s, %llu, %d: transition to state %s\n",
				nstate_string(priv->nstate),
				rdtsc(),
				priv->term,
				nstate_string(state));
#endif
	priv->nstate = state;

	return 0;

error:
	sassy_error(" node transition failed\n");
	return err;
}

