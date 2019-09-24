#pragma once

#include <sassy/logger.h>
#include <sassy/sassy.h>

#define MAX_NODE_ID 5

#define TOLERANCE_FTIMEOUT_NS 500000
#define TOLERANCE_CTIMEOUT_NS 500000

#define MAX_CONSENSUS_LOG 1000000 


enum le_state {
	LE_RUNNING = 0,
	LE_READY = 1,
	LE_UNINIT = 2,
};

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


struct sm_command{
	u32 sm_logvar_id;
	u32 sm_logvar_value;
};


struct sm_log_entry {

	/* Term in which this command was appended to the log
	 */
	u32 term;

	/* The command for the state machine.
	 * Contains the required information to change the state machine.
	 *
	 * A ordered set of commands applied to the state machine will 
	 * transition the state machine to a common state (shared across the cluster).
	 */ 
	struct sm_command cmd;
};


struct state_machine_cmd_log {

	u32 last_applied;

	/* Index of the last valid entry in the entries array
	 */
	u32 last_idx;

	/* Index of the last commited entry in the entries array 
	 */
	u32 commit_idx;

	/* Maximum index of the entries array
	 */
	u32 max_entries;

	struct sm_log_entry **entries;

};

struct consensus_priv;

struct consensus_test_container {
	struct consensus_priv *priv;
	struct hrtimer timer;
	int running;
	int x;
};

struct consensus_priv {

	struct sassy_device *sdev;

	struct proto_instance *ins;

	struct consensus_test_container test_data;


	enum node_state nstate;

	enum le_state state;

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
	int ft_max;
	int ft_min;

	/* candidate timeout */
	struct hrtimer ctimer;
	int ctimer_init;
	int c_retries;

	int ct_min;
	int ct_max;

	struct proc_dir_entry *le_config_procfs;

	/* number of followers voted for this node */
	int votes;

	struct state_machine_cmd_log sm_log;
};


int commit_upto_index(struct state_machine_cmd_log *log, u32 index);
int append_command(struct state_machine_cmd_log *log, struct sm_command *cmd);



int node_transition(struct proto_instance *ins, enum node_state state);

struct consensus_priv *con_priv(void);
ktime_t get_rnd_timeout(int min, int max);
ktime_t get_rnd_timeout_candidate(void);
ktime_t get_rnd_timeout_plus(int plus);
ktime_t get_rnd_timeout_candidate_plus(int plus);

void set_le_opcode(unsigned char *pkt, enum le_opcode opcode, u32 p1, u32 p2);

void set_ae_data(unsigned char *pkt, 
				 u32 in_term, 
				 u32 in_leaderid,
				 u32 in_prevLogIndex,
				 u32 in_prevLogTerm,
				 u32 in_leaderCommitIdx,
				 struct sm_command *cmd_array, 
				 int num_of_entries);



void accept_leader(struct proto_instance *ins, int remote_lid, int cluster_id, u32 term);
int setup_le_broadcast_msg(struct proto_instance *ins, enum le_opcode opcode);
int setup_le_msg(struct proto_instance *ins, struct pminfo *spminfo, enum le_opcode opcode, u32 target_id, u32 term);
int setup_ae_msg(struct proto_instance *ins, struct pminfo *spminfo, u32 target_id, struct sm_command *cmd_array, int num_of_entries);


void log_le_rx(int verbose, enum node_state nstate, uint64_t ts, int term, enum le_opcode opcode, int rcluster_id, int rterm);
void log_le_vote(int verbose, enum node_state nstate, uint64_t ts, int term, int cur_votes);
const char *nstate_string(enum node_state state);
void le_state_transition_to(struct consensus_priv *priv, enum le_state state);


void init_le_config_ctrl_interfaces(struct consensus_priv *priv);
void remove_le_config_ctrl_interfaces(struct consensus_priv *priv);

void init_eval_ctrl_interfaces(struct consensus_priv *priv);
void remove_eval_ctrl_interfaces(struct consensus_priv *priv);


int consensus_is_alive(struct consensus_priv *sdev);
