#pragma once

#include <sassy/logger.h>
#include <sassy/sassy.h>


#define MAX_NODE_ID 5

#define TOLERANCE_FTIMEOUT_NS 5000000
#define TOLERANCE_CTIMEOUT_NS 5000000

#define MIN_FTIMEOUT_NS 100000000
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

	/* last term this node has voted in. Initialized with -1*/
	u32 voted; 

	/* follower timeout */
	struct hrtimer ftimer;
	int ftimer_init;

	/* candidate timeout */
	struct hrtimer ctimer;
	int ctimer_init;
	int c_retries;

	/* number of followers voted for this node */
	int votes;
};

int node_transition(struct sassy_device *sdev, enum node_state state);

struct consensus_priv *con_priv(void);
ktime_t get_rnd_timeout(void);
ktime_t get_rnd_timeout_plus(int plus);
void set_le_opcode(unsigned char *pkt, enum le_opcode opcode, u32 p1, u32 p2);
void accept_leader(struct sassy_device *sdev, int remote_lid, int cluster_id, u32 term);
int setup_le_broadcast_msg(struct sassy_device *sdev, enum le_opcode opcode);
int setup_le_msg(struct pminfo *spminfo, enum le_opcode opcode, u32 target_id, u32 term);
int setup_nomination(struct sassy_device *sdev);
void log_le_rx(int verbose, enum node_state nstate, uint64_t ts, int term, enum le_opcode opcode, int rcluster_id, int rterm);
void log_le_vote(int verbose, enum node_state nstate, uint64_t ts, int term, int cur_votes);
const char *nstate_string(enum node_state state);