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

struct sassy_pacemaker_info {
	enum sassy_pacemaker_state state;

	int active_cpu;

	int num_of_targets;
	struct sassy_network_address_info targets[MAX_REMOTE_SOURCES];

};

struct sk_buff *sassy_setup_hb_packet(struct sassy_pacemaker_info *spminfo, int host_number);





#endif