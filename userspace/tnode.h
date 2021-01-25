#pragma once




#ifdef ASGARD_KERNEL_MODULE




#else
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "libasraft.h"

#include "ini.h"

#endif

#include "../pacemaker.h"
#include "../pkthandler.h"
#include "../libasraft.h"
#include "../logger.h"



#define MAX_PEERS 100

typedef struct {
    int node_id;
    unsigned long ip_addr;
    char *domain_name;
} peer_t;

typedef enum {
    ONE_SHOT = 0,
} testmode_t;

typedef struct {

    char *domain_name;
    unsigned int port;

    struct asgard_device *sdev;

    int num_peers;

    testmode_t testmode;

    int is_running;

    peer_t *peers[MAX_PEERS];

#ifdef ASGARD_KERNEL_MODULE
#else
    pthread_t pm_thread;
    pthread_t pl_thread;
#endif
    int oneshot_num_entries;

    int reg_macs;
    int reg_ips;
} tnode_t;

extern int user_requested_stop;

#ifndef ASGARD_KERNEL_MODULE
void trap(int signal);
#endif

tnode_t *init_node(tnode_t *tn);
int start_node(tnode_t *tn);
int stop_node(tnode_t *tn);
int dpdk_server_listener(void *data);
