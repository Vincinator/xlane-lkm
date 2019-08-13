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

int node_transition(enum node_state state)
{
	struct consensus_priv *priv = con_priv();
	int err = 0;

	sassy_dbg("node transition from %s to %s\n",
		node_state_name(priv->nstate), node_state_name(state));

	switch(priv->nstate) {
		case FOLLOWER:
			err = stop_follower();
			break;
		case CANDIDATE:
			err = stop_candidate();
			break;
		case LEADER:
			err = stop_leader();
			break;
		default:
			sassy_dbg("No previous state was defined\n");
	}

	if(err){
		sassy_dbg("Failed to stop previous role\n");
		goto error;
	}

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

	if (err){
		sassy_dbg("Failed to start new role\n");
		goto error;
	}

	priv->nstate = state;
	sassy_dbg(" node transition was successfull ");
	return 0;

error:
	sassy_error(" node transition failed\n");
	return err;
}

