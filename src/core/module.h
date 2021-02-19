#pragma once


#define MAX_ASGARD_PROC_NAME 256
#define MAX_PROCFS_BUF 512
#define ASGARD_NUMBUF 13
#define ASGARD_TARGETS_BUF 512

#define MAX_CPU_NUMBER 55
#define MAX_CLUSTER_MEMBER 512 // arbitrary limit

#define MIN_HB_CYCLES 1000
#define MAX_HB_CYCLES 1000000000

#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_5MS 12000000
#define CYCLES_PER_10MS 24000000
#define CYCLES_PER_100MS 240000000

#define TIMESTAMP_ARRAY_LIMIT 100000
#define ASGARD_NUM_TS_LOG_TYPES 8
#define LE_EVENT_LOG_LIMIT 100000



#include "libasraft.h"


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

/*
 * Each NIC port gets a unique table.
 * This struct holds references to all tables.
 * ifindex of NIC PORT corresponds to array position of struct asgard_rx_table *tables.
 */
struct asgard_core {

    /* NIC specific Data */
    struct asgard_device **sdevices;

    /* Number of registered asgard devices */
    int num_devices;
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
    u64 ots;

};

struct asgard_ringbuf_read_work_data {
    struct delayed_work dwork;
    struct asg_ring_buf *rb;
    struct asgard_device *sdev;
};





struct asgard_device *get_sdev(int devid);
void asg_init_workqueues(struct asgard_device *sdev);
int is_ip_local(struct net_device *dev,	u32 ip_addr);
void clear_protocol_instances(struct asgard_device *sdev);
const char *asgard_get_protocol_name(enum asgard_protocol_type protocol_type);
int asgard_core_register_remote_host(int asgard_id, u32 ip, char *mac,
                                     int protocol_id, int cluster_id);