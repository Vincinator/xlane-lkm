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

#include <sassy/sassy.h>
#include <sassy/logger.h>

#define UDP_LENGTH sizeof(struct udphdr)
#define IP_LENGTH sizeof(struct iphdr)
#define ETH_LENGTH sizeof(struct ethhdr)
#define UDP_PROTOCOL_NUMBER 17
#define SKB_QUICK_TTL 64
#define IP_HEADER_VERSION 4
#define IP_HEADER_LENGTH 5

void sassy_hex_to_ip(char *retval, int dst_ip)
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
int sassy_ip_convert(const char *str)
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

inline void add_L3_header(struct sk_buff *skb, uint32_t src_ip, uint32_t dst_ip)
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

inline void add_payload(struct sk_buff *skb, void *payload)
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

struct sk_buff *compose_heartbeat_skb(struct net_device *dev,
				      struct pminfo *spminfo,
				      int host_number)
{
	struct sk_buff *hb_pkt = NULL;
	struct sassy_packet_data *hparams;

	uint32_t src_ip;

	if (!spminfo) {
		sassy_error(" spminfo is invalid\n");
		return NULL;
	}

	hparams = &spminfo->pm_targets[host_number].pkt_data;

	hb_pkt = prepare_heartbeat_skb(dev);

	if (!hb_pkt) {
		sassy_error("Could not create heartbeat packet (%s)\n",
			    __FUNCTION__);
		return NULL;
	}

	src_ip = dev->ip_ptr->ifa_list->ifa_address;

	if (!src_ip) {
		sassy_error("No source IP for netdevice condfigured");
		return NULL;
	}

	add_L2_header(hb_pkt, dev->dev_addr, hparams->dst_mac);
	add_L3_header(hb_pkt, src_ip, hparams->dst_ip);
	add_L4_header(hb_pkt);
	add_payload(hb_pkt, hparams->pkt_payload[hparams->hb_active_ix]);

	print_hex_dump(KERN_DEBUG, "Payload: ", DUMP_PREFIX_NONE, 16, 1,
		       hparams->pkt_payload[hparams->hb_active_ix],
		       SASSY_PAYLOAD_BYTES, 0);

	sassy_dbg("Composed Heartbeat\n");
	return hb_pkt;
}
EXPORT_SYMBOL(compose_heartbeat_skb);
