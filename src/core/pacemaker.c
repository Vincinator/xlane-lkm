/*
 * Logic
 */
#include "pacemaker.h"

#include "pingpong.h"


#ifdef ASGARD_DPDK
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#endif

#ifdef ASGARD_KERNEL_MODULE
static struct task_struct *heartbeat_task;
#endif



#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][PACEMAKER]"

#define IP_DEFTTL 64 /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN 0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)
#define IP_ADDR_FMT_SIZE 15


#ifdef ASGARD_DPDK
struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

/*
 * Tx buffer error callback
 */
static void flush_tx_error_callback(struct rte_mbuf **unsent, uint16_t count,
                        void *userdata) {
    int i;
    uint16_t port_id = (uintptr_t)userdata;
    /* free the mbufs which failed from transmit */
    for (i = 0; i < count; i++)
        rte_pktmbuf_free(unsent[i]);
}


void configure_tx_buffer(uint16_t port_id, uint16_t size)
{
    int ret;

    tx_buffer[port_id] = rte_zmalloc_socket("tx_buffer", RTE_ETH_TX_BUFFER_SIZE(size), 0,
                                            rte_eth_dev_socket_id(port_id));

    if (tx_buffer[port_id] == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                 port_id);

    rte_eth_tx_buffer_init(tx_buffer[port_id], size);
    ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[port_id],
                                             flush_tx_error_callback, (void *)(intptr_t)port_id);
    if (ret < 0)
        rte_exit(EXIT_FAILURE,
                 "Cannot set error callback for tx buffer on port %u\n",
                 port_id);
}



#endif

const char *pm_state_string(enum pmstate state) {
    switch (state) {
        case ASGARD_PM_UNINIT:
            return "ASGARD_PM_UNINIT";
        case ASGARD_PM_READY:
            return "ASGARD_PM_READY";
        case ASGARD_PM_EMITTING:
            return "ASGARD_PM_EMITTING";
        case ASGARD_PM_FAILED:
            return "ASGARD_PM_FAILED";
        default:
            return "UNKNOWN STATE";
    }
}

void pm_state_transition_to(struct pminfo *spminfo, const enum pmstate state) {
    asgard_dbg("State Transition from %s to %s\n",
               pm_state_string(spminfo->state), pm_state_string(state));
    spminfo->state = state;
}

void set_le_opcode_ad(unsigned char *pkt, enum le_opcode opco, int32_t cluster_id, int32_t self_ip,
                      asg_mac_ptr_t self_mac) {
    uint16_t *opcode;
    uint32_t *param1;
    int32_t *param2;
    char *param3;

    opcode = GET_CON_PROTO_OPCODE_PTR(pkt);
    *opcode = (uint16_t) opco;

    param1 = GET_CON_PROTO_PARAM1_PTR(pkt);
    *param1 = (uint32_t) cluster_id;

    param2 = GET_CON_PROTO_PARAM2_PTR(pkt);
    *param2 = (int32_t) self_ip;

    param3 = GET_CON_PROTO_PARAM3_MAC_PTR(pkt);

    memcpy(param3, self_mac, 6);

}

int setup_cluster_join_advertisement(struct asgard_payload *spay, int advertise_id, uint32_t ip, asg_mac_ptr_t mac) {
    unsigned char *pkt_payload_sub;

    pkt_payload_sub = asgard_reserve_proto(1, spay, ASGARD_PROTO_CON_PAYLOAD_SZ);

    if (!pkt_payload_sub)
        return -1;

    set_le_opcode_ad((unsigned char *) pkt_payload_sub, ADVERTISE, advertise_id, ip, mac);

    return 0;
}

int setup_alive_msg(struct consensus_priv *cur_priv, struct asgard_payload *spay, int instance_id) {
    unsigned char *pkt_payload_sub;

    pkt_payload_sub = asgard_reserve_proto(instance_id, spay, ASGARD_PROTO_CON_PAYLOAD_SZ);

    if (!pkt_payload_sub)
        return -1;

    set_le_opcode((unsigned char *) pkt_payload_sub, ALIVE, cur_priv->term, cur_priv->sm_log.commit_idx,
                  cur_priv->sm_log.stable_idx, cur_priv->sdev->hb_interval);

    return 0;
}

/* Logic to include protocol dependent messages for the heartbeat message */
void pre_hb_setup(struct asgard_device *sdev, struct asgard_payload *pkt_payload, int target_lid) {
    int j;

    // Not a member of a cluster yet - thus, append advertising messages
    if (sdev->warmup_state == WARMING_UP) {
        if (sdev->self_mac) {
            setup_cluster_join_advertisement(pkt_payload, sdev->pminfo.cluster_id, sdev->self_ip, sdev->self_mac);
        }
        return; // All currently defined protocols require the cluster to be warmed up
    }

    for (j = 0; j < sdev->num_of_proto_instances; j++) {

        if (sdev->protos[j]->proto_type == ASGARD_PROTO_CONSENSUS && sdev->is_leader != 0) {

            setup_alive_msg((struct consensus_priv *) sdev->protos[j]->proto_data,
                            pkt_payload, sdev->protos[j]->instance_id);

        } else if(sdev->protos[j]->proto_type == ASGARD_PROTO_PP) {

            setup_ping_msg((struct pingpong_priv *) sdev->protos[j]->proto_data,
                    pkt_payload, sdev->protos[j]->instance_id, target_lid);

        }
    }
}

int asgard_pm_stop(struct pminfo *spminfo) {
    int i;
    struct asgard_device *sdev =
            container_of(spminfo, struct asgard_device, pminfo);

    if (!spminfo) {
        asgard_error("spminfo is NULL.\n");
        return -EINVAL;
    }

    pm_state_transition_to(spminfo, ASGARD_PM_READY);

    // Clear Cluster Membership
    for(i = 0; i >spminfo->num_of_targets; i++)
        update_cluster_member(sdev->ci, i, 0);

    return 0;
}


/* ------------------------------------*/

void invalidate_proto_data(struct asgard_payload *spay) {
    // free previous piggybacked protocols
    spay->protocols_included = 0;
}


uint32_t get_lowest_alive_id(struct asgard_device *sdev, struct pminfo *spminfo) {
    int i;
    uint32_t cur_low = sdev->pminfo.cluster_id;

    for (i = 0; i < spminfo->num_of_targets; i++) {
        if (spminfo->pm_targets[i].alive) {
            if (cur_low > spminfo->pm_targets[i].cluster_id) {
                cur_low = (uint32_t) spminfo->pm_targets[i].cluster_id;
            }
        }
    }
    return cur_low;
}

void update_leader(struct asgard_device *sdev, struct pminfo *spminfo) {
    int leader_lid = sdev->cur_leader_lid;
    uint32_t self_id = sdev->pminfo.cluster_id;
    uint32_t lowest_follower_id = get_lowest_alive_id(sdev, spminfo);
    struct consensus_priv *priv = sdev->consensus_priv;

    if (!priv) {
        asgard_dbg("consensus priv is NULL \n");
        return;
    }

    if (sdev->warmup_state == WARMING_UP)
        return;

    if (leader_lid == -1 || spminfo->pm_targets[leader_lid].alive == 0) {
        if (lowest_follower_id == self_id) {
            /* TODO: parameterize candidate_counter check */
            if (priv->nstate == CANDIDATE &&
                priv->candidate_counter < 50) {
                priv->candidate_counter++;
                return;
            }

            node_transition(priv->ins, CANDIDATE);
            write_log(&priv->ins->logger, FOLLOWER_BECOME_CANDIDATE,
                      ASGARD_TIMESTAMP);
        }
    }
}

/*
 * Marks target with local id <i> as dead
 * if no update came in since last check
 */
void update_aliveness_states(struct asgard_device *sdev, struct pminfo *spminfo,
                             int i) {

    /* cur_waiting_interval specifies how many heartbeat intervals to wait until considering the target as dead. */
    if (spminfo->pm_targets[i].cur_waiting_interval != 0) {
        spminfo->pm_targets[i].cur_waiting_interval =
                spminfo->pm_targets[i].cur_waiting_interval - 1;
        return;
    }

    /* check if last hb timestamp and current hearbeat timestamp are the same. */
    if (spminfo->pm_targets[i].lhb_ts == spminfo->pm_targets[i].chb_ts) {
        spminfo->pm_targets[i].alive = 0;
        update_cluster_member(sdev->ci, i, 0);
        spminfo->pm_targets[i].cur_waiting_interval =
                spminfo->pm_targets[i].resp_factor;
        return;
    }

    // may be redundant - since we already update aliveness on reception of pkt
    //spminfo->pm_targets[i].alive = 1; // Todo: remove this?
    //update_cluster_member(sdev->ci, i, 1);

    spminfo->pm_targets[i].lhb_ts = spminfo->pm_targets[i].chb_ts;
    spminfo->pm_targets[i].cur_waiting_interval =
            spminfo->pm_targets[i].resp_factor;
}

/* --------- Check Functions --------- */

static inline int pacemaker_is_alive(struct pminfo *spminfo) {
    return spminfo->state == ASGARD_PM_EMITTING;
}

static inline int scheduled_tx(uint64_t prev_time, uint64_t cur_time,
                               uint64_t interval) {
    return (cur_time - prev_time) >= interval;
}

static inline int check_async_window(uint64_t prev_time, uint64_t cur_time,
                                     uint64_t interval, uint64_t sync_window) {
    return (cur_time - prev_time) <= (interval - sync_window);
}


static inline int out_of_schedule_tx(struct asgard_device *sdev) {
    int i;

    if (sdev->hold_fire)
        return 0;

    for (i = 0; i < sdev->pminfo.num_of_targets; i++) {
        if (sdev->pminfo.pm_targets[i].fire)
            return 1;
    }

    return 0;
}

static inline int check_async_door(struct asgard_device *sdev) {
    int i;

    if (sdev->multicast.enable)
        return sdev->multicast.aapriv->doorbell;

    for (i = 0; i < sdev->pminfo.num_of_targets; i++) {
        if (sdev->pminfo.pm_targets[i].aapriv->doorbell)
            return 1;
    }

    return 0;
}


static inline int out_of_schedule_multi_tx(struct asgard_device *sdev) {
    if (sdev->hold_fire)
        return 0;

    if(sdev->multicast.enable == 0)
        return 0;

    return sdev->pminfo.multicast_pkt_data_oos_fire;
}

/* --------- Emitter Functions --------- */


#ifdef ASGARD_DPDK
static inline uint16_t
ip_sum(const unaligned_uint16_t *hdr, int hdr_len)
{
    uint32_t sum = 0;

    while (hdr_len > 1)
    {
        sum += *hdr++;
        if (sum & 0x80000000)
            sum = (sum & 0xFFFF) + (sum >> 16);
        hdr_len -= 2;
    }

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

static unsigned int get_packet_size_for_alloc(void){
    unsigned int ip_len, udp_len, asgard_len, total_len;

    udp_len =  sizeof(struct rte_udp_hdr);
    ip_len = udp_len + sizeof(struct rte_ipv4_hdr);
    total_len = ip_len + sizeof(struct rte_ether_hdr);

    return total_len;
}

/* construct ping packet */
static struct rte_mbuf *contruct_dpdk_asg_packet(struct rte_mempool *pktmbuf_pool,
                                                 struct node_addr recvaddr, uint32_t send_ip, struct asgard_payload *asgp,
                                                 asg_mac_ptr_t recv_mac, asg_mac_ptr_t sender_mac)
{
    struct rte_mbuf *pkt;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    char* payload_ptr;
    unsigned pkt_size = get_packet_size_for_alloc();

    if(!recv_mac){
        asgard_error("Recv mac is NULL\n");
        return NULL;
    }

    pkt = rte_pktmbuf_alloc(pktmbuf_pool);
    if (!pkt) {
        asgard_error("fail to alloc mbuf for packet\n");
        return NULL;
    }
    pkt->data_len = pkt_size;
    pkt->next = NULL;

    /* Initialize Ethernet header. */
    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

    rte_ether_addr_copy(recv_mac, &eth_hdr->d_addr);

    rte_ether_addr_copy(sender_mac, &eth_hdr->s_addr);

    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    /* Initialize IP header. */
    ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);

    memset(ip_hdr, 0, sizeof(*ip_hdr));

    ip_hdr->version_ihl = IP_VHL_DEF;

    ip_hdr->type_of_service = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live = IP_DEFTTL;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->packet_id = 0;


    /* Just use the IP address from socket address.
     *  - in_addr_t is a typedef to __uint32_t.
     * */
    ip_hdr->src_addr = rte_cpu_to_be_32(send_ip);
    ip_hdr->dst_addr = rte_cpu_to_be_32((uint32_t) recvaddr.dst_ip);

    ip_hdr->total_length = rte_cpu_to_be_16(pkt_size -
                                            sizeof(*eth_hdr));
    ip_hdr->hdr_checksum = ip_sum((unaligned_uint16_t *)ip_hdr,
                                  sizeof(*ip_hdr));

    /* Initialize UDP header. */
    udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);

    /* Just use the port of the socket data struct.
     * - in_port_t is a typedef to __uint16_t
     * */
    udp_hdr->src_port = rte_cpu_to_be_16((uint16_t)recvaddr.port + 1);
    udp_hdr->dst_port = rte_cpu_to_be_16((uint16_t)recvaddr.port);

    udp_hdr->dgram_cksum = 0; /* No UDP checksum. */

    pkt->nb_segs = 1;
    pkt->pkt_len = pkt_size;
    pkt->l2_len = sizeof(struct rte_ether_hdr);
    pkt->l3_len = sizeof(struct rte_ipv4_hdr);
    pkt->l4_len = sizeof(struct rte_udp_hdr);

    // asgard_dbg("Pre payload allocation pkt len: %d\n", pkt->pkt_len);
    /* Copy the Payload to the allocated dpdk packet
     * 1. Get Pointer after pkt with (currently) only headers
     * 2. memcpy asgard payload to pkt
     * */
    payload_ptr = rte_pktmbuf_append(pkt, sizeof(struct asgard_payload));
    // asgard_dbg("Post payload allocation pkt len: %d\n", pkt->pkt_len);

    if (payload_ptr != NULL) {
        rte_memcpy(payload_ptr, asgp, sizeof(struct asgard_payload));
    } else {
        asgard_error("Could not append %ld bytes to packet. \n", sizeof(struct asgard_payload));
        return NULL;
    }

    udp_hdr->dgram_len = rte_cpu_to_be_16(pkt->pkt_len - (sizeof(*eth_hdr) + sizeof(*ip_hdr)));
    // asgard_dbg("udp_hdr->dgram_len: %d\n", rte_be_to_cpu_16(udp_hdr->dgram_len));



    return pkt;
}

unsigned int emit_dpdk_asg_packet(uint16_t portid, uint32_t self_ip, struct rte_mempool *pktmbuf_pool,
                                  struct node_addr recvaddr, struct asgard_payload *asg_payload,
                                  asg_mac_ptr_t recv_mac, asg_mac_ptr_t sender_mac) {
    struct rte_mbuf *dpdk_pkt= NULL;
    unsigned int nb_tx;

    dpdk_pkt = contruct_dpdk_asg_packet(pktmbuf_pool, recvaddr, self_ip, asg_payload, recv_mac, sender_mac);
    if (!dpdk_pkt) {
        asgard_dbg("DPDK packet not created! dropping packet transmission\n");
        return -1;
    }

#ifdef DPDK_BURST_SINGLE
    nb_tx = rte_eth_tx_burst(portid, 0, &dpdk_pkt, 1);
#else
    /* queue id = 0, number of packets = 1 (due to "fire when ready" approach) */
    nb_tx = rte_eth_tx_buffer(portid, 0, tx_buffer[portid], dpdk_pkt);

    if(nb_tx == 0)
        nb_tx = rte_eth_tx_buffer_flush(portid, 0, tx_buffer[portid]);
#endif

    DumpHex(asg_payload->proto_data, ASGARD_PROTO_PP_PAYLOAD_SZ);
    return nb_tx;
}
#elif ASGARD_KERNEL_MODULE

static inline void asgard_update_skb_udp_port(struct sk_buff *skb, int udp_port)
{
	struct udphdr *uh = udp_hdr(skb);

	uh->dest = htons((u16)udp_port);
}

static inline void asgard_update_skb_payload(struct sk_buff *skb, void *payload)
{
	unsigned char *tail_ptr;
	unsigned char *data_ptr;

    if(!skb){
        asgard_error("SKB is NULL\n");
        return;
    }

    if(!payload){
        asgard_error("payload void ptr is NULL\n");
        return;    
    }

	tail_ptr = skb_tail_pointer(skb);
	data_ptr = (tail_ptr - sizeof(struct asgard_payload));

	memcpy(data_ptr, payload, sizeof(struct asgard_payload));
    memset(payload, 0, sizeof(struct asgard_payload));

}



int asg_xmit_skb(struct net_device *ndev, struct netdev_queue *txq,  struct sk_buff *skb) {
    int ret = 0;

    skb_get(skb);

    ret = netdev_start_xmit(skb, ndev, txq, 0);

    switch (ret) {
        case NETDEV_TX_OK:
            break;
        case NET_XMIT_DROP:
            asgard_error("NETDEV TX DROP\n");
            break;
        case NET_XMIT_CN:
            asgard_error("NETDEV XMIT CN\n");
            break;
        default:
            asgard_error("NETDEV UNKNOWN \n");
            /* fall through */
        case NETDEV_TX_BUSY:
            asgard_error("NETDEV TX BUSY\n");
            break;
    }
    return ret;
}


static inline void asgard_send_skb(struct net_device *ndev, struct pminfo *spminfo, struct sk_buff *skb )
{
    struct netdev_queue *txq;
    unsigned long flags;
    int tx_index = smp_processor_id();

    if (unlikely(!netif_running(ndev) ||
                 !netif_carrier_ok(ndev))) {
        asgard_error("Network device offline!\n exiting pacemaker\n");
        spminfo->errors++;
        return;
    }
    spminfo->errors = 0;

    local_irq_save(flags);
    local_bh_disable();

    /* The queue mapping is the same for each target <i>
     * Since we pinned the pacemaker to a single cpu,
     * we can use the smp_processor_id() directly.
     */
    txq = &ndev->_tx[tx_index];

    HARD_TX_LOCK(ndev, txq, tx_index);

    if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
        //asgard_error("Device Busy unlocking.\n");
        goto unlock;
    }

    asg_xmit_skb(ndev, txq, skb);

unlock:
    HARD_TX_UNLOCK(ndev, txq);

    local_bh_enable();
    local_irq_restore(flags);

}


#else
int emit_packet(struct node_addr recv_nodeaddr, struct asgard_payload *asg_payload) {

    int sockfd;
    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return -1;
    }
    struct sockaddr_in recv_sock_addr;

    recv_sock_addr.sin_port = recv_nodeaddr.port;
    recv_sock_addr.sin_family = AF_INET;
    recv_sock_addr.sin_addr.s_addr = recv_nodeaddr.dst_ip;

    sendto(sockfd, asg_payload, sizeof(struct asgard_payload), 0, (const struct sockaddr *) &recv_sock_addr, sizeof(recv_sock_addr));
    close(sockfd);
    return 0;
}
#endif

int emit_async_unicast_pkts(struct asgard_device *sdev, struct pminfo *spminfo)
{
    int i;
    struct asgard_async_pkt *cur_apkt;

    for (i = 0; i < spminfo->num_of_targets; i++) {
        if (spminfo->pm_targets[i].aapriv->doorbell > 0) {
            cur_apkt = dequeue_async_pkt(spminfo->pm_targets[i].aapriv);

            // consider packet already handled
            spminfo->pm_targets[i].aapriv->doorbell--;

            if (!cur_apkt) {
                asgard_error("pkt or skb is NULL! \n");
                continue;
            }
#if 0
            // DEBUG: print emitted pkts
            int32_t num_entries = 0, *prev_log_idx;
            num_entries = GET_CON_AE_NUM_ENTRIES_VAL( cur_apkt->pkt_data.payload->proto_data);
            prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(cur_apkt->pkt_data.payload->proto_data);
            asgard_dbg("Node: %d - Emitting %d entries, start_idx=%d\n", i, num_entries, *prev_log_idx);
#endif

#ifdef ASGARD_DPDK
            sdev->tx_counter += emit_dpdk_asg_packet(sdev->dpdk_portid, sdev->self_ip,
                                                     sdev->pktmbuf_pool,
                                                     cur_apkt->pkt_data.naddr, cur_apkt->pkt_data.payload,
                                                     spminfo->pm_targets[i].mac_addr, sdev->self_mac);
#elif ASGARD_KERNEL_MODULE

            skb_set_queue_mapping(cur_apkt->skb, smp_processor_id());

            asgard_update_skb_udp_port(cur_apkt->skb, sdev->tx_port);
            asgard_update_skb_payload(cur_apkt->skb, cur_apkt->pkt_data.payload);
            asgard_send_skb(sdev->ndev, spminfo, cur_apkt->skb);
            sdev->tx_counter++;

#else
            emit_packet(cur_apkt->pkt_data.naddr, cur_apkt->pkt_data.payload);

#endif
            spminfo->pm_targets[i].pkt_tx_counter++;
        }
    }

    return 0;
}

int emit_async_multicast_pkt(struct asgard_device *sdev, struct pminfo *spminfo)
{
    struct asgard_async_pkt *cur_apkt;

    if (sdev->multicast.aapriv->doorbell > 0) {
        cur_apkt = dequeue_async_pkt(sdev->multicast.aapriv);

        // consider packet already handled
        sdev->multicast.aapriv->doorbell--;

        if (!cur_apkt) {
            asgard_error("pkt is NULL! \n");
            return -1;
        }
#ifdef ASGARD_DPDK
        // NOT IMPLEMENTED! Missing pm target ethernet address
        // sdev->tx_counter += emit_dpdk_asg_packet(sdev->dpdk_portid, sdev->self_ip,
        //                                sdev->pktmbuf_pool, cur_apkt->pkt_data.sockaddr,
        //                                cur_apkt->pkt_data.payload,
        //                                NULL, sdev->rte_self_mac);
#elif ASGARD_KERNEL_MODULE

        skb_set_queue_mapping(cur_apkt->skb, smp_processor_id());

        asgard_update_skb_udp_port(cur_apkt->skb, sdev->tx_port);
        asgard_update_skb_payload(cur_apkt->skb, cur_apkt->pkt_data.payload);
        asgard_send_skb(sdev->ndev, spminfo, cur_apkt->skb);
        sdev->tx_counter++;


#else
        emit_packet(cur_apkt->pkt_data.naddr, cur_apkt->pkt_data.payload);
#endif

    }

    return 0;
}


static inline int emit_pkts_non_scheduled_multi(struct asgard_device *sdev,
                                                 struct pminfo *spminfo)
{
    struct asgard_packet_data *pkt_payload = NULL;

    asg_mutex_lock(&spminfo->multicast_pkt_data_oos.mlock);

    pkt_payload = &spminfo->multicast_pkt_data_oos;

#ifdef ASGARD_DPDK
    // NOT IMPLEMENTED! Missing pm target ethernet address
    //sdev->tx_counter += emit_dpdk_asg_packet(sdev->dpdk_portid, sdev->self_ip,
    //                                sdev->pktmbuf_pool, pkt_payload->sockaddr,
    //                               pkt_payload->payload,
    //                               spminfo->pm_targets[i].mac_addr, sdev->rte_self_mac);
#elif ASGARD_KERNEL_MODULE

    skb_set_queue_mapping(pkt_payload->skb, smp_processor_id());

    asgard_update_skb_udp_port(pkt_payload->skb, pkt_payload->naddr.port);
    asgard_update_skb_payload(pkt_payload->skb, pkt_payload->payload);
    asgard_send_skb(sdev->ndev, spminfo, pkt_payload->skb);
    sdev->tx_counter++;

#else
    emit_packet(pkt_payload->naddr, pkt_payload->payload);
#endif

    memset(pkt_payload->payload, 0, sizeof(struct asgard_payload));

    spminfo->multicast_pkt_data_oos_fire = 0;

    asg_mutex_unlock(&spminfo->multicast_pkt_data_oos.mlock);

    return 0;
}


int emit_async_pkts(struct asgard_device *sdev, struct pminfo *spminfo)
{

    asgard_dbg("multicast is: %d \n", sdev->multicast.enable);

    if (sdev->multicast.enable)
        return emit_async_multicast_pkt(sdev, spminfo);
    else
        return emit_async_unicast_pkts(sdev, spminfo);
}

static inline void asgard_send_oos_pkts(struct asgard_device *sdev,
        struct pminfo *spminfo, const int *target_fire)
{
    int i;

    if (spminfo->num_of_targets == 0) {
        asgard_dbg("No targets for pacemaker. \n");
        return;
    }

    for (i = 0; i < spminfo->num_of_targets; i++) {
        if (!target_fire[i])
            continue;
#ifdef ASGARD_DPDK
        sdev->tx_counter += emit_dpdk_asg_packet(sdev->dpdk_portid, sdev->self_ip,
                                                 sdev->pktmbuf_pool, spminfo->pm_targets[i].pkt_data.naddr,
                                                 spminfo->pm_targets[i].pkt_data.payload,
                                                 spminfo->pm_targets[i].mac_addr, sdev->self_mac);

#elif ASGARD_KERNEL_MODULE

        if(sdev->verbose >= 1){   
            asgard_dbg("Leader Election Payload Dump in %s: \n", __FUNCTION__);
            print_hex_dump(KERN_DEBUG, ": ", DUMP_PREFIX_NONE, 32, 1,
                spminfo->pm_targets[i].pkt_data.payload, 64, 0);
        }

        asgard_update_skb_udp_port(spminfo->pm_targets[i].pkt_data.skb, sdev->tx_port);
        asgard_update_skb_payload(spminfo->pm_targets[i].pkt_data.skb, spminfo->pm_targets[i].pkt_data.payload);

        asgard_send_skb(sdev->ndev, spminfo, spminfo->pm_targets[i].pkt_data.skb);
        sdev->tx_counter++;

#else
        emit_packet(spminfo->pm_targets[i].pkt_data.naddr, spminfo->pm_targets[i].pkt_data.payload);
#endif
        spminfo->pm_targets[i].pkt_tx_counter++;
    }
}


static inline int emit_pkts_non_scheduled(struct asgard_device *sdev,
                                           struct pminfo *spminfo)
{
    struct asgard_payload *pkt_payload = NULL;
    int i;
    int target_fire[CLUSTER_SIZE];
    asgard_dbg("Function under debug %s \n", __FUNCTION__);

    for (i = 0; i < spminfo->num_of_targets; i++) {
        target_fire[i] = spminfo->pm_targets[i].fire;
        asg_mutex_lock(&spminfo->pm_targets[i].pkt_data.mlock);
    }

    asgard_send_oos_pkts(sdev, spminfo, target_fire);

    /* Leave pkts in clean state */
    for (i = 0; i < spminfo->num_of_targets; i++) {

        if (!target_fire[i])
            continue;

        pkt_payload = spminfo->pm_targets[i].pkt_data.payload;

      

        memset(pkt_payload, 0, sizeof(struct asgard_payload));

        // after alive msg has been added, the current active buffer can be used again
        spminfo->pm_targets[i].fire = 0;
    }

    for (i = 0; i < spminfo->num_of_targets; i++) {
        asg_mutex_unlock(&spminfo->pm_targets[i].pkt_data.mlock);
    }

    return 0;
}


static inline int emit_pkts_scheduled(struct asgard_device *sdev,
                                       struct pminfo *spminfo) {
    struct asgard_payload *pkt_payload;
#ifdef ASGARD_KERNEL_MODULE
    struct sk_buff *skb;
#endif

    int i;

    /* Send heartbeats to all targets */
    for(i=0; i< spminfo->num_of_targets; i++) {

        pkt_payload = spminfo->pm_targets[i].hb_pkt_data.payload;

#ifdef ASGARD_DPDK
        sdev->tx_counter += emit_dpdk_asg_packet(sdev->dpdk_portid, sdev->self_ip,
                                                 sdev->pktmbuf_pool, spminfo->pm_targets[i].pkt_data.naddr,
                                                 pkt_payload,
                                                 spminfo->pm_targets[i].mac_addr, sdev->self_mac);
#elif ASGARD_KERNEL_MODULE
        skb  = spminfo->pm_targets[i].hb_pkt_data.skb;

        asgard_update_skb_udp_port(skb, sdev->tx_port);
        asgard_update_skb_payload(skb, pkt_payload);

        /* Send heartbeats to all targets */
        asgard_send_skb(sdev->ndev, spminfo, skb);

        if(sdev->verbose >= 200)
            asgard_dbg("emitted heartbeat for local remote id %d \n", i);

#else
        emit_packet(spminfo->pm_targets[i].hb_pkt_data.naddr, pkt_payload);
#endif

        /* Protocols have been emitted, do not send them again ..
         * .. and free the reservations for new protocols */
        // Not needed when we memset payload to 0 in update skb payload function
        // invalidate_proto_data(pkt_payload);
    }

    return 0;
}

#ifdef ASGARD_KERNEL_MODULE
static inline int asgard_setup_hb_skbs(struct asgard_device  *sdev)
{
    struct pminfo *spminfo = &sdev->pminfo;

    if(sdev->verbose >= 1)
        asgard_dbg("Setting up hb skbs\n");

    if (!spminfo) {
        asgard_error("spminfo is NULL \n");
        //BUG();
        return -1;
    }
    // BUG_ON(spminfo->num_of_targets > MAX_REMOTE_SOURCES);
    if(sdev->multicast.enable) {
        asgard_dbg("broadcast ip: %x  mac: %pM", sdev->multicast.naddr.dst_ip,
                sdev->multicast.naddr.dst_mac);

        spminfo->multicast_pkt_data_oos.skb = 
            asgard_reserve_skb(sdev->ndev, sdev->multicast.naddr.dst_ip, sdev->multicast.naddr.dst_mac, NULL);

        skb_set_queue_mapping(
                spminfo->multicast_pkt_data_oos.skb,
                smp_processor_id()); // Queue mapping same for each target i
        spminfo->multicast_pkt_data_oos.naddr.port = 3321; /* TODO */

        spminfo->multicast_skb = asgard_reserve_skb(
                sdev->ndev,  sdev->multicast.naddr.dst_ip, sdev->multicast.naddr.dst_mac, NULL);
        skb_set_queue_mapping(
                spminfo->multicast_skb,
                smp_processor_id()); // Queue mapping same for each target i

    } else {
        if(sdev->verbose >= 1)
            asgard_dbg("using unicast hb skbs\n");

    }





    return 0;
}

#endif


static int prepare_pm_loop(struct asgard_device *sdev, struct pminfo *spminfo) {

#ifdef ASGARD_KERNEL_MODULE
    if (asgard_setup_hb_skbs(sdev))
		return -1;
#endif


    pm_state_transition_to(spminfo, ASGARD_PM_EMITTING);

    sdev->warmup_state = WARMING_UP;

    if(!sdev->consensus_priv) {
        asgard_error("consensus priv is not initialized\n");
        return 1;
    }

    if(!spminfo->multicast_pkt_data.payload) {
        asgard_error("multicast pkt payload is not initialized\n");
        return 1;
    }

#ifdef ASGARD_KERNEL_MODULE
    get_cpu(); // disable preemption
#endif


#ifdef ASGARD_DPDK
    configure_tx_buffer(sdev->dpdk_portid, 32);
#endif

    return 0;
}

static void postwork_pm_loop(struct asgard_device *sdev) {
    int i;

    // Stopping all protocols
    for (i = 0; i < sdev->num_of_proto_instances; i++) {
        if (sdev->protos[i] != NULL &&
            sdev->protos[i]->ctrl_ops.stop != NULL) {
            sdev->protos[i]->ctrl_ops.stop(sdev->protos[i]);
            asgard_dbg("Stopping Protocol\n");
        }
    }
#ifdef ASGARD_DPDK
    rte_free(tx_buffer);
#endif
    pm_state_transition_to(&sdev->pminfo, ASGARD_PM_READY);
}


int do_pacemaker(void *data) {

    uint64_t prev_time, cur_time;
    struct asgard_device *sdev = (struct asgard_device *) data;
    struct pminfo *spminfo = &sdev->pminfo;
    int scheduled_hb = 0, out_of_sched_hb = 0, async_pkts = 0, out_of_sched_multi = 0;
    uint64_t interval = spminfo->hbi;
    int err = 0;
    int i;

#ifndef ASGARD_KERNEL_MODULE
    signal(SIGINT, &trap);
#endif

    if(interval <= 0){
        asgard_error("Invalid hbi of %llu set!\n",(unsigned long long) interval);
        pm_state_transition_to(spminfo, ASGARD_PM_FAILED);
        return -1;
    }

    asgard_dbg("Starting Pacemaker with hbi: %llu\n", (unsigned long long) interval);

    /* Reset Errors from previous runs */
    spminfo->errors = 0;

    if (prepare_pm_loop(sdev, spminfo)){
        pm_state_transition_to(spminfo, ASGARD_PM_FAILED);
        return -1;
    }

    prev_time = ASGARD_TIMESTAMP;
    while (pacemaker_is_alive(spminfo)) {

        cur_time = ASGARD_TIMESTAMP;

        out_of_sched_hb = 0;
        async_pkts = 0;
        out_of_sched_multi = 0;

        /* Scheduled Multicast Heartbeats */
        scheduled_hb = scheduled_tx(prev_time, cur_time, interval);

        if (spminfo->errors > 1000) {
            asgard_dbg("Errors over threshold - quitting pacemaker\n");
            pm_state_transition_to(spminfo, ASGARD_PM_FAILED);
            break;
        }

        /* Heartbeat messages */
        if (scheduled_hb)
            goto emit;

        /* If in Sync Window, do not send anything until the Heartbeat has been sent */
        if (!check_async_window(prev_time, cur_time, interval, spminfo->waiting_window))
            continue;

        /* Irregular heartbeats - e.g. Leader Election Messages  */
        out_of_sched_hb = out_of_schedule_tx(sdev);

        if (out_of_sched_hb)
            goto emit;


        out_of_sched_multi = out_of_schedule_multi_tx(sdev);

        if (out_of_sched_multi)
            goto emit;

        /* Asynchronous messages - e.g. Log Replication Messages */
        async_pkts = check_async_door(sdev);

        if (async_pkts)
            goto emit;

        continue;
emit:
        if (scheduled_hb) {
           // asgard_dbg("(scheduled_hb=%d, interval=%lld)scheduled hb: cur_time= %lld, prev_time=%lld\n",scheduled_hb, interval, cur_time, prev_time);
            sdev->hb_interval++;

            prev_time = cur_time;
            err = emit_pkts_scheduled(sdev, spminfo);

            //  Post HB emission work. Setup next HB message and update states
            for(i = 0; i< spminfo->num_of_targets; i++)
                pre_hb_setup(sdev, spminfo->pm_targets[i].hb_pkt_data.payload, i);

            for(i = 0; i < spminfo->num_of_targets; i++)
                update_aliveness_states(sdev, spminfo, i);

            if (sdev->consensus_priv && sdev->consensus_priv->nstate != LEADER) {
                update_leader(sdev, spminfo);
            }

            /* check on every heartbeat if we have something to replicate */
            check_pending_log_rep(sdev);

        } else if (out_of_sched_hb) {
            err = emit_pkts_non_scheduled(sdev, spminfo);
        } else if (async_pkts) {
            err = emit_async_pkts(sdev, spminfo);
        } else if (out_of_sched_multi) {
            //err = emit_pkts_non_scheduled_multi(sdev, spminfo);
        }
        if (err) {
            asgard_pm_stop(spminfo);
            return -1;
        }
    }
    asgard_dbg("Exiting pacemaker \n");
    postwork_pm_loop(sdev);
    return 0;
}


#ifdef ASGARD_DPDK
int pacemaker(void *data){
    return do_pacemaker(data);
}
#else
void *pacemaker(void *data) {
    do_pacemaker(data);
    return NULL;
}
#endif


void init_pacemaker(struct pminfo *spminfo){

    spminfo->hbi = 0;
    spminfo->num_of_targets = 0;
    spminfo->waiting_window = 10000000;
    spminfo->cluster_id = -1;
    spminfo->debug_counter = 0;

    /* Init packet Buffers */
    spminfo->multicast_pkt_data.payload = ACMALLOC(1, sizeof(struct asgard_payload), GFP_KERNEL);
    spminfo->multicast_pkt_data_oos.payload = ACMALLOC(1, sizeof(struct asgard_payload), GFP_KERNEL);
    spminfo->multicast_pkt_data.payload->protocols_included = 0;
    spminfo->multicast_pkt_data_oos.payload->protocols_included = 0;

    pm_state_transition_to(spminfo, ASGARD_PM_READY);
}



#ifdef ASGARD_KERNEL_MODULE

static int validate_pm(struct asgard_device *sdev, struct pminfo *spminfo)
{
    if (!spminfo) {
        asgard_error("No Device. %s\n", __func__);
        return -ENODEV;
    }

    if (spminfo->state != ASGARD_PM_READY) {
        asgard_error("Pacemaker is not in ready state!\n");
        return -EPERM;
    }

    if (!sdev) {
        asgard_error("No sdev\n");
        return -ENODEV;
    }

    if (!sdev->ndev) {
        asgard_error("netdevice is NULL\n");
        return -EINVAL;
    }

    if (!sdev->self_mac) {
        asgard_error("self mac is NULL\n");
        return -EINVAL;
    }

    return 0;
}


int asgard_pm_start_loop(void *data)
{
    struct pminfo *spminfo = (struct pminfo *)data;
    struct asgard_device *sdev =
            container_of(spminfo, struct asgard_device, pminfo);
    struct cpumask *mask = AMALLOC(sizeof(struct cpumask), GFP_KERNEL);
    int err;

    asgard_dbg("asgard_pm_start_loop\n");

    err = validate_pm(sdev, spminfo);

    if (err)
        return err;
    cpumask_clear(mask);

    heartbeat_task =
            kthread_create(&do_pacemaker, sdev, "asgard pm loop");

    kthread_bind(heartbeat_task, spminfo->active_cpu);

    if (IS_ERR(heartbeat_task)) {
        asgard_error("Task Error. %s\n", __func__);
        return -EINVAL;
    }

    wake_up_process(heartbeat_task);

    return 0;
}

#endif

#ifdef ASGARD_KERNEL_MODULE
void asgard_reset_remote_host_counter(int asgard_id)
{
    int i;
    struct asgard_device *sdev = get_sdev(asgard_id);
    struct asgard_pm_target_info *pmtarget;

    for (i = 0; i < MAX_REMOTE_SOURCES; i++) {
        pmtarget = &sdev->pminfo.pm_targets[i];
        //kfree(pmtarget->pkt_data.payload);
        AFREE(pmtarget->pkt_data.payload);
    }

    sdev->pminfo.num_of_targets = 0;

    asgard_dbg("reset number of targets to 0\n");
}
EXPORT_SYMBOL(asgard_reset_remote_host_counter);


int asgard_pm_reset(struct pminfo *spminfo)
{
    struct asgard_device *sdev;

    asgard_dbg("Reset Pacemaker Configuration\n");

    if (!spminfo) {
        asgard_error("No Device. %s\n", __func__);
        return -ENODEV;
    }

    if (spminfo->state == ASGARD_PM_EMITTING) {
        asgard_error(
                "Can not reset targets when pacemaker is running\n");
        return -EPERM;
    }

    sdev = container_of(spminfo, struct asgard_device, pminfo);
    asgard_reset_remote_host_counter(sdev->asgard_id);
    return 0;
}

#endif