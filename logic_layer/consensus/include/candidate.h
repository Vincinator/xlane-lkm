#pragma once

#include <sassy/sassy.h>

int candidate_process_pkt(struct sassy_device *sdev, void* pkt);
int start_candidate(void);
