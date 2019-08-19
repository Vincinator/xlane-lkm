#pragma once

#include <sassy/sassy.h>

#include "sassy_consensus.h"

int leader_process_pkt(struct sassy_device *sdev, struct sassy_payload* pkt);
int start_leader(void);
int stop_leader(void);
