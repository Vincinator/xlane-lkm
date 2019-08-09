#pragma once

#include <sassy/sassy.h>


int follower_process_pkt(struct sassy_device *sdev, void* pkt);
int reset_timeout(void);
int stop_follower(void);
int start_follower(void);
ktime_t get_rnd_timeout(void);