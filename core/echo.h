#pragma once

#include "types.h"
#include "logger.h"
#include <stdbool.h>


struct echo_priv {
    struct asgard_logger echo_logger;
    struct asgard_device *sdev;
    struct proto_instance *ins;
    struct proc_dir_entry *echo_pupu_entry;
    bool echo_pmpu_entry;
    struct proc_dir_entry *echo_pmpm_entry;
    struct proc_dir_entry *echo_pupm_entry;
    struct proc_dir_entry *echo_ppwt_entry;

    int pong_waiting_interval;
    uint64_t last_echo_ts;
    bool fire_ping;
};