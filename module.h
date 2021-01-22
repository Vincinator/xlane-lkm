#pragma once

#define MAX_ASGARD_PROC_NAME 256
#define ASGARD_KERNEL_MODULE
#define MAX_PROCFS_BUF 512
#define ASGARD_NUMBUF 13
#define ASGARD_TARGETS_BUF 512

#define MAX_CPU_NUMBER 55
#define MAX_CLUSTER_MEMBER 512 // arbitrary limit

#define MIN_HB_CYCLES 1000
#define MAX_HB_CYCLES 1000000000

#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_5MS 12000000
#define CYCLES_PER_10MS 24000000
#define CYCLES_PER_100MS 240000000




#include "libasraft.h"
/*
 * Each NIC port gets a unique table.
 * This struct holds references to all tables.
 * ifindex of NIC PORT corresponds to array position of struct asgard_rx_table *tables.
 */
struct asgard_core {

    /* NIC specific Data */
    struct asgard_device **sdevices;

    /* Number of registered asgard devices */
    int num_devices;
};

int is_ip_local(struct net_device *dev,	u32 ip_addr);
void clear_protocol_instances(struct asgard_device *sdev);
const char *asgard_get_protocol_name(enum asgard_protocol_type protocol_type);