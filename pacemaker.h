#pragma once


#include "types.h"
#include "consensus.h"
#include "libasraft.h"
#include "payload.h"
#include "logger.h"
#include "config.h"

#define ETH_HLEN	14
#define UDP_HLEN    8
#define IP_HLEN     60

int asgard_pm_stop(struct pminfo *spminfo);

#if ASGARD_DPDK
int pacemaker(void *data);
#else
void *pacemaker(void *data);

#endif
void pm_state_transition_to(struct pminfo *spminfo, const enum pmstate state);
void update_alive_msg(struct asgard_device *sdev, struct asgard_payload *pkt_payload);
void init_pacemaker(struct pminfo *spminfo);
void update_leader(struct asgard_device *sdev, struct pminfo *spminfo);