#pragma once

#include <sassy/sassy.h>


int follower_process_pkt(int sassy_id, void* pkt);
int reset_timeout(int sassy_id);
int stop_follower(int sassy_id);
int start_follower(int sassy_id);
ktime_t get_rnd_timeout(void);