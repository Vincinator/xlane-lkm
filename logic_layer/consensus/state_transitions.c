#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_consensus.h"
#include "include/leader.h"
#include "include/follower.h"
#include "include/candidate.h"


char *node_state_name(enum node_state state)
{
	switch (state) {
	case FOLLOWER: return "Follower";
	case CANDIDATE: return "Candidate";
	case LEADER: return "Leader";
	default: return "UNKNOWN STATE";
	}
}

int node_transition(struct sassy_device *sdev, enum node_state state)
{
	struct sassy_protocol *sproto = sdev->proto;
	struct consensus_priv *priv =
		(struct consensus_priv *)sproto->priv;
	int err;

	sassy_dbg("node transition from %s to %s\n",
		node_state_name(priv->nstate), node_state_name(state));

	switch (state) {
	case FOLLOWER:
		err = start_follower();
		break;
	case CANDIDATE:
		err = start_candidate();
		break;
	case LEADER:
		err = start_leader();
		break;
	default:
		sassy_error("Unknown node state %d\n - abort", state);
		err = -EINVAL;
	}

	if (err)
		goto error;

	priv->nstate = state;
	sassy_dbg(" node transition was successfull ");
	return 0;

error:
	sassy_error(" node transition failed\n");
	return err;
}
