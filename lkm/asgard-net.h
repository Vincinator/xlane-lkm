#pragma once


#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>
#include <asm/checksum.h>
#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>

#include <linux/kernel.h>

#include <linux/compiler.h>

#include "../core/replication.h"
#include "../core/module.h"


struct sk_buff *asgard_reserve_skb(struct net_device *dev,
        u32 dst_ip, unsigned char *dst_mac, struct asgard_payload *payload);

int compare_mac(unsigned char *m1, unsigned char *m2);

void get_cluster_ids(struct asgard_device *sdev, unsigned char *remote_mac, int *lid, int *cid);

void prepare_log_replication_handler(struct work_struct *w);
void prepare_log_replication_multicast_handler(struct asgard_device *sdev);
