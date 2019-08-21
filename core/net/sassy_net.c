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

#include <sassy/sassy.h>
#include <sassy/logger.h>

#define UDP_LENGTH sizeof(struct udphdr)
#define IP_LENGTH sizeof(struct iphdr)
#define ETH_LENGTH sizeof(struct ethhdr)
#define UDP_PROTOCOL_NUMBER 17
#define SKB_QUICK_TTL 64
#define IP_HEADER_VERSION 4
#define IP_HEADER_LENGTH 5

void sassy_hex_to_ip(char *retval, u32 dst_ip)
{
	sprintf(retval, "%d.%d.%d.%d", ((dst_ip & 0xff000000) >> 24),
		((dst_ip & 0x00ff0000) >> 16), ((dst_ip & 0x0000ff00) >> 8),
		((dst_ip & 0x000000ff)));
}

struct net_device *sassy_get_netdevice(int ifindex)
{
	struct net_device *ndev = NULL;

	ndev = first_net_device(&init_net);

	while (ndev != NULL) {
		if (ndev->ifindex == ifindex)
			return ndev;
		;
		ndev = next_net_device(ndev);
	}

	sassy_error("Netdevice is NULL %s\n", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(sassy_get_netdevice);

/*
 * Converts an IP address from dotted numbers string to hex.
 */
u32 sassy_ip_convert(const char *str)
{
	unsigned int byte0;
	unsigned int byte1;
	unsigned int byte2;
	unsigned int byte3;

	if (sscanf(str, "%u.%u.%u.%u", &byte0, &byte1, &byte2, &byte3) == 4)
		return (byte0 << 24) + (byte1 << 16) + (byte2 << 8) + byte3;

	return -EINVAL;
}
EXPORT_SYMBOL(sassy_ip_convert);

/*
 * Converts an MAC address to hex char array
 */
unsigned char *sassy_convert_mac(const char *str)
{
	unsigned int tmp_data[6];
	unsigned char *bytestring_mac =
		kmalloc(sizeof(unsigned char) * 6, GFP_KERNEL);
	int i;

	if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x", &tmp_data[0], &tmp_data[1],
		   &tmp_data[2], &tmp_data[3], &tmp_data[4],
		   &tmp_data[5]) == 6) {
		for (i = 0; i < 6; i++)
			bytestring_mac[i] = (unsigned char)tmp_data[i];
		return bytestring_mac;
	}

	return NULL;
}
EXPORT_SYMBOL(sassy_convert_mac);

inline struct sk_buff *prepare_heartbeat_skb(struct net_device *dev)
{
	uint16_t skb_size = SASSY_PAYLOAD_BYTES + LL_RESERVED_SPACE(dev);
	struct sk_buff *skb = alloc_skb(skb_size, GFP_ATOMIC);

	if (!skb) {
		sassy_error("Could not allocate SKB");
		return NULL;
	}

	/* Reserve space for network headers */
	skb_reserve(skb, LL_RESERVED_SPACE(dev));

	skb->dev = dev;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_IP);
	skb->ip_summed = CHECKSUM_NONE;
	skb->priority = 0;
	skb->next = skb->prev = NULL;

	return skb;
}

inline void add_L2_header(struct sk_buff *skb, char *src_mac, char *dst_mac)
{
	struct ethhdr *eth = NULL;

	skb_set_mac_header(skb, 0);

	eth = (struct ethhdr *)skb_put(skb, ETH_HLEN);
	if (!eth) {
		sassy_error("Could not get ethhdr from skb (%s)\n",
			    __FUNCTION__);
		return;
	}

	eth->h_proto = htons(ETH_P_IP);
	memcpy(eth->h_source, src_mac, ETH_ALEN);
	memcpy(eth->h_dest, dst_mac, ETH_ALEN);
}

inline void add_L3_header(struct sk_buff *skb, u32 src_ip, u32 dst_ip)
{
	struct iphdr *ipv4 = NULL;

	skb_set_network_header(skb, sizeof(struct ethhdr));

	ipv4 = (struct iphdr *)skb_put(skb, sizeof(struct iphdr));

	if (!ipv4) {
		sassy_error("Could not get ipv4 Header from skb (%s)\n",
			    __FUNCTION__);
		return;
	}

	ipv4->version = IPVERSION;
	ipv4->ihl = sizeof(struct iphdr) >> 2;
	ipv4->tos = 0;
	ipv4->id = 0;
	ipv4->ttl = 0x40;
	ipv4->frag_off = 0;
	ipv4->protocol = IPPROTO_UDP;
	ipv4->saddr = htonl(src_ip);
	ipv4->daddr = htonl(dst_ip);
	ipv4->tot_len =
		htons((u16)(SASSY_PAYLOAD_BYTES + IP_LENGTH + UDP_LENGTH));
	ipv4->check = 0;

	skb_set_transport_header(skb,
				 sizeof(struct ethhdr) + sizeof(struct iphdr));
}

inline void add_L4_header(struct sk_buff *skb)
{
	struct udphdr *udp = NULL;

	udp = (struct udphdr *)skb_put(skb, sizeof(struct udphdr));
	if (udp) {
		udp->source = htons((u16)1111);
		;
		udp->dest = htons((u16)319);
		udp->len = htons((u16)SASSY_PAYLOAD_BYTES);
		udp->check = 0;
	}
}

inline void add_payload(struct sk_buff *skb, struct sassy_payload *payload)
{
	void *data = (void *)skb_put(skb, SASSY_PAYLOAD_BYTES);

	if (!data) {
		sassy_error("Could not get data ptr to skb data\n (%s)",
			    __FUNCTION__);
		return;
	}

	memcpy(data, payload, SASSY_PAYLOAD_BYTES);

	print_hex_dump(KERN_DEBUG, "Payload: ", DUMP_PREFIX_NONE, 16, 1, data,
		       SASSY_PAYLOAD_BYTES, 0);
}

int is_ip_local(struct net_device *dev,	u32 ip_addr)
{
	u32 local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);
	return local_ipaddr == ip_addr;
}
EXPORT_SYMBOL(is_ip_local);


struct sk_buff *compose_skb(struct sassy_device *sdev, struct node_addr *naddr,
							struct sassy_payload *payload)
{
	struct sk_buff *nomination_pkt = NULL;
	struct pminfo *spminfo = &sdev->pminfo;
	struct net_device *dev = sdev->ndev;

	u32 local_ipaddr;

	if (!spminfo) {
		sassy_error("spminfo is invalid\n");
		return NULL;
	}

	nomination_pkt = prepare_heartbeat_skb(dev);

	if (!nomination_pkt) {
		sassy_error("Could not compose packet (%s)\n",
			    __FUNCTION__);
		return NULL;
	}

	local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);

	add_L2_header(nomination_pkt, dev->dev_addr, naddr->dst_mac);
	add_L3_header(nomination_pkt, local_ipaddr, naddr->dst_ip);
	add_L4_header(nomination_pkt);
	add_payload(nomination_pkt, payload);

	print_hex_dump(KERN_DEBUG, "Payload: ", DUMP_PREFIX_NONE, 16, 1,
		       payload,
		       SASSY_PAYLOAD_BYTES, 0);

	sassy_dbg("Composed packet\n");
	return nomination_pkt;
}
EXPORT_SYMBOL(compose_skb);


int compare_mac(unsigned char *m1, unsigned char *m2)
{
	int i;

	for(i = 0; i < 6; i ++)
		if(m1[i] != m2[i])
			return -1;
	
	return 0;
}

int get_ltarget_id(struct sassy_device *sdev, unsigned char *remote_mac)
{
	int i;
	struct pminfo *spminfo = &sdev->pminfo;
	unsigned char *cur_mac = NULL;

	if(!remote_mac){
		sassy_error("remote mac is NULL\n");
		return;
	}

	for(i = 0; i < spminfo->num_of_targets; i++) {
		cur_mac = spminfo->pm_targets[i].pkt_data.naddr.dst_mac;

		if(compare_mac(cur_mac, remote_mac) == 0){

			if(sdev->verbose >= 3)
				sassy_dbg("Found mac in remote host list\n");

			return i;
		}
	}

	if(sdev->verbose >= 3){
		sassy_error("MAC %x:%x:%x:%x:%x:%x is not registered!\n)\n",
			   cur_mac[0],
			   cur_mac[1],
			   cur_mac[2],
			   cur_mac[3],
			   cur_mac[4],
			   cur_mac[5]);
	}
	return -1;

}


void send_pkt(struct net_device *ndev, struct sk_buff *skb)
{
	int ret;
	struct netdev_queue *txq;

	txq = skb_get_tx_queue(ndev, skb);
	skb_get(skb); /* keep this. otherwise this thread locks the system */

	HARD_TX_LOCK(ndev, txq, smp_processor_id());

	if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
		sassy_error("Device Busy unlocking.\n");
		goto unlock;
	}

	ret = netdev_start_xmit(skb, ndev, txq, 0);
unlock:
	HARD_TX_UNLOCK(ndev, txq);
}
EXPORT_SYMBOL(send_pkt);


int send_pkts(struct sassy_device *sdev, struct sk_buff **skbs, int num_pkts)
{
	int i;

	/* If netdev is offline, then stop pacemaker */
	if (unlikely(!netif_running(sdev->ndev) ||
		     !netif_carrier_ok(sdev->ndev))) {
		return -1;
	}

	for (i = 0; i < num_pkts; i++) {
		if(!skbs[i])
			continue;
		send_pkt(sdev->ndev, skbs[i]);
	}
	return 0;
}