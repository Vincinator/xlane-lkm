#pragma once


#ifdef ASGARD_KERNEL_MODULE

    #include <linux/proc_fs.h>
    #include <linux/seq_file.h>
    #include <linux/kthread.h>
    #include <linux/sched.h>

    #include <linux/kernel.h>

    #include <linux/netdevice.h>
    #include <linux/skbuff.h>
    #include <linux/udp.h>
    #include <linux/etherdevice.h>
    #include <linux/ip.h>
    #include <net/ip.h>
    #include <linux/slab.h>
    #include <linux/inetdevice.h>

    #include <linux/hrtimer.h>
    #include <linux/ktime.h>
    #include <linux/time.h>

    #include "module.h"
    #include "../lkm/asgard-net.h"

#else

    #include <string.h>
    #include <stdio.h>
    #include <netinet/in.h>
    #include <errno.h>
    #include <stdlib.h>
    #include <signal.h>
    #include "../userspace/tnode.h"

#endif

#include "types.h"
#include "consensus.h"
#include "libasraft.h"
#include "payload.h"
#include "logger.h"
#include "config.h"
#include "membership.h"
#include "pktqueue.h"


#define ETH_HLEN	14
#define UDP_HLEN    8
#define IP_HLEN     60


int asgard_pm_stop(struct pminfo *spminfo);

#ifdef ASGARD_DPDK
int pacemaker(void *data);
#else
void *pacemaker(void *data);

#endif
void pm_state_transition_to(struct pminfo *spminfo, const enum pmstate state);
void update_alive_msg(struct asgard_device *sdev, struct asgard_payload *pkt_payload);
void init_pacemaker(struct pminfo *spminfo);
void update_leader(struct asgard_device *sdev, struct pminfo *spminfo);
int asgard_pm_reset(struct pminfo *spminfo);

#ifdef ASGARD_KERNEL_MODULE
int asgard_pm_start_loop(void *data);
#endif

