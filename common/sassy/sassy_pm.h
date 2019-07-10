#ifndef _SASSY_PM_H_
#define _SASSY_PM_H_


#include <sassy/sassy.h>


#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_5MS 12000000
#define CYCLES_PER_10MS 24000000
#define CYCLES_PER_100MS 240000000


#define MAX_CPU_NUMBER 55

struct sassy_network_address_info {
	uint32_t dst_ip; 			
	unsigned char *dst_mac; 				
};

enum sassy_pacemaker_state {
	SASSY_PM_UNINIT = 0,
	SASSY_PM_READY = 1,
	SASSY_PM_EMITTING = 2,
};

typedef enum sassy_pacemaker_state sassy_pacemaker_state_t;

struct sassy_pacemaker_info {
	enum sassy_pacemaker_state state;

	int active_cpu;

	int num_of_targets;
	struct sassy_network_address_info targets[MAX_REMOTE_SOURCES];

};

struct sk_buff *sassy_setup_hb_packet(struct sassy_pacemaker_info *spminfo, int host_number);
void sassy_setup_skbs(struct sassy_pacemaker_info *spminfo);
void pm_state_transition_to(struct sassy_pacemaker_info *spminfo, enum sassy_pacemaker_state state);
const char *pm_state_string(sassy_pacemaker_state_t state);
void sassy_send_all_heartbeats(struct sassy_pacemaker_info *spminfo);


int sassy_pm_reset(struct sassy_pacemaker_info *spminfo);
int sassy_pm_stop(struct sassy_pacemaker_info *spminfo);
int sassy_pm_start(struct sassy_pacemaker_info *spminfo);

void init_sassy_pm_ctrl_interfaces(struct sassy_device *sdev);

#endif