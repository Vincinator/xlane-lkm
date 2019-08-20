#pragma once

#include <sassy/sassy.h>

#include "sassy_consensus.h"

int follower_process_pkt(struct sassy_device *sdev, struct sassy_payload* pkt);
void reset_ftimeout(struct sassy_device *sdev);
int stop_follower(struct sassy_device *sdev);
int start_follower(struct sassy_device *sdev);
void init_timeout(struct sassy_device *sdev);
