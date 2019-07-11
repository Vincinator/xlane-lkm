#ifndef _SASSY_H_
#define _SASSY_H_


#define MAX_SYNCBEAT_PROC_NAME  64

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

enum sassy_pacemaker_state {
	SASSY_PM_UNINIT = 0,
	SASSY_PM_READY = 1,
	SASSY_PM_EMITTING = 2,
};

typedef enum sassy_pacemaker_state sassy_pacemaker_state_t;



struct sassy_remote_process_info {
	u8 rpid;	/* Process ID on remote host */
	u8 rps;		/* Status of remote process */
};


struct sassy_heartbeat_payload {
	u8 message;							/* short message bundled with this hb */
	u8 alive_rp;						/* Number of alive processes */
	struct sassy_remote_process_info rpinfo[MAX_PROCESSES_PER_HOST];
};


struct sassy_hb_packet_params {
	/* Parameters used to Build the SKB */
	uint32_t dst_ip; 			
	unsigned char *dst_mac; 

	/* Payload  */
	struct sassy_heartbeat_packet *hb_payload;

};

struct sassy_mlx5_con_info {
	int ix;
	int cqn;
};

struct sassy_pm_target_info {
	int target_id;

	/* Params used to build the SKB for TX */
	struct sassy_hb_packet_params *hb_pkt_params;

	/* Data for transmitting the packet  */
    struct sk_buff *skb;
    struct netdev_queue *txq;
}


struct sassy_pacemaker_info {
	enum sassy_pacemaker_state state;

	int active_cpu;

	int num_of_targets;
	struct sassy_pm_target_info pm_targets[MAX_REMOTE_SOURCES];
};

struct sassy_device {
	int ifindex;					/* corresponds to ifindex of net_device */	
	int sassy_id;

	struct netdevice *ndev;

	/* SASSY CTRL Structures */
	struct sassy_pacemaker_info 	pminfo;	

};



struct sk_buff *sassy_setup_hb_packet(struct sassy_pacemaker_info *spminfo, int host_number);
void sassy_setup_skbs(struct sassy_pacemaker_info *spminfo);
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

struct netdevice *sassy_get_netdevice(int ifindex);

int sassy_mlx5_con_register_device(int ifindex);

int sassy_mlx5_con_register_channel(int sassy_id, int ix, int cqn);
int sassy_mlx5_con_check_cqn(int sassy_id, int cqn);
int sassy_mlx5_con_check_ix(int sassy_id, int ix);



int sassy_mlx5_post_optimistical_timestamp(int sassy_id, uint64_t cycle_ts);
int sassy_mlx5_post_payload(int sassy_id, void *va, u32 frag_size, u16 headroom, u32 cqe_bcnt);



#endif /* _SASSY_H_ */