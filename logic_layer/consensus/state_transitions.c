#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_consensus.h"
#include "include/leader.h"
#include "include/follower.h"
#include "include/candidate.h"


static char *node_state_name(enum node_state state)
{
	switch (state) {
	case FOLLOWER: return "Follower";
	case CANDIDATE: return "Candidate";
	case LEADER: return "Leader";
	default: return "UNKNOWN STATE";
	}
}

int setup_le_msg(struct pminfo *spminfo, enum le_opcode opcode, u32 target_id, u32 term)
{
	struct sassy_payload *pkt_payload;
	int hb_passive_ix;

	hb_passive_ix =
	     !!!spminfo->pm_targets[target_id].pkt_data.hb_active_ix;

	pkt_payload =
     	spminfo->pm_targets[target_id].pkt_data.pkt_payload[hb_passive_ix];

	set_le_opcode((unsigned char*)pkt_payload, opcode, term, 0);
	
	spminfo->pm_targets[target_id].pkt_data.hb_active_ix = hb_passive_ix;

	return 0;
}

int setup_le_broadcast_msg(struct sassy_device *sdev, enum le_opcode opcode)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
	int i;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++)
		setup_le_msg(&priv->sdev->pminfo, opcode, (u32) i, (u32) priv->term);

	return 0;
}

void accept_leader(struct sassy_device *sdev, int remote_lid, int cluster_id, u32 term)
{
	struct consensus_priv *priv = 
			(struct consensus_priv *)sdev->le_proto->priv;
	
	sassy_log_le("%s, %llu, %d: accept cluster node %d with term %u as new leader\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			cluster_id,
			term);

	priv->term = term;
	priv->leader_id = remote_lid;
	node_transition(sdev, FOLLOWER);
}


int node_transition(struct sassy_device *sdev, enum node_state state)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
	int err = 0;

	priv->votes = 0; // start with 0 votes on every transition
	
	switch (state) {
	case FOLLOWER:
		err = start_follower(sdev);
		break;
	case CANDIDATE:
		err = start_candidate(sdev);
		break;
	case LEADER:
		err = start_leader(sdev);
		break;
	default:
		sassy_error("Unknown node state %d\n - abort", state);
		err = -EINVAL;
	}

	if (err)
		goto error;

	sassy_log_le("%s, %llu, %d: transition to state %s\n",
				nstate_string(priv->nstate),
				rdtsc(),
				priv->term,
				nstate_string(state));
	
	priv->nstate = state;

	return 0;

error:
	sassy_error(" node transition failed\n");
	return err;
}

