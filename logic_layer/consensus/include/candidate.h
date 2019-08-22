#pragma once

#include <sassy/sassy.h>

#include "sassy_consensus.h"

int candidate_process_pkt(struct sassy_device *sdev, int remote_lid, unsigned char *pkt);
void reset_ctimeout(struct sassy_device *sdev);
int stop_candidate(struct sassy_device *sdev);
int start_candidate(struct sassy_device *sdev);
void init_ctimeout(struct sassy_device *sdev);
int setup_nomination(struct sassy_device *sdev);


