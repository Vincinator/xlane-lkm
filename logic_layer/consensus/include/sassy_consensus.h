#pragma once

#include <sassy/logger.h>
#include <sassy/sassy.h>


/*
 * Every node starts as follower.
 *
 * When a follower does not receive a heartbeat from a leader after a random timeout,
 * then the follower transitions to candidate state.
 *
 * A candidate proposes itself to all other nodes as new leader.
 * Other followers vote for the candidate if they did not vote for any other 
 * candidate in the same term yet. 
 *
 * Once elected, the candidate becomes the new leader and sends heartbeats to all followers.
 *
 * A heartbeat can contain log updates.
 */
enum node_state {
	FOLLOWER = 0,
	CANDIDATE = 1,
	LEADER = 2,
};


struct consensus_priv {
	enum node_state nstate;
};