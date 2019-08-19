#pragma once

#include <sassy/sassy.h>

#include "sassy_consensus.h"

int candidate_process_pkt(struct sassy_device *sdev, void* pkt);
int start_candidate(void);
int stop_candidate(void);
int broadcast_nomination(void);


