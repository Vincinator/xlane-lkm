#ifndef _SASSY_H_
#define _SASSY_H_

#include <linux/list.h>
#include <linux/spinlock_types.h>


#define MAX_SYNCBEAT_PROC_NAME  256

#define SASSY_TARGETS_BUF 512
#define SASSY_NUMBUF 13


#define MAX_REMOTE_SOURCES 16

#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_5MS 12000000
#define CYCLES_PER_10MS 24000000
#define CYCLES_PER_100MS 240000000

#define MAX_PROCESSES_PER_HOST 16


#define SASSY_MLX5_DEVICES_LIMIT 5 	/* Number of allowed mlx5 devices that can connect to SASSY */
#define MAX_CPU_NUMBER 55

int sassy_core_register_nic(int ifindex);


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
	u8 pid;		/* Process ID on remote host */
	u8 ps;		/* Status of remote process */
};


struct sassy_heartbeat_payload {
	u8 message;							/* short message bundled with this hb */
	u8 alive_rp;						/* Number of alive processes */
	struct sassy_process_info pinfo[MAX_PROCESSES_PER_HOST];
};


struct sassy_hb_packet_params {
	uint32_t dst_ip; 			
	unsigned char dst_mac[6]; 

	/* pacemaker MUST copy in every loop. 
	 * Thus, we need a copy of hb_payload to continue the copy in the pacemaker
	 * even when a new version is currently written.
	 *
	 * The hb_active_ix member of this struct tells which index can be used by the pacemaker.
	 */
	struct sassy_heartbeat_payload hb_payload[2];
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
};

struct sassy_pacemaker_test_data {
	enum sassy_pacemaker_test_state state;
	int active_processes; 		/* Number of active processes */
	struct sassy_process_info	pinfos[MAX_PROCESSES_PER_HOST];
};

struct sassy_pm_target_info {
	int target_id;
	int active; 

	/* Params used to build the SKB for TX */
	struct sassy_hb_packet_params hb_pkt_params;

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
};


struct sassy_protocol;

struct sassy_device {
	int ifindex;					/* corresponds to ifindex of net_device */	
	int sassy_id;

	struct net_device *ndev;

	/* SASSY CTRL Structures */
	struct sassy_pacemaker_info 	pminfo;	

	/* Can only use one protocol at a time. */
	struct sassy_protocol *proto;

};

struct sassy_protocol_ctrl_ops {

	int (*init_ctrl)(void);

	/* Initializes data and user space interfaces */
	int (*init)(struct sassy_device*);

	int (*start)(struct sassy_device*);

	int (*stop)(struct sassy_device*);

	/* free memory of app and remove user space interfaces */
	int (*clean) (struct sassy_device*);

	/* Write statistics to debug console  */
	int (*info)(struct sassy_device*);

};

struct sassy_protocol {

	enum sassy_protocol_type proto_type;

	char *name;

	struct sassy_protocol_ctrl_ops ctrl_ops;

    struct list_head listh;

	/* private data of protocol handling */
	void *priv;

};



struct sk_buff *sassy_setup_hb_packet(struct sassy_pacemaker_info *spminfo, int host_number);
//void sassy_setup_skbs(struct sassy_pacemaker_info *spminfo);
void pm_state_transition_to(struct sassy_pacemaker_info *spminfo, enum sassy_pacemaker_state state);
const char *pm_state_string(sassy_pacemaker_state_t state);


int sassy_pm_reset(struct sassy_pacemaker_info *spminfo);
int sassy_pm_stop(struct sassy_pacemaker_info *spminfo);
int sassy_pm_start(struct sassy_pacemaker_info *spminfo);

void init_sassy_pm_ctrl_interfaces(struct sassy_device *sdev);
void clean_sassy_pm_ctrl_interfaces(struct sassy_device *sdev);



void sassy_hex_to_ip(char *retval, int dst_ip);

/*
 * Converts an IP address from dotted numbers string to hex.
 */
int sassy_ip_convert(const char *str);

/*
 * Converts an MAC address to hex char array
 */
unsigned char *sassy_convert_mac(const char *str);

struct sk_buff *compose_heartbeat_skb(struct net_device *dev, struct sassy_pacemaker_info *spminfo, int host_number);

struct net_device *sassy_get_netdevice(int ifindex);

int sassy_mlx5_con_register_device(int ifindex);

int sassy_mlx5_con_register_channel(int sassy_id, int ix, int cqn);
int sassy_mlx5_con_check_cqn(int sassy_id, int cqn);
int sassy_mlx5_con_check_ix(int sassy_id, int ix);



int sassy_mlx5_post_optimistical_timestamp(int sassy_id, uint64_t cycle_ts);
int sassy_mlx5_post_payload(int sassy_id, void *va, u32 frag_size, u16 headroom, u32 cqe_bcnt);

int sassy_core_register_remote_host(int sassy_id, uint32_t ip, char *mac);

int sassy_validate_sassy_device(int sassy_id);
void sassy_reset_remote_host_counter(int sassy_id);

void sassy_post_payload(int sassy_id, unsigned char *remote_mac, struct sassy_heartbeat_payload *hb_payload);


int sassy_register_protocol(struct sassy_protocol *proto);
int sassy_remove_protocol(struct sassy_protocol *proto);

const char *sassy_get_protocol_name(enum sassy_protocol_type protocol_type);

struct sassy_protocol* sassy_find_protocol_by_id(int protocol_id) 

#endif /* _SASSY_H_ */