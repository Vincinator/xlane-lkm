#pragma once

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