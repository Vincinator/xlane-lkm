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

#include <asguard/asguard.h>
#include <asguard/logger.h>

#define UDP_LENGTH sizeof(struct udphdr)
#define IP_LENGTH sizeof(struct iphdr)
#define ETH_LENGTH sizeof(struct ethhdr)
#define UDP_PROTOCOL_NUMBER 17
#define SKB_QUICK_TTL 64
#define IP_HEADER_VERSION 4
#define IP_HEADER_LENGTH 5

void asguard_hex_to_ip(char *retval, u32 dst_ip)
{
	sprintf(retval, "%d.%d.%d.%d", ((dst_ip & 0xff000000) >> 24),
		((dst_ip & 0x00ff0000) >> 16), ((dst_ip & 0x0000ff00) >> 8),
		((dst_ip & 0x000000ff)));
}

#ifndef CONFIG_KUNIT
struct net_device *asguard_get_netdevice(int ifindex)
{
	struct net_device *ndev = NULL;

	ndev = first_net_device(&init_net);

	while (ndev != NULL) {
		if (ndev->ifindex == ifindex)
			return ndev;
		;
		ndev = next_net_device(ndev);
	}

	asguard_error("Netdevice is NULL %s\n", __func__);
	return NULL;
}
EXPORT_SYMBOL(asguard_get_netdevice);
#else
struct net_device *asguard_get_netdevice(int ifindex)
{
	return NULL;
}
EXPORT_SYMBOL(asguard_get_netdevice);
#endif
/*
 * Converts an IP address from dotted numbers string to hex.
 */
u32 asguard_ip_convert(const char *str)
{
	unsigned int byte0;
	unsigned int byte1;
	unsigned int byte2;
	unsigned int byte3;

	if (sscanf(str, "%u.%u.%u.%u", &byte0, &byte1, &byte2, &byte3) == 4)
		return (byte0 << 24) + (byte1 << 16) + (byte2 << 8) + byte3;

	return -EINVAL;
}
EXPORT_SYMBOL(asguard_ip_convert);

/*
 * Converts an MAC address to hex char array
 */
unsigned char *asguard_convert_mac(const char *str)
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
EXPORT_SYMBOL(asguard_convert_mac);

#ifndef CONFIG_KUNIT
inline struct sk_buff *prepare_heartbeat_skb(struct net_device *dev)
{
	uint16_t skb_size = ASGUARD_PAYLOAD_BYTES + LL_RESERVED_SPACE(dev);
	struct sk_buff *skb = alloc_skb(skb_size, GFP_ATOMIC);

	if (!skb) {
		asguard_error("Could not allocate SKB");
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
#else
inline struct sk_buff *prepare_heartbeat_skb(struct net_device *dev)
{
	return NULL;
}
#endif

#ifndef CONFIG_KUNIT
inline void add_L2_header(struct sk_buff *skb, char *src_mac, char *dst_mac)
{
	struct ethhdr *eth = NULL;

	skb_set_mac_header(skb, 0);

	eth = (struct ethhdr *)skb_put(skb, ETH_HLEN);
	if (!eth) {
		asguard_error("Could not get ethhdr from skb (%s)\n",
			    __func__);
		return;
	}

	eth->h_proto = htons(ETH_P_IP);
	memcpy(eth->h_source, src_mac, ETH_ALEN);
	memcpy(eth->h_dest, dst_mac, ETH_ALEN);
}
#else
inline void add_L2_header(struct sk_buff *skb, char *src_mac, char *dst_mac)
{
}
#endif

#ifndef CONFIG_KUNIT
inline void add_L3_header(struct sk_buff *skb, u32 src_ip, u32 dst_ip)
{
	struct iphdr *ipv4 = NULL;

	skb_set_network_header(skb, sizeof(struct ethhdr));

	ipv4 = (struct iphdr *)skb_put(skb, sizeof(struct iphdr));

	if (!ipv4) {
		asguard_error("Could not get ipv4 Header from skb (%s)\n",
			    __func__);
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
		htons((u16)(ASGUARD_PAYLOAD_BYTES + IP_LENGTH + UDP_LENGTH));
	ipv4->check = 0;

	skb_set_transport_header(skb,
				 sizeof(struct ethhdr) + sizeof(struct iphdr));
}
#else
inline void add_L3_header(struct sk_buff *skb, u32 src_ip, u32 dst_ip)
{

}
#endif

#ifndef CONFIG_KUNIT
inline void add_L4_header(struct sk_buff *skb)
{
	struct udphdr *udp = NULL;

	udp = (struct udphdr *)skb_put(skb, sizeof(struct udphdr));
	if (udp) {
		udp->source = htons((u16)1111);
		udp->dest = htons((u16)3319);
		udp->len = htons((u16)ASGUARD_PAYLOAD_BYTES);
		udp->check = 0;
	}
}
#else
inline void add_L4_header(struct sk_buff *skb)
{

}
#endif

#ifndef CONFIG_KUNIT
inline void add_payload(struct sk_buff *skb, struct asguard_payload *payload)
{
	void *data = (void *)skb_put(skb, ASGUARD_PAYLOAD_BYTES);

	if (!data) {
		asguard_error("Could not get data ptr to skb data\n (%s)",
			    __func__);
		return;
	}

	memcpy(data, payload, ASGUARD_PAYLOAD_BYTES);

//	print_hex_dump(KERN_DEBUG, "Payload: ", DUMP_PREFIX_NONE, 16, 1, data,
//		       ASGUARD_PAYLOAD_BYTES, 0);
}
#else
inline void add_payload(struct sk_buff *skb, struct asguard_payload *payload)
{

}
#endif

int is_ip_local(struct net_device *dev,	u32 ip_addr)
{
	u32 local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);
	return local_ipaddr == ip_addr;
}
EXPORT_SYMBOL(is_ip_local);


struct sk_buff *compose_skb(struct asguard_device *sdev, struct node_addr *naddr,
							struct asguard_payload *payload)
{
	struct sk_buff *nomination_pkt = NULL;
	struct pminfo *spminfo = &sdev->pminfo;
	struct net_device *dev = sdev->ndev;

	u32 local_ipaddr;

	if (!spminfo) {
		asguard_error("spminfo is invalid\n");
		return NULL;
	}

	nomination_pkt = prepare_heartbeat_skb(dev);

	if (!nomination_pkt) {
		asguard_error("Could not compose packet (%s)\n",
			    __func__);
		return NULL;
	}

	local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);

	add_L2_header(nomination_pkt, dev->dev_addr, naddr->dst_mac);
	add_L3_header(nomination_pkt, local_ipaddr, naddr->dst_ip);
	add_L4_header(nomination_pkt);
	add_payload(nomination_pkt, payload);

#if VERBOSE_DEBUG
//	print_hex_dump(KERN_DEBUG, "Payload: ", DUMP_PREFIX_NONE, 16, 1,
//		       payload,
//		       ASGUARD_PAYLOAD_BYTES, 0);

	asguard_dbg("Composed packet\n");
#endif

	return nomination_pkt;
}
EXPORT_SYMBOL(compose_skb);


int compare_mac(unsigned char *m1, unsigned char *m2)
{
	int i;

	for (i = 5; i >= 0; i--)
		if (m1[i] != m2[i])
			return -1;

	return 0;
}

void get_cluster_ids(struct asguard_device *sdev, unsigned char *remote_mac, int *lid, int *cid)
{
	int i;
	struct pminfo *spminfo = &sdev->pminfo;

	*lid = -1;
	*cid = -1;

	for (i = 0; i < spminfo->num_of_targets; i++) {
		if (compare_mac(spminfo->pm_targets[i].pkt_data.naddr.dst_mac, remote_mac) == 0) {
			*cid = spminfo->pm_targets[i].pkt_data.naddr.cluster_id;
			*lid = i;
			return;
		}
	}
}

#ifndef CONFIG_KUNIT
void send_pkt(struct net_device *ndev, struct sk_buff *skb)
{
	int ret;
	struct netdev_queue *txq;

	txq = skb_get_tx_queue(ndev, skb);
	skb_get(skb); /* keep this. otherwise this thread locks the system */

	HARD_TX_LOCK(ndev, txq, smp_processor_id());

	if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
		asguard_error("Device Busy unlocking.\n");
		goto unlock;
	}

	ret = netdev_start_xmit(skb, ndev, txq, 0);
	switch (ret) {
	case NETDEV_TX_OK:
		asguard_dbg("NETDEV TX OK\n");
		break;
	case NET_XMIT_DROP:
		asguard_dbg("NETDEV TX DROP\n");
		break;
	case NET_XMIT_CN:
		asguard_dbg("NETDEV XMIT CN\n");
		break;
	default:
		asguard_dbg("NETDEV UNKNOWN \n");
		/* fall through */
	case NETDEV_TX_BUSY:
		asguard_dbg("NETDEV TX BUSY\n");
		break;
	}

unlock:
	HARD_TX_UNLOCK(ndev, txq);
}
EXPORT_SYMBOL(send_pkt);
#else
void send_pkt(struct net_device *ndev, struct sk_buff *skb)
{

}
EXPORT_SYMBOL(send_pkt);
#endif


