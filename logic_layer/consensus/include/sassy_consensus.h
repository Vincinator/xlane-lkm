#pragma once

#include <sassy/logger.h>
#include <sassy/sassy.h>


#define MAX_NODE_ID 5

#define MIN_FTIMEOUT_NS 150000000
#define MAX_FTIMEOUT_NS 300000000

#define MIN_CTIMEOUT_NS 300000000
#define MAX_CTIMEOUT_NS 600000000

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
 * Each heartbeat contains one of the following operations 
 *
 * NOOP: 			This heartbeat does not contain an update
 *					for the leader election
 *
 * NOMI(TERM): 		The sender of this message has nominated
 *					itself to become the new leader of the cluster
 *			   		for the given TERM (parameter1).
 *
 * VOTE(TERM,ID): 	The sender voted for the node with 
 *					the given ID (parameter2) to become
 *					the new leader in the TERM (parameter1).
 *
 * LEAD(TERM):		The sender is the active leader of the cluster.
 *					The receiver accepts the leader if the term is
 *					greater or equal the receivers localy stored term
 *					value.
 */
enum le_opcode {
	NOOP = 0,
	NOMI = 1,
	VOTE = 2,
	LEAD = 3,
};

struct consensus_priv {

	struct sassy_device *sdev;

	enum node_state nstate;

	u32 leader_id;

	u32 node_id;

	/* index of array is node_id, 
	 * value at index of array is index to pm_targets
	 */
	int cluster_mapping[MAX_NODE_ID];

	u32 term;

	/* True if this node has already voted in the current term*/
	bool voted; 

	/* follower timeout */
	struct hrtimer ftimer;
	int ftimer_init;

	/* candidate timeout */
	struct hrtimer ctimer;
	int ctimer_init;

	/* number of followers voted for this node */
	int votes;


};

int node_transition(struct sassy_device *sdev, enum node_state state);

struct consensus_priv *con_priv(void);
ktime_t get_rnd_timeout(void);
void set_le_opcode(struct sassy_payload *pkt_payload, enum le_opcode opcode, u32 p1, u32 p2);
void accept_leader(struct sassy_device *sdev, int remote_lid, u32 term);