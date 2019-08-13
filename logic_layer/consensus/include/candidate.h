#pragma once

#include <sassy/sassy.h>

struct nomination_pkt_data {
	enum cmsg_type msg_type;
	int candidate_id; 
	int term;
};


int candidate_process_pkt(struct sassy_device *sdev, void* pkt);
int start_candidate(void);
int stop_candidate(void);
int broadcast_nomination(void);


