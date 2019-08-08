#ifndef _SASSY_H_
#define _SASSY_H_

#include <linux/list.h>
#include <linux/spinlock_types.h>

#include <sassy/sassy_ts.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>

#define MAX_SASSY_PROC_NAME 256



#define SASSY_TARGETS_BUF 512
#define SASSY_NUMBUF 13

#define MAX_REMOTE_SOURCES 16

#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_5MS 12000000
#define CYCLES_PER_10MS 24000000
#define CYCLES_PER_100MS 240000000

#define MAX_PROCESSES_PER_HOST 16

#define SASSY_MLX5_DEVICES_LIMIT                                               \
	5 /* Number of allowed mlx5 devices that can connect to SASSY */
#define MAX_CPU_NUMBER 55

#define SASSY_PAYLOAD_BYTES 64
#define SASSY_HEADER_BYTES                                                     \
	128 // TODO: this should be more than enough for UDP/ipv4
#define SASSY_PKT_BYTES SASSY_PAYLOAD_BYTES + SASSY_HEADER_BYTES

int sassy_core_register_nic(int ifindex);



#define SASSY_NUM_TS_LOG_TYPES 8
#define TIMESTAMP_ARRAY_LIMIT 100000


enum sassy_ts_state {
    SASSY_TS_RUNNING,
    SASSY_TS_READY, 	/* Initialized but not active*/
    SASSY_TS_UNINIT,
    SASSY_TS_LOG_FULL,
};

struct sassy_timestamp_item {
	uint64_t timestamp_tcs;
	//ktime_t correlation_id;
	int target_id; /* Host target id to which the heartbeat was send (if TX) */
};

struct sassy_timestamp_logs {
	struct sassy_timestamp_item *timestamp_items; /* Size is defined with TIMESTAMP_ARRAY_LIMIT macro*/
	int current_timestamps; /* How many timestamps are set in the timestamp */
	struct proc_dir_entry	*proc_dir;
	char *name;
};

struct sassy_stats {
	/* Array of timestamp logs
	 * - Each item corresponds to one timestamping type
	 * - Each item is a pointer to a sassy_timestamp_logs struct
	 */
	struct sassy_timestamp_logs **timestamp_logs;
	int timestamp_amount; /* how many different timestamps types are tracked*/
};

enum sassy_rx_state {
	SASSY_RX_DISABLED = 0,
	SASSY_RX_ENABLED = 1,
};

enum sassy_protocol_type {
	SASSY_PROTO_ECHO = 0,
	SASSY_PROTO_FD = 1,
	SASSY_PROTO_CONSENSUS = 2,
};

typedef enum sassy_protocol_type sassy_protocol_t;

enum sassy_pacemaker_test_state {
	SASSY_PM_TEST_UNINIT = 0,
	SASSY_PM_TEST_INIT = 1,
};

enum sassy_pacemaker_state {
	SASSY_PM_UNINIT = 0,
	SASSY_PM_READY = 1,
	SASSY_PM_EMITTING = 2,
};

typedef enum sassy_pacemaker_state sassy_pacemaker_state_t;

struct sassy_process_info {
	u8 pid; /* Process ID on remote host */
	u8 ps; /* Status of remote process */
};





struct sassy_packet_data {
	uint32_t dst_ip;
	unsigned char dst_mac[6];

	u8 protocol_id;

	/* pacemaker MUST copy in every loop. 
	 * Thus, we need a copy of pkt_payload to continue the copy in the pacemaker
	 * even when a new version is currently written.
	 *
	 * The hb_active_ix member of this struct tells which index can be used by the pacemaker.
	 * 
	 * Size of pkt_payload MUST be SASSY_PAYLOAD_BYTES 
	 */
	void *pkt_payload[2];

	int hb_active_ix;

	/* if updating != 0, then pacemaker will not update skb
	 * uses old values in skb until updating == 0 */
	int updating;

	/* pacemaker locks payload if it is using it */
	spinlock_t lock;
};

struct sassy_mlx5_con_info {
	int ix;
	int cqn;
	void *c; /* mlx5 channel */
};

struct sassy_pacemaker_test_data {
	enum sassy_pacemaker_test_state state;
	int active_processes; /* Number of active processes */
	struct sassy_process_info pinfos[MAX_PROCESSES_PER_HOST];
};

struct sassy_pm_target_info {
	int target_id;
	int active;

	/* Params used to build the SKB for TX */
	struct sassy_packet_data pkt_data;

	/* Data for transmitting the packet  */
	struct sk_buff *skb;
	struct netdev_queue *txq;
};

struct sassy_test_procfile_container {
	struct sassy_pacemaker_info *spminfo;
	int procid;
};

struct sassy_pacemaker_info {
	enum sassy_pacemaker_state state;

	int active_cpu;

	int num_of_targets;
	struct sassy_pm_target_info pm_targets[MAX_REMOTE_SOURCES];

	/* Test Data */
	struct sassy_pacemaker_test_data tdata;

	struct hrtimer pm_timer;

};

struct sassy_protocol;

struct sassy_device {
	int ifindex; /* corresponds to ifindex of net_device */
	int sassy_id;

	int verbose; /* Prints more information when set to 1 during RX/TX to dmesg*/

	enum sassy_rx_state rx_state;
	enum sassy_ts_state ts_state; 

	struct sassy_stats *stats;

	struct net_device *ndev;

	/* SASSY CTRL Structures */
	struct sassy_pacemaker_info pminfo;

	/* Can only use one protocol at a time. */
	struct sassy_protocol *proto;
};

struct sassy_protocol_ctrl_ops {
	int (*init_ctrl)(void);

	/* Initializes data and user space interfaces */
	int (*init)(const struct sassy_device *);

	int (*init_payload)(void *data);

	int (*start)(const struct sassy_device *);

	int (*stop)(const struct sassy_device *);

	int (*us_update)(const struct sassy_device *, void *payload);

	/* free memory of app and remove user space interfaces */
	int (*clean)(const struct sassy_device *);

	int (*post_payload)(const struct sassy_device *, unsigned char *remote_mac,
			    void *payload);

	int (*post_ts)(const struct sassy_device *, unsigned char *remote_mac,
		       uint64_t ts);

	/* Write statistics to debug console  */
	int (*info)(const struct sassy_device *);
};

struct sassy_protocol {
	enum sassy_protocol_type proto_type;

	char *name;

	struct sassy_protocol_ctrl_ops ctrl_ops;

	struct list_head listh;

	/* private data of protocol handling */
	void *priv;
};

struct sk_buff *sassy_setup_hb_packet(const struct sassy_pacemaker_info *spminfo,
				      int host_number);
//void sassy_setup_skbs(struct sassy_pacemaker_info *spminfo);
void pm_state_transition_to(const struct sassy_pacemaker_info *spminfo,
			    enum sassy_pacemaker_state state);
const char *pm_state_string(sassy_pacemaker_state_t state);

int sassy_pm_reset(const struct sassy_pacemaker_info *spminfo);
int sassy_pm_stop(const struct sassy_pacemaker_info *spminfo);
int sassy_pm_start_loop(void *data);
int sassy_pm_start_timer(void *data);

void init_sassy_pm_ctrl_interfaces(const struct sassy_device *sdev);
void clean_sassy_pm_ctrl_interfaces(const struct sassy_device *sdev);

void sassy_hex_to_ip(char *retval, int dst_ip);

/*
 * Converts an IP address from dotted numbers string to hex.
 */
int sassy_ip_convert(const char *str);

/*
 * Converts an MAC address to hex char array
 */
unsigned char *sassy_convert_mac(const char *str);

struct sk_buff *compose_heartbeat_skb(const struct net_device *dev,
				      const struct sassy_pacemaker_info *spminfo,
				      int host_number);

struct net_device *sassy_get_netdevice(int ifindex);

int sassy_mlx5_con_register_device(int ifindex);

/* c is void ptr to  struct mlx5e_channel *c  */
int sassy_mlx5_con_register_channel(int sassy_id, int ix, int cqn, void *c);
int sassy_mlx5_con_check_cqn(int sassy_id, int cqn);
int sassy_mlx5_con_check_ix(int sassy_id, int ix);

int sassy_mlx5_post_optimistical_timestamp(int sassy_id, uint64_t cycle_ts);
int sassy_mlx5_post_payload(int sassy_id, void *va, u32 frag_size, u16 headroom,
			    u32 cqe_bcnt);

int sassy_core_register_remote_host(int sassy_id, uint32_t ip, char *mac,
				    int protocol_id);

int sassy_validate_sassy_device(int sassy_id);
void sassy_reset_remote_host_counter(int sassy_id);

void sassy_post_payload(int sassy_id, unsigned char *remote_mac, void *payload);

int sassy_register_protocol(struct sassy_protocol *proto);
int sassy_remove_protocol(struct sassy_protocol *proto);

const char *sassy_get_protocol_name(enum sassy_protocol_type protocol_type);

struct sassy_protocol *sassy_find_protocol_by_id(u8 protocol_id);

void clean_sassy_ctrl_interfaces(const struct sassy_device *sdev);
void init_sassy_ctrl_interfaces(const struct sassy_device *sdev);

void sassy_post_ts(int sassy_id, uint64_t cycles);

void *sassy_mlx5_get_channel(int sassy_id);


void ts_state_transition_to(const struct sassy_device *sdev,
			    enum sassy_ts_state state);

int sassy_ts_stop(const struct sassy_device *sdev);
int sassy_ts_start(const struct sassy_device *sdev);
int sassy_reset_stats(const struct sassy_device *sdev);

int sassy_write_timestamp(const struct sassy_device *sdev,
				int logid, uint64_t cycles, int target_id);

const char *ts_state_string(enum sassy_ts_state state);
int init_timestamping(const struct sassy_device *sdev);
void init_sassy_ts_ctrl_interfaces(const struct sassy_device *sdev);

struct sassy_device *get_sdev(int devid);

#endif /* _SASSY_H_ */