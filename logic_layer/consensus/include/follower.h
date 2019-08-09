#pragma once

#include <sassy/sassy.h>


int follower_process_pkt(struct sassy_device *sdev, void* pkt);
int reset_timeout();
int stop_follower();
int start_follower();
ktime_t get_rnd_timeout(void);