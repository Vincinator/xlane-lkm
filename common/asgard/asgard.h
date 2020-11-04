#ifndef _ASGARD_H_
#define _ASGARD_H_

#include <linux/workqueue.h>
#include <asgard/ringbuffer.h>
#include <asgard/multicast.h>


#define VERBOSE_DEBUG 1

#ifndef CONFIG_KUNIT
#define RDTSC_ASGARD rdtsc()
#else
#define RDTSC_ASGARD 0ULL
#endif


#include <linux/list.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <asgard/asgard_async.h>

#include "logger.h"

#define MAX_ASGARD_PROC_NAME 256

#define MAX_PROCFS_BUF 512


#define ASGARD_UUID_BUF 42

#define ASGARD_TARGETS_BUF 512
#define ASGARD_NUMBUF 13

#define MAX_REMOTE_SOURCES 16

#define MIN_HB_CYCLES 1000
#define MAX_HB_CYCLES 1000000000

#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_5MS 12000000
#define CYCLES_PER_10MS 24000000
#define CYCLES_PER_100MS 240000000

#define MAX_PROCESSES_PER_HOST 16

#define ASGARD_MLX5_DEVICES_LIMIT                                               \
	5 /* Number of allowed mlx5 devices that can connect to ASGARD */
#define MAX_CPU_NUMBER 55

#define ASGARD_HEADER_BYTES 64

#define MAX_ASGARD_PAYLOAD_BYTES (1500 - ASGARD_HEADER_BYTES) // asuming an ethernet mtu of ~1500 bytes

#define ASGARD_PAYLOAD_BYTES MAX_ASGARD_PAYLOAD_BYTES
#define ASGARD_PKT_BYTES (ASGARD_PAYLOAD_BYTES + ASGARD_HEADER_BYTES)

int asgard_core_register_nic(int ifindex, int asgard_id);
int asgard_core_remove_nic(int asgard_id);

#define ASGARD_NUM_TS_LOG_TYPES 8
#define TIMESTAMP_ARRAY_LIMIT 100000
#define LE_EVENT_LOG_LIMIT 100000

#define MAX_PROTOS_PER_PKT 2

#define MAX_PROTO_INSTANCES 8


#define DEFAULT_HB_INTERVAL CYC_1MS


/*
 * Each heartbeat contains one of the following operations
 *
 * NOOP:			This heartbeat does not contain an update
 *					for the leader election
 *
 * NOMI(TERM):		The sender of this message has nominated
 *					itself to become the new leader of the cluster
 *					for the given TERM (parameter1).
 *
 * VOTE(TERM,ID):	The sender voted for the node with
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
	APPEND = 4,
	APPEND_REPLY = 5,
	ALIVE = 6,
	ADVERTISE = 7,
};

enum echo_opcode {
    ASGARD_PING_REQ_UNI = 0,
    ASGARD_PONG_UNI = 1,
    ASGARD_PING_REQ_MULTI = 2,
    ASGARD_PONG_MULTI = 3,

};


enum hb_interval {
	CYC_1MS = 1,
	CYC_5MS = 2,
	CYC_10MS = 3,
	CYC_100MS = 4,
};

enum tsstate {
    ASGARD_TS_RUNNING,
    ASGARD_TS_READY,		/* Initialized but not active*/
    ASGARD_TS_UNINIT,
    ASGARD_TS_LOG_FULL,
};

struct asgard_timestamp_item {
	uint64_t timestamp_tcs;
	//ktime_t correlation_id;
	int target_id; /* Host target id to which the heartbeat was send (if TX) */
};

struct asgard_timestamp_logs {
	struct asgard_timestamp_item *timestamp_items; /* Size is defined with TIMESTAMP_ARRAY_LIMIT macro*/
	int current_timestamps; /* How many timestamps are set in the timestamp */
	struct proc_dir_entry	*proc_dir;
	char *name;
};

struct asgard_stats {
	/* Array of timestamp logs
	 * - Each item corresponds to one timestamping type
	 * - Each item is a pointer to a asgard_timestamp_logs struct
	 */
	struct asgard_timestamp_logs **timestamp_logs;
	int timestamp_amount; /* how many different timestamps types are tracked*/
};

enum le_event_type {

	FOLLOWER_TIMEOUT = 0,
	CANDIDATE_TIMEOUT = 1,

	FOLLOWER_ACCEPT_NEW_LEADER = 2,
	CANDIDATE_ACCEPT_NEW_LEADER = 3,
	LEADER_ACCEPT_NEW_LEADER = 4,

	FOLLOWER_BECOME_CANDIDATE = 5,
	CANDIDATE_BECOME_LEADER = 6,

	START_CONSENSUS = 7,
	VOTE_FOR_CANDIDATE = 8,
	CANDIDATE_ACCEPT_VOTE = 9,

	REPLY_APPEND_SUCCESS = 10,
	REPLY_APPEND_FAIL = 11,

	CONSENSUS_REQUEST = 12,
	GOT_CONSENSUS_ON_VALUE = 13,
	START_LOG_REP = 14,
};

enum logger_state {
	LOGGER_RUNNING,
	LOGGER_READY,		/* Initialized but not active*/
	LOGGER_UNINIT,
	LOGGER_LOG_FULL,
};

struct logger_event {
	uint64_t timestamp_tcs;
	int type;
};


struct asgard_logger {

	int ifindex;

	u16 instance_id;

	enum logger_state state;

	char *name;

	int current_entries;

	/* Size is defined by LOGGER_EVENT_LIMIT */
	struct logger_event *events;

	int accept_user_ts;

	int applied;
    uint64_t first_ts;
    uint64_t last_ts;

    struct proc_dir_entry *ctrl_logger_entry;
    struct proc_dir_entry *log_logger_entry;
};

enum w_state {
	WARMED_UP = 0,
	WARMING_UP = 1,
};

enum asgard_rx_state {
	ASGARD_RX_DISABLED = 0,
	ASGARD_RX_ENABLED = 1,
};

enum asgard_protocol_type {
	ASGARD_PROTO_ECHO = 0,
	ASGARD_PROTO_FD = 1,
	ASGARD_PROTO_CONSENSUS = 2,

};

enum asgard_pacemaker_test_state {
	ASGARD_PM_TEST_UNINIT = 0,
	ASGARD_PM_TEST_INIT = 1,
};

enum pmstate {
	ASGARD_PM_UNINIT = 0,
	ASGARD_PM_READY = 1,
	ASGARD_PM_EMITTING = 2,
    ASGARD_PM_FAILED = 3,
};

struct asgard_process_info {
	u8 pid; /* Process ID on remote host */
	u8 ps; /* Status of remote process */
};

struct node_addr {
	int cluster_id;
	u32 dst_ip;
	unsigned char dst_mac[6];
};


struct asgard_payload {

	/* The number of protocols that are included in this payload.
	 * If 0, then this payload is interpreted as "noop" operation
	 *		.. for all local proto instances
	 */
	u16 protocols_included;

	/* Pointer to the first protocol payload.
	 *
	 * TODO: The first value of the protocol payload must be a protocol id of type u8.
	 * TODO: Protocols with a variable payload size (e.g. consensus lead)
	 * must include the offset to the next protocol payload as u16 directly after the first value.
	 *
	 * TODO: Check on creation if protocol fits in the payload (MAX_ASGARD_PAYLOAD_BYTES).
	 * If protocol payload does not fit in the asgard payload,
	 * then the protocol payload is queued to be stored in the next asgard payload.
	 */
	char proto_data[MAX_ASGARD_PAYLOAD_BYTES - 2];
};


struct asgard_packet_data {

	struct node_addr naddr;

	struct asgard_payload *payload;

	spinlock_t lock;

    struct sk_buff *skb;

    int port;
};

struct asgard_pacemaker_test_data {
	enum asgard_pacemaker_test_state state;
	int active_processes; /* Number of active processes */
	struct asgard_process_info pinfos[MAX_PROCESSES_PER_HOST];
};

struct asgard_pm_target_info {
	int fire;

	int pkt_tx_counter;
	int pkt_rx_counter;

	int scheduled_log_replications;
	int received_log_replications;

	/* Timestamp of heartbeat from last check*/
	uint64_t lhb_ts;

	/* Timestamp refreshed by the reception of the last packet from this node
	 * If chb_ts == lhb_ts: remote node is considered dead
	 * If chb_ts != lhb_ts: remote node is considered alive
	 */
	uint64_t chb_ts;

	/* 1 if node is considered alive
	 * 0 if node is considered dead
	 * node considered dead if lhb_ts did not change since X local interval
	 * X=resp_factor
	 */
	int alive;

	/* responsiveness factor
	 *
	 * Determines the factor how many local intervals to wait
	 * before the node is considered dead
	 */
	int resp_factor;

	/* Used to determine how many intervals the PM has already waited
	 * Will be reset to resp_factor after every aliveness check
	 */
	int cur_waiting_interval;

	/* Params used to build the SKB for TX */
	struct asgard_packet_data pkt_data;

	/* async packet queue for pm target*/
	struct asgard_async_queue_priv *aapriv;

    int pkt_tx_errors;
};

struct pminfo {
	enum pmstate state;

	int active_cpu;

	u32 cluster_id;

	int num_of_targets;

	//enum hb_interval hbi;
	// 2.4 GHz (must be fixed)
	uint64_t hbi;

	/* interval*/
	uint64_t waiting_window;

	struct asgard_pm_target_info pm_targets[MAX_REMOTE_SOURCES];

	/* Test Data */
	struct asgard_pacemaker_test_data tdata;

	struct hrtimer pm_timer;

    int errors;
    struct sk_buff *multicast_skb;

    // For Heartbeats
    struct asgard_packet_data multicast_pkt_data;

    // For out of schedule multicast
    struct asgard_packet_data multicast_pkt_data_oos;
    int multicast_pkt_data_oos_fire;
};

struct proto_instance;

struct multicast {
    /*Internal Usage*/
    int delay;
    int enable;
    /*Internal Usage*/
    struct asgard_logger logger;
    struct asgard_async_queue_priv *aapriv;
    int nextIdx;
};


struct asgard_device {
	int ifindex; /* corresponds to ifindex of net_device */

    u32 hb_interval;

	int asgard_id;
	int hold_fire;
	int cur_leader_lid;
	int bug_counter;

	struct consensus_priv *consensus_priv;
    void *echo_priv;

	int is_leader; /* Is this node a leader? */

	// 3319 for normal traffic, 3320 for leader traffic.
	int tx_port;

	uint64_t last_leader_ts;

	int verbose; /* Prints more information when set to 1 during RX/TX to dmesg*/

	enum asgard_rx_state rx_state;
	enum tsstate ts_state;
	enum w_state warmup_state;

	struct asgard_stats *stats;

	struct net_device *ndev;

	/* ASGARD CTRL Structures */
	struct pminfo pminfo;

	int instance_id_mapping[MAX_PROTO_INSTANCES];
	int num_of_proto_instances;
	struct proto_instance **protos; // array of ptrs to protocol instances

	struct workqueue_struct *asgard_leader_wq;
    struct cluster_info *ci;
    struct synbuf_device* synbuf_clustermem;
    struct workqueue_struct *asgard_ringbuf_reader_wq;
    struct proc_dir_entry *rx_ctrl_entry;
    struct proc_dir_entry *debug_entry;
    struct proc_dir_entry *proto_instances_ctrl_entry;
    struct proc_dir_entry *pacemaker_cpu_entry;
    struct proc_dir_entry *pacemaker_targets_entry;
    struct proc_dir_entry *pacemaker_waiting_window_entry;
    struct proc_dir_entry *pacemaker_hbi_entry;
    struct proc_dir_entry *pacemaker_ctrl_entry;
    struct proc_dir_entry *pacemaker_payload_entry;
    struct proc_dir_entry *pacemaker_cluster_id_entry;
    struct proc_dir_entry *multicast_delay_entry;
    struct proc_dir_entry *multicast_enable_entry;
    struct multicast multicast;

    u32 multicast_ip;
    unsigned char *multicast_mac;

    u32 self_ip;
    unsigned char *self_mac;
};

struct asgard_protocol_ctrl_ops {
	int (*init_ctrl)(void);

	/* Initializes data and user space interfaces */
	int (*init)(struct proto_instance *ins);

	int (*init_payload)(void *payload);

	int (*start)(struct proto_instance *ins);

	int (*stop)(struct proto_instance *ins);

	int (*us_update)(struct proto_instance *ins, void *payload);

	/* free memory of app and remove user space interfaces */
	int (*clean)(struct proto_instance *ins);

	int (*post_payload)(struct proto_instance *ins, int remote_lid, int cluster_id,
			    void *payload);

	int (*post_ts)(struct proto_instance *ins, unsigned char *remote_mac,
		       uint64_t ts);

	/* Write statistics to debug console  */
	int (*info)(struct proto_instance *ins);
};


struct proto_instance {

	u16 instance_id;

	enum asgard_protocol_type proto_type;

    struct asgard_logger logger;

    struct asgard_logger user_a;
    struct asgard_logger user_b;
    struct asgard_logger user_c;
    struct asgard_logger user_d;

	struct asgard_protocol_ctrl_ops ctrl_ops;

	void *proto_data;
};

struct retrans_request {
	s32 request_idx;
	struct list_head retrans_req_head;
};

struct asgard_pkt_work_data {
    struct work_struct work;

	struct asgard_device *sdev;
	int remote_lid;
	int rcluster_id;
	char *payload;
	int received_proto_instances;
	u32 cqe_bcnt;
    u16 headroom;

};
struct asgard_leader_pkt_work_data {
    struct work_struct work;
	struct asgard_device *sdev;

	int target_id;
	s32 next_index;
	int retrans;

};

struct asgard_ringbuf_read_work_data {
    struct delayed_work dwork;
    struct asg_ring_buf *rb;
    struct asgard_device *sdev;
};


//void asgard_setup_skbs(struct pminfo *spminfo);
void pm_state_transition_to(struct pminfo *spminfo,
			    enum pmstate state);
const char *pm_state_string(enum pmstate state);

int asgard_pm_reset(struct pminfo *spminfo);
int asgard_pm_stop(struct pminfo *spminfo);
int asgard_pm_start_loop(void *data);

void init_asgard_pm_ctrl_interfaces(struct asgard_device *sdev);
void clean_asgard_pm_ctrl_interfaces(struct asgard_device *sdev);

void asgard_hex_to_ip(char *retval, u32 dst_ip);
struct asgard_payload *get_payload_ptr(struct asgard_async_pkt *pkt);

/*
 * Converts an IP address from dotted numbers string to hex.
 */
u32 asgard_ip_convert(const char *str);

/*
 * Converts an MAC address to hex char array
 */
unsigned char *asgard_convert_mac(const char *str);


struct sk_buff *asgard_reserve_skb(struct net_device *dev, u32 dst_ip, unsigned char *dst_mac, struct asgard_payload *payload);

struct net_device *asgard_get_netdevice(int ifindex);

int asgard_validate_asgard_device(int asgard_id);
void asgard_reset_remote_host_counter(int asgard_id);

void asgard_post_payload(int asgard_id, void *payload, u16 headroom, u32 cqe_bcnt);

const char *asgard_get_protocol_name(enum asgard_protocol_type protocol_type);

struct proto_instance *generate_protocol_instance(struct asgard_device *sdev, int protocol_id);

void clean_asgard_ctrl_interfaces(struct asgard_device *sdev);
void init_asgard_ctrl_interfaces(struct asgard_device *sdev);

void asgard_post_ts(int asgard_id, uint64_t cycles, int leader_queue);

void ts_state_transition_to(struct asgard_device *sdev,
			    enum tsstate state);

int asgard_ts_stop(struct asgard_device *sdev);
int asgard_ts_start(struct asgard_device *sdev);
int asgard_reset_stats(struct asgard_device *sdev);

int asgard_write_timestamp(struct asgard_device *sdev,
				int logid, uint64_t cycles, int target_id);

const char *ts_state_string(enum tsstate state);
int init_timestamping(struct asgard_device *sdev);
void init_asgard_ts_ctrl_interfaces(struct asgard_device *sdev);
int asgard_clean_timestamping(struct asgard_device *sdev);

int write_log(struct asgard_logger *slog, int type, uint64_t tcs);

struct asgard_device *get_sdev(int devid);

void send_pkt(struct net_device *ndev, struct sk_buff *skb);

int is_ip_local(struct net_device *dev,	u32 ip_addr);

struct proto_instance *get_consensus_proto_instance(struct asgard_device *sdev);
struct proto_instance *get_fd_proto_instance(struct asgard_device *sdev);
struct proto_instance *get_echo_proto_instance(struct asgard_device *sdev);

void get_cluster_ids(struct asgard_device *sdev, unsigned char *remote_mac, int *lid, int *cid);
//void set_le_noop(struct asgard_device *sdev, unsigned char *pkt);
int compare_mac(unsigned char *m1, unsigned char *m2);

void init_logger_ctrl(struct asgard_logger *slog);


int init_logger(struct asgard_logger *slog, u16 i, int i1, char string[MAX_LOGGER_NAME], int accept_user);
void clear_logger(struct asgard_logger *slog);

int asgard_log_stop(struct asgard_logger *slog);
int asgard_log_start(struct asgard_logger *slog);
int asgard_log_reset(struct asgard_logger *slog);

const char *logger_state_string(enum logger_state state);
void set_all_targets_dead(struct asgard_device *sdev);
void remove_logger_ifaces(struct asgard_logger *slog);

void init_proto_instance_ctrl(struct asgard_device *sdev);
void remove_proto_instance_ctrl(struct asgard_device *sdev);

int asgard_core_register_remote_host(int asgard_id, u32 ip, char *mac,
				    int protocol_id, int cluster_id);
void update_leader(struct asgard_device *sdev, struct pminfo *spminfo);
void update_alive_msg(struct asgard_device *sdev, struct asgard_payload *pkt_payload);

void _schedule_update_from_userspace(struct asgard_device *sdev, struct synbuf_device *syndev);

#endif /* _ASGARD_H_ */
