/*
 * The runner.c contains functions for starting a node with parameters read from
 * the node.ini residing in the same folder as the executable.
 */

#include "stdlib.h"
#include "stdio.h"
#include "limits.h"
#include "signal.h"
#include "errno.h"

#include "../core/libasraft.h"
#include "tnode.h"
#include "../core/ringbuffer.h"
#include "../core/membership.h"
#include "../core/kvstore.h"
#include "../core/pacemaker.h"

#ifdef ASGARD_DPDK
#include <rte_log.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#endif

#define APP "libasraft"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][TRUNNER]"

#ifdef ASGARD_DPDK
uint32_t DPDK_LIBASRAFT_LOG_LEVEL = RTE_LOG_DEBUG;

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

static struct rte_eth_dev_tx_buffer *tx_buffer;

int RTE_LOGTYPE_LIBASRAFT;

static struct rte_eth_conf port_conf = {
        .rxmode = {
                .split_hdr_size = 0,
        },
        .txmode = {
                .mq_mode = ETH_MQ_TX_NONE,
        },
};


#endif



#define MAX_PKT_BURST 32
#define MEMPOOL_CACHE_SIZE 128



#define MAX_VALUE_SM_VALUE_SPACE 1024
#define MAX_VALUE_SM_ID_SPACE 255



/* Handler for inih config parser (see dependencies)*/
static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
    tnode_t * node_config = (tnode_t*)user;
    char *tuples, *outer_end_token, *inner_end_token, *inner_token;
    char *cur_ip, *cur_id;
    char *long_endptr, *str = NULL;

    char cur_mac_buffer[19];

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("node", "id")) {
        node_config->sdev->pminfo.cluster_id = atoi(value);
        asgard_dbg("node id set to %s\n", value);
    } else if (MATCH("node", "ip")) {
        node_config->domain_name = strdup(value);
        asgard_dbg("domain name set to %s\n", value);
    }  else if (MATCH("test", "one_shot_num_entries")) {
        node_config->oneshot_num_entries = atoi(value);
        asgard_dbg("number of test entries set to %s\n", value);
    } else if (MATCH("node", "port")) {
        node_config->sdev->tx_port = atoi(value);
        node_config->port = atoi(value);
        asgard_dbg("port set to %d\n", node_config->sdev->tx_port);
    }else if (MATCH("node", "hbi")) {
        node_config->sdev->pminfo.hbi = strtol(value, &long_endptr, 10);
        if ((errno == ERANGE && (node_config->sdev->pminfo.hbi == LONG_MAX || node_config->sdev->pminfo.hbi == LONG_MIN))
            || (errno != 0 && node_config->sdev->pminfo.hbi == 0)) {
            perror("strtol");
            exit(EXIT_FAILURE);
        }

        if (long_endptr == str) {
            fprintf(stderr, "No digits were found\n");
            exit(EXIT_FAILURE);
        }
        asgard_dbg("heart beat interval set to %lu nanoseconds\n", node_config->sdev->pminfo.hbi);
    } else if (MATCH("node", "peer_ip_id_tuple")) {
        tuples = strdup(value);
        char *tuple_token = strtok_r(tuples, ";", &outer_end_token);

        while(tuple_token != NULL) {
            inner_token = strtok_r(tuple_token, ",", &inner_end_token);
            cur_ip = strdup(inner_token);
            inner_token = strtok_r(NULL, ",", &inner_end_token);
            cur_id = strdup(inner_token);
            asgard_dbg("parser found ip %s for node id %s\n", cur_ip, cur_id);
            node_config->reg_ips += register_peer_by_ip(node_config->sdev, asgard_ip_convert(cur_ip), atoi(cur_id));
            tuple_token = strtok_r(NULL, ";", &outer_end_token);
        }
    } else if (MATCH("node", "peer_mac_id_tuple")) {
        tuples = strdup(value);
        char *tuple_token = strtok_r(tuples, ";", &outer_end_token);
        while(tuple_token != NULL) {
            inner_token = strtok_r(tuple_token, ",", &inner_end_token);
            sprintf(cur_mac_buffer, "%s\0", inner_token);
            asgard_dbg("parser found mac %s\n", cur_mac_buffer);
            inner_token = strtok_r(NULL, ",", &inner_end_token);
            cur_id = strdup(inner_token);
            node_config->reg_macs += add_mac_to_peer_id(node_config->sdev, cur_mac_buffer, atoi(cur_id));
            tuple_token = strtok_r(NULL, ";", &outer_end_token);
        }
    } else {
        asgard_error("Unmatched token.\n");
        return 0;
    }



    return 1;
}



void testcase_one_shot_big_log(struct consensus_priv *priv, int num_entries)
{
    int i, err;
    struct data_chunk *cur_cmd;
    uint32_t rand_value, rand_id;
    uint32_t *dptr;

    for (i = 0; i < num_entries; i++) {
        rand_value = rand()%((MAX_VALUE_SM_VALUE_SPACE+1)-0) + 0;
        rand_id =  rand()%((MAX_VALUE_SM_ID_SPACE+1)-0) + 0;

        // freed by consensus_clean
        cur_cmd = AMALLOC(sizeof(struct data_chunk), GFP_KERNEL);

        if (!cur_cmd) {
            err = -1;
            goto error;
        }
        dptr = (uint32_t *) cur_cmd->data;
        (*dptr) = rand_id;
        dptr += 1;
        (*dptr) = rand_value;

        err = append_command(priv, cur_cmd, priv->term, i, 0);

        if (err)
            goto error;
    }

    return;
error:
    asgard_error("Evaluation Crashed init_error code=%d\n", err);
}

int one_shot = 0;

void generate_load(tnode_t *tn){
    switch (tn->testmode) {
        case ONE_SHOT:
            /* make sure one shot is only executed once. */
            if(one_shot == 0){
                testcase_one_shot_big_log(tn->sdev->consensus_priv, tn->oneshot_num_entries);
                one_shot = 1;
            }
    }
}


#ifdef ASGARD_DPDK
int main(int argc, char *argv[]){
    tnode_t node;
    int i, ret;
    unsigned int lcore_id;
    uint16_t nb_ports;
    unsigned int nb_mbufs;
    unsigned int stored_dpdk_portid;
    uint64_t dropped;

    printf("DPDK version of asgard\n");
    
    // TODO: we may want to load the mode via the config file
    node.testmode = ONE_SHOT;

    init_node(&node);
    node.sdev->dpdk_portid = 0;
    /* random numbers are used for load generation
     * therefore, predictable random numbers should be OK. */
    srand(time(NULL));

    if (ini_parse("node.ini", handler, &node) < 0) {
        asgard_error("Can't load 'node.ini'\n");
        return 1;
    }
    if(node.reg_ips != node.reg_macs) {
        asgard_error("Number of registered Ips do not match number of registered mac addresses. \n");
        asgard_error("\t Each node must be configured with a valid ip and a valid mac address: \n");
        return 1;
    }

    asgard_dbg("Loaded node.ini\n");

    asgard_dbg("Starting initialization of DPDK env\n");

    ret = rte_eal_init(argc, argv);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

    /* init log */
    RTE_LOGTYPE_LIBASRAFT = rte_log_register(APP);
    ret = rte_log_set_level(RTE_LOGTYPE_LIBASRAFT, DPDK_LIBASRAFT_LOG_LEVEL);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Set log level to %u failed\n", DPDK_LIBASRAFT_LOG_LEVEL);

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports, bye...\n");

    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_LIBASRAFT, "%u port(s) available\n", nb_ports);

    nb_mbufs = RTE_MAX((unsigned int)(nb_ports * (nb_rxd + nb_txd + MAX_PKT_BURST + MEMPOOL_CACHE_SIZE)), 8192U);
    node.sdev->pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
                                                    MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                                    rte_socket_id());
    if (node.sdev->pktmbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

    struct rte_eth_rxconf rxq_conf;
    struct rte_eth_txconf txq_conf;
    struct rte_eth_conf local_port_conf = port_conf;
    struct rte_eth_dev_info dev_info;

    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_LIBASRAFT, "Initializing port %u...\n", node.sdev->dpdk_portid);
    fflush(stdout);

    /* init port */
    rte_eth_dev_info_get(node.sdev->dpdk_portid, &dev_info);
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        local_port_conf.txmode.offloads |=
                DEV_TX_OFFLOAD_MBUF_FAST_FREE;

    ret = rte_eth_dev_configure(node.sdev->dpdk_portid, 1, 1, &local_port_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
                 ret, node.sdev->dpdk_portid);

    ret = rte_eth_dev_adjust_nb_rx_tx_desc(node.sdev->dpdk_portid, &nb_rxd,
                                           &nb_txd);
    if (ret < 0)
        rte_exit(EXIT_FAILURE,
                 "Cannot adjust number of descriptors: err=%d, port=%u\n",
                 ret, node.sdev->dpdk_portid);

    /* init one RX queue */
    fflush(stdout);
    rxq_conf = dev_info.default_rxconf;

    rxq_conf.offloads = local_port_conf.rxmode.offloads;
    ret = rte_eth_rx_queue_setup(node.sdev->dpdk_portid, 0, nb_rxd,
                                 rte_eth_dev_socket_id(node.sdev->dpdk_portid),
                                 &rxq_conf,
                                 node.sdev->pktmbuf_pool);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
                 ret, node.sdev->dpdk_portid);

    /* init one TX queue on each port */
    fflush(stdout);
    txq_conf = dev_info.default_txconf;
    txq_conf.offloads = local_port_conf.txmode.offloads;
    ret = rte_eth_tx_queue_setup(node.sdev->dpdk_portid, 0, nb_txd,
                                 rte_eth_dev_socket_id(node.sdev->dpdk_portid),
                                 &txq_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
                 ret, node.sdev->dpdk_portid);

    configure_tx_buffer(node.sdev->dpdk_portid, 32);

    /* Start device */
    ret = rte_eth_dev_start(node.sdev->dpdk_portid);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
                 ret, node.sdev->dpdk_portid);


    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_LIBASRAFT, "Initilize port %u done.\n", node.sdev->dpdk_portid);

    lcore_id = rte_get_next_lcore(0, true, false);

    signal(SIGINT, &trap);

    start_node(&node);

    rte_eal_remote_launch(dpdk_server_listener, &node, lcore_id);
    asgard_dbg("\n!!! Run server listener on lcore id: %d\n", lcore_id);

    lcore_id = rte_get_next_lcore(lcore_id, true, false);
    rte_eal_remote_launch(pacemaker, node.sdev, lcore_id);
    asgard_dbg("\n!!! Run pacemaker on lcore id: %d\n", lcore_id);

    asgard_dbg("Node is running. Press ctrl+c to exit ...\n");
    // we need to store the id since we destroy the asgard device before we can access the dpdk port id for dpdk cleanup
    stored_dpdk_portid = node.sdev->dpdk_portid;
    while(node.is_running){
        sleep(1);
        /* If this node is the leader, then generate the test load against this leader node.
         * This way, we do not measure any overhead between leader and client.
         */

        if(node.sdev->is_leader)
            generate_load(&node);


        if(user_requested_stop != 0) {
            asgard_dbg("Stopping node ..\n");
            stop_node(&node);
            asgard_dbg("Node stopped %d\n", i);
            break;
        }
    }

    rte_eth_dev_stop(stored_dpdk_portid);
    rte_eth_dev_close(stored_dpdk_portid);
    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_LIBASRAFT, "DPDK exited..\n");

    signal(SIGINT, SIG_DFL);

    asgard_dbg("libasraft exited..\n");
    return 0;
}

#else
int main(int argc, char *argv[]){
    tnode_t node;
    printf("Userspace plain version of asgard\n");


    signal(SIGINT, &trap);

    // TODO: we may want to load the mode via the config file
    node.testmode = ONE_SHOT;

    init_node(&node);

    /* random numbers are used for load generation
     * therefore, predictable random numbers should be OK. */
    srand(time(NULL));

    if (ini_parse("node.ini", handler, &node) < 0) {
        printf("Can't load 'node.ini'\n");
        return 1;
    }

    asgard_dbg("Loaded node.ini\n");

    start_node(&node);

    user_requested_stop = 0;
    asgard_dbg("Node is running. Press ctrl+c to exit ...\n");
    while(node.is_running ){

        if(node.sdev->is_leader)
           generate_load(&node);


        if(user_requested_stop){
            stop_node(&node);
            asgard_dbg("Node stopped\n");
        }
    }
    signal(SIGINT, SIG_DFL);

    asgard_dbg("exiting..\n");
    return 0;
}

#endif