#ifndef LIBASRAFT_LOGGER_H
#define LIBASRAFT_LOGGER_H

#ifndef ASGARD_KERNEL_MODULE
#include <stdio.h>
#endif

#ifdef ASGARD_KERNEL_MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#endif

#include "types.h"

#define ASGARD_TIMESTAMP asgts()


#define VERBOSE_DEBUG 1
#define ASGARD_DEBUG 1
#define MAX_LOGGER_NAME 32
#define LOGGER_EVENT_LIMIT 100000000
#define MAX_NIC_DEVICES 8


#ifndef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][UNKNOWN MODULE]"
#endif
#define LOG_LE_PREFIX "[LEADER ELECTION][LOG]"


#ifdef ASGARD_KERNEL_MODULE
#if ASGARD_DEBUG

#include <linux/kernel.h>

#define asgard_dbg(format, arg...)                        \
    ({                                                        \
        if (1)                                                \
            printk(KERN_INFO LOG_PREFIX format, ##arg);    \
    })
#else
#define asgard_dbg(format, arg...)
#endif

#define asgard_error(format, arg...)                        \
({                                                        \
    if (1)                                                \
        printk(KERN_ERR LOG_PREFIX format, ##arg);    \
})

#define asgard_log_le(format, arg...)                        \
({                                                        \
    if (1)                                                \
        printk(KERN_INFO LOG_LE_PREFIX format, ##arg);    \
})


#else
#if ASGARD_DEBUG
#define asgard_dbg(format, arg...)                        \
    ({                                                        \
        if (1)                                                \
            printf(LOG_PREFIX format, ##arg);    \
    })
#else
#define asgard_dbg(format, arg...)
#endif

#define asgard_error(format, arg...)                        \
({                                                        \
    if (1)                                                \
        printf(LOG_PREFIX format, ##arg);    \
})

#define asgard_log_le(format, arg...)                        \
({                                                        \
    if (1)                                                \
        printf(LOG_LE_PREFIX format, ##arg);    \
})


#endif
void asg_print_mac(unsigned char *sa_data);
void asg_print_ip(unsigned int ip);

enum le_event_type {

    FOLLOWER_TIMEOUT = 0,
    CANDIDATE_TIMEOUT = 1,

    FOLLOWER_ACCEPT_NEW_LEADER = 2,
    CANDIDATE_ACCEPT_NEW_LEADER = 3,
    LEADER_ACCEPT_NEW_LEADER = 4,

    FOLLOWER_BECOME_CANDIDATE = 5,
    CANDIDATE_BECOME_LEADER = 6,

    START_CONSENSUS = 7,
    VOTE_FOR_CANDIDATE = 8,
    CANDIDATE_ACCEPT_VOTE = 9,

    REPLY_APPEND_SUCCESS = 10,
    REPLY_APPEND_FAIL = 11,

    CONSENSUS_REQUEST = 12,
    GOT_CONSENSUS_ON_VALUE = 13,
    START_LOG_REP = 14,

    INGRESS_PACKET = 15,

};

struct logger_event {
    uint64_t timestamp_tcs;
    int type;
    int node_id;

    // used to store the delta to the previous event.
    uint64_t delta;
};

enum logger_state {
    LOGGER_RUNNING,
    LOGGER_READY,        /* Initialized but not active*/
    LOGGER_UNINIT,
    LOGGER_LOG_FULL,
};



struct asgard_logger {
    uint16_t instance_id;
    enum logger_state state;
    int ifindex;
    int accept_user_ts;

    char *name;

    int current_entries;

    /* Size is defined by LOGGER_EVENT_LIMIT */
    struct logger_event *events;

    int applied;
    uint64_t first_ts;
    uint64_t last_ts;

};

struct asgard_ingress_logger {
    uint16_t instance_id;
    enum logger_state state;

    struct asgard_logger *per_node_logger;

    int num_of_nodes;
};

uint64_t asgts(void);

int init_logger(struct asgard_logger *slog, uint16_t instance_id, int ifindex, char name[MAX_LOGGER_NAME],
                int accept_user_ts);

void clear_logger(struct asgard_logger *slog);
int write_log(struct asgard_logger *slog, int type, uint64_t tcs);
void dump_ingress_log(struct asgard_logger *slog, int num_nodes, uint64_t hb_ns);
int write_ingress_log(struct asgard_ingress_logger *slog, int type, uint64_t tcs, int node_id);

void logger_state_transition_to(struct asgard_logger *slog, enum logger_state state);

void clear_ingress_logger(struct asgard_ingress_logger *ailog);
int init_ingress_logger(struct asgard_ingress_logger *ailog, int instance_id);
void asgard_hex_to_ip(char *retval, uint32_t dst_ip);
#endif //LIBASRAFT_LOGGER_H
