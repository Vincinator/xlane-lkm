#pragma once

#include <sassy/logger.h>
#include <sassy/sassy.h>


#define MAX_NODE_ID 5

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


/* 
 * HB: message from leader
 * VOTE: message from follower to candidate
 * NOMINATION: message from candidate to all cluster nodes
 */
enum cmsg_type {
	HB = 0,
	VOTE = 1,
	NOMINATION = 2,
};


struct consensus_priv {

	struct sassy_device *sdev;

	enum node_state nstate;

	int node_id;

	/* index of array is node_id, 
	 * value at index of array is index to pm_targets
	 */
	int cluster_mapping[MAX_NODE_ID];

	int term;

	/* True if this node has already voted in the current term*/
	bool voted; 

	/* follower timeout */
	struct hrtimer ftimer;

	/* candidate timeout */
	struct hrtimer ctimer;

	/* number of followers voted for this node */
	int votes;


};

int node_transition(enum node_state state);

struct consensus_priv *con_priv(void);