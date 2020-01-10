#ifndef _ASGUARD_CONSENSUS_H_
#define _ASGUARD_CONSENSUS_H_

#include <asguard/asguard.h>
#include <asguard/logger.h>

#define MAX_NODE_ID 10

#define TOLERANCE_FTIMEOUT_NS 0
#define TOLERANCE_CTIMEOUT_NS 500000

#define MAX_CONSENSUS_LOG 100000

#define AE_ENTRY_SIZE 8
#define MAX_AE_ENTRIES_PER_PKT 200

#define MAX_THROUGPUT_LOGGER_EVENTS 10000


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

struct sm_command {
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
	struct sm_command *cmd;
};


struct state_machine_cmd_log {

	s32 next_index[MAX_NODE_ID];

	s32 match_index[MAX_NODE_ID];

	s32 last_applied;

	/* Index of the last valid entry in the entries array
	 */
	s32 last_idx;

	/* Index of the last commited entry in the entries array
	 */
	s32 commit_idx;

	/* Maximum index of the entries array
	 */
	u32 max_entries;

	/* locked if node currently writes to this consensus log.
	 * this lock prevents the race condition when a follower can not keep up
	 * with the updates from the current leader.
	 */
	int lock;

	spinlock_t slock;

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

	struct asguard_device *sdev;

	struct proto_instance *ins;

	struct consensus_test_container test_data;

	// last leader timestmap before current follower timeout
	uint64_t llts_before_ftime;

	enum node_state nstate;

	enum le_state state;

	int candidate_counter;

	u32 leader_id;

	u32 node_id;

	/* index of array is node_id,
	 * value at index of array is index to pm_targets
	 */
	int cluster_mapping[MAX_NODE_ID];

	s32 term;

	uint64_t accu_rand;

	/* last term this node has voted in. Initialized with -1*/
	u32 voted;

	u32 started_log;

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

	int max_entries_per_pkt;

	/* number of followers voted for this node */
	int votes;


	struct state_machine_cmd_log sm_log;
	struct asguard_logger throughput_logger;

};


int commit_log(struct consensus_priv *priv);
int append_command(struct state_machine_cmd_log *log, struct sm_command *cmd, int term);



int node_transition(struct proto_instance *ins, enum node_state state);

struct consensus_priv *con_priv(void);
ktime_t get_rnd_timeout(int min, int max);
ktime_t get_rnd_timeout_candidate(void);
ktime_t get_rnd_timeout_plus(int plus);
ktime_t get_rnd_timeout_candidate_plus(int plus);

void set_le_opcode(unsigned char *pkt, enum le_opcode opcode, s32 p1, s32 p2, s32 p3, s32 p4);

void set_ae_data(unsigned char *pkt,
				 s32 in_term,
				 s32 in_leaderid,
				 s32 in_prevLogIndex,
				 s32 in_prevLogTerm,
				 s32 in_leaderCommitIdx,
				 struct consensus_priv *priv,
				 s32 num_of_entries);



void accept_leader(struct proto_instance *ins, int remote_lid, int cluster_id, u32 term);
int setup_le_broadcast_msg(struct proto_instance *ins, enum le_opcode opcode);
int setup_le_msg(struct proto_instance *ins, struct pminfo *spminfo, enum le_opcode opcode, u32 target_id, s32 param1, s32 param2, s32 param3, s32 param4);
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
int check_append_rpc(u16 pkt_size, u32 prev_log_term, s32 prev_log_idx, int max_entries_per_pkt);

#endif /* _ASGUARD_CONSENSUS_H_ */
