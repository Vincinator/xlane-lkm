#pragma once



#ifndef ASGARD_KERNEL_MODULE
#include <pthread.h>
#include <netinet/in.h>
#include "list.h"
#endif

#include "types.h"
#include "config.h"
#include "types.h"
#include "logger.h"


#ifdef ASGARD_DPDK
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#endif

#ifndef ASG_CHUNK_SIZE
/* Use 64 Bits (8 Bytes) as default chunk size for asgard
 *
 * Must be longer than 8 bits (1byte)!
 */
#define ASG_CHUNK_SIZE 8
#endif

#define MAX_REMOTE_SOURCES 16
#define MAX_PROTO_INSTANCES 8
#define ASGARD_HEADER_BYTES 82
#define MAX_ASGARD_PAYLOAD_BYTES (1500 - ASGARD_HEADER_BYTES) // asuming an ethernet mtu of ~1500 bytes

#define ASGARD_PAYLOAD_BYTES MAX_ASGARD_PAYLOAD_BYTES
#define ASGARD_PKT_BYTES (ASGARD_PAYLOAD_BYTES + ASGARD_HEADER_BYTES)

#define MAX_PROTOCOLS 4

#define MAX_NODE_ID 10


enum asgard_protocol_type {
    ASGARD_PROTO_ECHO = 0,
    ASGARD_PROTO_FD = 1,
    ASGARD_PROTO_CONSENSUS = 2,

};


struct proto_instance;

struct asgard_protocol_ctrl_ops {
#ifdef ASGARD_KERNEL
    int (*init_ctrl)(void);
#endif
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
    uint16_t instance_id;

    enum asgard_protocol_type proto_type;

    struct asgard_ingress_logger ingress_logger;

    struct asgard_logger logger;
    struct asgard_logger user_a;
    struct asgard_logger user_b;
    struct asgard_logger user_c;
    struct asgard_logger user_d;

    struct asgard_protocol_ctrl_ops ctrl_ops;

    void *proto_data;
};




struct asgard_async_queue_priv {

    /* List head for asgard async pkt*/
    struct list_head head_of_async_pkt_queue;

    int doorbell;

    asg_rwlock_t queue_rwlock;
};

struct multicast {
    int enable;
    struct asgard_logger logger;
    struct asgard_async_queue_priv *aapriv;
    int nextIdx;
};

enum pmstate {
    ASGARD_PM_UNINIT = 0,
    ASGARD_PM_READY = 1,
    ASGARD_PM_EMITTING = 2,
    ASGARD_PM_FAILED = 3,
};


enum tsstate {
    ASGARD_TS_RUNNING,
    ASGARD_TS_READY,		/* Initialized but not active*/
    ASGARD_TS_UNINIT,
    ASGARD_TS_LOG_FULL,
};

enum w_state {
    WARMED_UP = 0,
    WARMING_UP = 1,
};

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

typedef enum node_state {
    FOLLOWER = 0,
    CANDIDATE = 1,
    LEADER = 2,
} node_state_t;

enum le_state {
    LE_RUNNING = 0,
    LE_READY = 1,
    LE_UNINIT = 2,
};

enum asgard_rx_state {
    ASGARD_RX_DISABLED = 0,
    ASGARD_RX_ENABLED = 1,
};

struct sm_log_entry {

    /* determines if this entry is valid to read */
    uint8_t valid;

    /* Term in which this command was appended to the log
     */
    uint32_t term;

    /* The command for the state machine.
     * Contains the required information to change the state machine.
     *
     * A ordered set of commands applied to the state machine will
     * transition the state machine to a common state (shared across the cluster).
     */
    struct data_chunk *dataChunk;
};


struct state_machine_cmd_log {

    int num_retransmissions[MAX_NODE_ID];

    int unstable_commits;

    int32_t next_index[MAX_NODE_ID];

    int32_t match_index[MAX_NODE_ID];

    struct list_head retrans_head[MAX_NODE_ID];

    asg_rwlock_t retrans_list_lock[MAX_NODE_ID];

    int retrans_entries[MAX_NODE_ID];

    int32_t last_applied;

    /* Index of the last valid entry in the entries array
     */
    int32_t last_idx;

    /* Index of the last commited entry in the entries array
     */
    int32_t commit_idx;

    /* Index of the last entry with no previous missing entries
     */
    int32_t stable_idx;

    int32_t next_retrans_req_idx;

    /* Maximum index of the entries array
     */
    int32_t max_entries;

    /* locked if node currently writes to this consensus log.
     * this lock prevents the race condition when a follower can not keep up
     * with the updates from the current leader.
     */
    int lock;

#ifdef ASGARD_KERNEL_MODULE
    asg_spinlock_t slock;
#endif

    asg_mutex_t mlock;

    /* Lock to prevent creation of multiple append entries for the same next_index */
    asg_mutex_t next_lock;

    struct sm_log_entry **entries;

    int turn;
};

struct consensus_priv {

    struct asgard_device *sdev;

    struct proto_instance *ins;

    // last leader timestmap before current follower timeout
    uint64_t llts_before_ftime;

    node_state_t nstate;

    enum le_state state;

    int candidate_counter;

    uint32_t leader_id;

    uint32_t node_id;

    /* index of array is node_id,
     * value at index of array is index to pm_targets
     */
    int cluster_mapping[MAX_NODE_ID];

    int32_t term;

    uint64_t accu_rand;

    /* last term this node has voted in. Initialized with -1*/
    uint32_t voted;

    uint32_t started_log;


    int max_entries_per_pkt;

    // number of followers voted for this node
    int votes;
    asg_mutex_t accept_vote_lock;

    struct state_machine_cmd_log sm_log;
    struct asgard_logger throughput_logger;

    // Used to correlate dmesg log output with evaluation results


    struct asg_ring_buf *txbuf;
    struct asg_ring_buf *rxbuf;

};

struct asgard_payload {

    /* The number of protocols that are included in this payload.
     * If 0, then this payload is interpreted as "noop" operation
     *		.. for all local proto instances
     */
    uint16_t protocols_included;

    /* Pointer to the first protocol payload.
     *
     * TODO: The first value of the protocol payload must be a protocol id of type uint8_t.
     * TODO: Protocols with a variable payload size (e.g. consensus lead)
     * must include the offset to the next protocol payload as uint16_t directly after the first value.
     *
     * TODO: Check on creation if protocol fits in the payload (MAX_ASGARD_PAYLOAD_BYTES).
     * If protocol payload does not fit in the asgard payload,
     * then the protocol payload is queued to be stored in the next asgard payload.
     */
    unsigned char proto_data[MAX_ASGARD_PAYLOAD_BYTES - 2];
};

struct node_addr {
    int cluster_id;
    uint32_t dst_ip;
    uint32_t port;
    unsigned char dst_mac[6];
};


struct asgard_packet_data {

    struct node_addr naddr;

    struct asgard_payload *payload;

    int fire;

    asg_mutex_t mlock;
#ifdef ASGARD_KERNEL_MODULE
    asg_spinlock_t slock;
    struct sk_buff *skb;
#endif
};

struct asgard_pm_target_info {
    int fire;

    int cluster_id;

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

    asg_mac_ptr_t mac_addr;
};


struct pminfo {
    enum pmstate state;

    int active_cpu;

    uint32_t cluster_id;

    int num_of_targets;

    //enum hb_interval hbi;
    // 2.4 GHz (must be fixed)
    uint64_t hbi;

    /* interval*/
    uint64_t waiting_window;

    struct asgard_pm_target_info pm_targets[MAX_REMOTE_SOURCES];

    /* Test Data */
    // struct asgard_pacemaker_test_data tdata;

    // struct hrtimer pm_timer;

    int errors;
    struct sk_buff *multicast_skb;

    // For Heartbeats
    struct asgard_packet_data multicast_pkt_data;

    // For out of schedule multicast
    struct asgard_packet_data multicast_pkt_data_oos;
    int multicast_pkt_data_oos_fire;
};



struct asgard_device {
    int ifindex; /* corresponds to ifindex of net_device */


#ifdef ASGARD_KERNEL_MODULE
    struct net_device *ndev;
#endif
    uint32_t hb_interval;

    uint64_t tx_counter;
    uint16_t dpdk_portid;

    int asgard_id;
    int hold_fire;
    int cur_leader_lid;
    int bug_counter;

    struct rte_mempool *pktmbuf_pool;
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

    // struct net_device *ndev;

    /* ASGARD CTRL Structures */
    struct pminfo pminfo;

    int instance_id_mapping[MAX_PROTO_INSTANCES];
    int num_of_proto_instances;
    struct proto_instance **protos; // array of ptrs to protocol instances

    struct workqueue_struct *asgard_leader_wq;
    struct cluster_info *ci;
    struct synbuf_device* synbuf_clustermem;
    struct workqueue_struct *asgard_ringbuf_reader_wq;

#ifdef ASGARD_KERNEL_MODULE
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
#endif
    struct multicast multicast;

    // uint32_t multicast_ip;
    // unsigned char *multicast_mac;

    uint32_t self_ip;

    asg_mac_ptr_t self_mac;

    /* Ensures that all targets are first checked before the */
    asg_mutex_t logrep_schedule_lock;
};


unsigned char *asgard_convert_mac(const char *str);
uint32_t asgard_ip_convert(const char *str);
void init_asgard_device(struct asgard_device *sdev);
struct proto_instance *generate_protocol_instance(struct asgard_device *sdev, int protocol_id);
