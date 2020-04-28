#ifndef _ASGUARD_H_
#define _ASGUARD_H_

#include <linux/workqueue.h>
#include <asguard/ringbuffer.h>


#define VERBOSE_DEBUG 1

#ifndef CONFIG_KUNIT
#define RDTSC_ASGUARD rdtsc()
#else
#define RDTSC_ASGUARD 0ULL
#endif


#include <linux/list.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <asguard/asguard_async.h>

#include "logger.h"

#define MAX_ASGUARD_PROC_NAME 256

#define MAX_PROCFS_BUF 512


#define ASGUARD_UUID_BUF 42

#define ASGUARD_TARGETS_BUF 512
#define ASGUARD_NUMBUF 13

#define MAX_REMOTE_SOURCES 16

#define MIN_HB_CYCLES 1000
#define MAX_HB_CYCLES 1000000000

#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_5MS 12000000
#define CYCLES_PER_10MS 24000000
#define CYCLES_PER_100MS 240000000

#define MAX_PROCESSES_PER_HOST 16

#define ASGUARD_MLX5_DEVICES_LIMIT                                               \
	5 /* Number of allowed mlx5 devices that can connect to ASGUARD */
#define MAX_CPU_NUMBER 55

#define ASGUARD_HEADER_BYTES 128

#define MAX_ASGUARD_PAYLOAD_BYTES (1500 - ASGUARD_HEADER_BYTES) // asuming an ethernet mtu of ~1500 bytes

#define ASGUARD_PAYLOAD_BYTES MAX_ASGUARD_PAYLOAD_BYTES
#define ASGUARD_PKT_BYTES (ASGUARD_PAYLOAD_BYTES + ASGUARD_HEADER_BYTES)

int asguard_core_register_nic(int ifindex, int asguard_id);
int asguard_core_remove_nic(int asguard_id);

#define ASGUARD_NUM_TS_LOG_TYPES 8
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


enum hb_interval {
	CYC_1MS = 1,
	CYC_5MS = 2,
	CYC_10MS = 3,
	CYC_100MS = 4,
};

enum tsstate {
    ASGUARD_TS_RUNNING,
    ASGUARD_TS_READY,		/* Initialized but not active*/
    ASGUARD_TS_UNINIT,
    ASGUARD_TS_LOG_FULL,
};

struct asguard_timestamp_item {
	uint64_t timestamp_tcs;
	//ktime_t correlation_id;
	int target_id; /* Host target id to which the heartbeat was send (if TX) */
};

struct asguard_timestamp_logs {
	struct asguard_timestamp_item *timestamp_items; /* Size is defined with TIMESTAMP_ARRAY_LIMIT macro*/
	int current_timestamps; /* How many timestamps are set in the timestamp */
	struct proc_dir_entry	*proc_dir;
	char *name;
};

struct asguard_stats {
	/* Array of timestamp logs
	 * - Each item corresponds to one timestamping type
	 * - Each item is a pointer to a asguard_timestamp_logs struct
	 */
	struct asguard_timestamp_logs **timestamp_logs;
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


struct asguard_logger {

	int ifindex;

	u16 instance_id;

	enum logger_state state;

	char *name;

	int current_entries;

	/* Size is defined by LOGGER_EVENT_LIMIT */
	struct logger_event *events;

	int accept_user_ts;

};

enum w_state {
	WARMED_UP = 0,
	WARMING_UP = 1,
};

enum asguard_rx_state {
	ASGUARD_RX_DISABLED = 0,
	ASGUARD_RX_ENABLED = 1,
};

enum asguard_protocol_type {
	ASGUARD_PROTO_ECHO = 0,
	ASGUARD_PROTO_FD = 1,
	ASGUARD_PROTO_CONSENSUS = 2,

};

enum asguard_pacemaker_test_state {
	ASGUARD_PM_TEST_UNINIT = 0,
	ASGUARD_PM_TEST_INIT = 1,
};

enum pmstate {
	ASGUARD_PM_UNINIT = 0,
	ASGUARD_PM_READY = 1,
	ASGUARD_PM_EMITTING = 2,
    ASGUARD_PM_FAILED = 3,
};

struct asguard_process_info {
	u8 pid; /* Process ID on remote host */
	u8 ps; /* Status of remote process */
};

struct node_addr {
	int cluster_id;
	u32 dst_ip;
	unsigned char dst_mac[6];
};


struct asguard_payload {

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
	 * TODO: Check on creation if protocol fits in the payload (MAX_ASGUARD_PAYLOAD_BYTES).
	 * If protocol payload does not fit in the asguard payload,
	 * then the protocol payload is queued to be stored in the next asguard payload.
	 */
	char proto_data[MAX_ASGUARD_PAYLOAD_BYTES - 2];
};


struct asguard_packet_data {
	struct node_addr naddr;

	struct asguard_payload *pkt_payload;

	spinlock_t pkt_lock;

	struct asguard_payload *hb_pkt_payload;

};

struct asguard_pacemaker_test_data {
	enum asguard_pacemaker_test_state state;
	int active_processes; /* Number of active processes */
	struct asguard_process_info pinfos[MAX_PROCESSES_PER_HOST];
};

struct asguard_pm_target_info {
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
	struct asguard_packet_data pkt_data;

	/* Data for transmitting the packet  */
	struct sk_buff *skb;

    /* Data for transmitting the packet  */
    struct sk_buff *skb_oos;

	/* async packet queue for pm target*/
	struct asguard_async_queue_priv *aapriv;

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

	struct asguard_pm_target_info pm_targets[MAX_REMOTE_SOURCES];

	/* Test Data */
	struct asguard_pacemaker_test_data tdata;

	struct hrtimer pm_timer;

    int errors;
    struct sk_buff *multicast_skb;
    struct asguard_packet_data multicast_pkt_data;
};

struct proto_instance;

struct asguard_device {
	int ifindex; /* corresponds to ifindex of net_device */

    u32 hb_interval;

	int asguard_id;
	int hold_fire;
	int cur_leader_lid;
	int bug_counter;

	struct consensus_priv *consensus_priv;

	int is_leader; /* Is this node a leader? */

	// 3319 for normal traffic, 3320 for leader traffic.
	int tx_port;

	uint64_t last_leader_ts;

	int verbose; /* Prints more information when set to 1 during RX/TX to dmesg*/

	enum asguard_rx_state rx_state;
	enum tsstate ts_state;
	enum w_state warmup_state;

	struct asguard_stats *stats;

	struct net_device *ndev;

	/* ASGUARD CTRL Structures */
	struct pminfo pminfo;

	int instance_id_mapping[MAX_PROTO_INSTANCES];
	int num_of_proto_instances;
	struct proto_instance **protos; // array of ptrs to protocol instances

	struct workqueue_struct *asguard_leader_wq;
    struct cluster_info *ci;
    struct synbuf_device* synbuf_clustermem;
    struct workqueue_struct *asguard_ringbuf_reader_wq;
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

    u32 multicast_ip;
    unsigned char *multicast_mac;

    u32 self_ip;
    unsigned char *self_mac;
};

struct asguard_protocol_ctrl_ops {
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

	enum asguard_protocol_type proto_type;

    struct asguard_logger logger;

    struct asguard_logger user_a;
    struct asguard_logger user_b;
    struct asguard_logger user_c;
    struct asguard_logger user_d;

	struct asguard_protocol_ctrl_ops ctrl_ops;

	void *proto_data;
};

struct retrans_request {
	s32 request_idx;
	struct list_head retrans_req_head;
};

struct asguard_pkt_work_data {
    struct work_struct work;

	struct asguard_device *sdev;
	int remote_lid;
	int rcluster_id;
	char *payload;
	int received_proto_instances;
	u32 cqe_bcnt;
    u16 headroom;

};
struct asguard_leader_pkt_work_data {
    struct work_struct work;
	struct asguard_device *sdev;

	int target_id;
	s32 next_index;
	int retrans;

};

struct asguard_ringbuf_read_work_data {
    struct delayed_work dwork;
    struct asg_ring_buf *rb;
    struct asguard_device *sdev;
};


//void asguard_setup_skbs(struct pminfo *spminfo);
void pm_state_transition_to(struct pminfo *spminfo,
			    enum pmstate state);
const char *pm_state_string(enum pmstate state);

int asguard_pm_reset(struct pminfo *spminfo);
int asguard_pm_stop(struct pminfo *spminfo);
int asguard_pm_start_loop(void *data);

void init_asguard_pm_ctrl_interfaces(struct asguard_device *sdev);
void clean_asguard_pm_ctrl_interfaces(struct asguard_device *sdev);

void asguard_hex_to_ip(char *retval, u32 dst_ip);
struct asguard_payload *get_payload_ptr(struct asguard_async_pkt *pkt);

/*
 * Converts an IP address from dotted numbers string to hex.
 */
u32 asguard_ip_convert(const char *str);

/*
 * Converts an MAC address to hex char array
 */
unsigned char *asguard_convert_mac(const char *str);


struct sk_buff *asguard_reserve_skb(struct net_device *dev, u32 dst_ip, unsigned char *dst_mac, struct asguard_payload *payload);

struct net_device *asguard_get_netdevice(int ifindex);

int asguard_validate_asguard_device(int asguard_id);
void asguard_reset_remote_host_counter(int asguard_id);

void asguard_post_payload(int asguard_id, void *payload, u16 headroom, u32 cqe_bcnt);

const char *asguard_get_protocol_name(enum asguard_protocol_type protocol_type);

struct proto_instance *generate_protocol_instance(struct asguard_device *sdev, int protocol_id);

void clean_asguard_ctrl_interfaces(struct asguard_device *sdev);
void init_asguard_ctrl_interfaces(struct asguard_device *sdev);

void asguard_post_ts(int asguard_id, uint64_t cycles, int leader_queue);

void ts_state_transition_to(struct asguard_device *sdev,
			    enum tsstate state);

int asguard_ts_stop(struct asguard_device *sdev);
int asguard_ts_start(struct asguard_device *sdev);
int asguard_reset_stats(struct asguard_device *sdev);

int asguard_write_timestamp(struct asguard_device *sdev,
				int logid, uint64_t cycles, int target_id);

const char *ts_state_string(enum tsstate state);
int init_timestamping(struct asguard_device *sdev);
void init_asguard_ts_ctrl_interfaces(struct asguard_device *sdev);
int asguard_clean_timestamping(struct asguard_device *sdev);

int write_log(struct asguard_logger *slog, int type, uint64_t tcs);

struct asguard_device *get_sdev(int devid);

void send_pkt(struct net_device *ndev, struct sk_buff *skb);

int is_ip_local(struct net_device *dev,	u32 ip_addr);

struct proto_instance *get_consensus_proto_instance(struct asguard_device *sdev);
struct proto_instance *get_fd_proto_instance(struct asguard_device *sdev);
struct proto_instance *get_echo_proto_instance(struct asguard_device *sdev);

void get_cluster_ids(struct asguard_device *sdev, unsigned char *remote_mac, int *lid, int *cid);
//void set_le_noop(struct asguard_device *sdev, unsigned char *pkt);
int compare_mac(unsigned char *m1, unsigned char *m2);

void init_logger_ctrl(struct asguard_logger *slog);


int init_logger(struct asguard_logger *slog, u16 i, int i1, char string[MAX_LOGGER_NAME], int accept_user);
void clear_logger(struct asguard_logger *slog);

int asguard_log_stop(struct asguard_logger *slog);
int asguard_log_start(struct asguard_logger *slog);
int asguard_log_reset(struct asguard_logger *slog);

const char *logger_state_string(enum logger_state state);
void set_all_targets_dead(struct asguard_device *sdev);
void remove_logger_ifaces(struct asguard_logger *slog);

void init_proto_instance_ctrl(struct asguard_device *sdev);
void remove_proto_instance_ctrl(struct asguard_device *sdev);

int asguard_core_register_remote_host(int asguard_id, u32 ip, char *mac,
				    int protocol_id, int cluster_id);
void update_leader(struct asguard_device *sdev, struct pminfo *spminfo);
void update_alive_msg(struct asguard_device *sdev, struct asguard_payload *pkt_payload);

void _schedule_update_from_userspace(struct asguard_device *sdev, struct synbuf_device *syndev);

#endif /* _ASGUARD_H_ */
