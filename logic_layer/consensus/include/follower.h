#pragma once

#include <sassy/sassy.h>

#include "sassy_consensus.h"

int follower_process_pkt(struct sassy_device *sdev, void* pkt);
void reset_ftimeout(void);
int stop_follower(void);
int start_follower(void);
void init_timeout(void);
