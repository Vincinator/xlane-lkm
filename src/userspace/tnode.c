/*
 * The node is an user space specific abstraction for managing the test nodes.
 * A node starts the pacemaker (transmitting packets) pthread and the
 * packet listener (receiving packets) pthread.
 *
 * Furthermore, the tnode.c contains the packet handler that extracts the payload from the socket
 * and posts the payload to the asgard handlers.
 */

#include "tnode.h"


#ifdef ASGARD_DPDK
#include <rte_byteorder.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#else
#define BUFSIZE 1024
#endif

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][TNODE]"

#define MAX_PKT_BURST 32


void error(tnode_t *tn, char *msg) {
    perror(msg);
    stop_node(tn);
}

void hex_dump(const void *data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';

    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char *) data)[i]);
        if (((unsigned char *) data)[i] >= ' ' && ((unsigned char *) data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char *) data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8)
                    printf(" ");
                for (j = (i + 1) % 16; j < 16; ++j)
                    printf("   ");

                printf("|  %s \n", ascii);
            }
        }
    }
}


int    user_requested_stop = 0;

void trap(int signal){ user_requested_stop = 1; }


#ifdef ASGARD_DPDK
/*
 * Keeps listening for new packets, and forwards the packet payload to the asgard handlers.
 * Also extracts the sender ip for asgard.
 * The sender can be identified via IP and not via MAC in this user space implementation.
 */
int dpdk_server_listener(void *data) {
    tnode_t *tn = (tnode_t *) data;
    struct rte_mbuf *rx_burst[MAX_PKT_BURST];
    struct rte_mbuf *pkt = NULL;
    unsigned int nb_rx = 0;
    struct rte_ether_hdr *eth_hdr;
    struct rte_vlan_hdr *vlan_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    uint16_t eth_type;
    struct asgard_device *sdev = tn->sdev;
    struct pminfo *spminfo = &sdev->pminfo;
    unsigned int i, l2_len;
    uint8_t *udp_payload;
    uint64_t ots;

    asgard_dbg("Starting Packet listener on port %d\n", tn->port);
    signal(SIGINT, &trap);
    while (tn->is_running && user_requested_stop != 1) {
        nb_rx = rte_eth_rx_burst(tn->sdev->dpdk_portid, 0, rx_burst, 1);

        if (nb_rx) {
            //asgard_dbg("something arrived\n");
            ots = ASGARD_TIMESTAMP;
            if(nb_rx >= 1)
                asgard_dbg("Received %d packets in burst", nb_rx);
            for (i = 0; i < nb_rx; i++) {

                pkt = rx_burst[i];

                eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
                eth_type = rte_cpu_to_be_16(eth_hdr->ether_type);
                l2_len = sizeof(struct rte_ether_hdr);

                if (eth_type == RTE_ETHER_TYPE_IPV4) {
                    ip_hdr = (struct rte_ipv4_hdr *) ((char *) eth_hdr + l2_len);
                    udp_hdr = (struct rte_udp_hdr *) (ip_hdr + 1);

                    if(ip_hdr->next_proto_id == IPPROTO_UDP) {
                        if(udp_hdr->dst_port == 4000){
                            udp_payload = (uint8_t *)(udp_hdr + 1);
                            asg_print_ip(rte_be_to_cpu_32(ip_hdr->src_addr));
                            asgard_post_payload(sdev, rte_be_to_cpu_32(ip_hdr->src_addr), udp_payload, rte_be_to_cpu_16(udp_hdr->dgram_len), ots);
                        }
                    }
                }
            }

            for (i = 0; i < nb_rx; i++)
                rte_pktmbuf_free(rx_burst[i]);
        }

    }
    asgard_dbg("Exited Packet listener\n");
    return 0;
}
#else
/*
 * Keeps listening for new packets, and forwards the packet payload to the asgard handlers.
 * Also extracts the sender ip for asgard.
 * The sender can be identified via IP and not via MAC in this user space implementation.
 */
void *server_listener(void *data) {
    tnode_t *tn = (tnode_t *) data;
    int sockfd;
    char sender_addr_buf[INET_ADDRSTRLEN];
    char *buf;        /* message buf */
    int n;
    socklen_t clientlen;
    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;
    char *hostaddrp;

    asgard_dbg("Starting Packet listener on port %d\n", tn->sdev->tx_port);

    /* 1. Create the Socket */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error(tn, "socket creation failed");
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(4000);

    /* 2. Bind the socket to the specified server address */
    if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        error(tn, "bind failed");
    }

    /* 3. Receive data from peers and post it */
    buf =AMALLOC(BUFSIZE, GFP_KERNEL);
    clientlen = sizeof(clientaddr);
    while (tn->is_running) {
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);

        if (n < 0)
            error(tn, "ERROR in recv call");

        hostaddrp = inet_ntoa(clientaddr.sin_addr);

        if (hostaddrp == NULL)
            error(tn, "ERROR in inet_ntoa\n");

        /* DEBUG: Print the Buffer*/
        inet_ntop(AF_INET, &clientaddr.sin_addr, sender_addr_buf, sizeof(sender_addr_buf));
        asgard_post_payload(tn->sdev, clientaddr.sin_addr.s_addr, buf, BUFSIZE, ASGARD_TIMESTAMP);

    }

    close(sockfd);
    AFREE(buf);
    asgard_dbg("Exited Packet listener\n");
    return 0;
}
#endif

int start_node(tnode_t *tn) {
    asgard_dbg("Starting Node with node id: %d\n", tn->sdev->pminfo.cluster_id);

    tn->is_running = 1;
#ifdef ASGARD_DPDK
#else
    /* Start Pacemaker - NOT FOR DPDK VERSION  */
    pthread_create(&tn->pm_thread, NULL, pacemaker, tn->sdev);

    /* Start Packet listener - NOT FOR DPDK VERSION */
    pthread_create(&tn->pl_thread, NULL, server_listener, tn);
#endif
    return 0;
}

int stop_node(tnode_t *tn) {
    asgard_dbg("Stopping Node with node id: %d\n", tn->sdev->pminfo.cluster_id);

    asgard_pm_stop(&tn->sdev->pminfo);

    tn->is_running = 0;

#ifdef ASGARD_DPDK
#else
    if (pthread_join(tn->pm_thread, NULL)) {
        printf("Failed to join pacemaker thread\n");
    }

    /* Only stops if tn->is_running is set to 0*/
    if (pthread_cancel(tn->pl_thread)) {
        printf("Failed to cancel server listener thread\n");
    }

#endif
    return 0;
}


tnode_t *init_node(tnode_t *tn) {

    tn->sdev =AMALLOC(sizeof(struct asgard_device), GFP_KERNEL);
    tn->num_peers = 0;
    tn->reg_macs = 0;
    tn->reg_ips = 0;
    init_asgard_device(tn->sdev);
    init_pacemaker(&tn->sdev->pminfo);

    return tn;
}


void connect_peer(struct sockaddr_in remote_address) {
    asgard_dbg("Connecting to peer");

}




