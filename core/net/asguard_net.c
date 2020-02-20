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


int is_ip_local(struct net_device *dev,	u32 ip_addr)
{
	u32 local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);
	return local_ipaddr == ip_addr;
}
EXPORT_SYMBOL(is_ip_local);

struct sk_buff *asguard_reserve_skb(struct net_device *dev, u32 dst_ip, unsigned char *dst_mac, struct asguard_payload *payload)
{
    int ip_len, udp_len, asguard_len, total_len;
    struct sk_buff *skb;
    struct ethhdr *eth;
    struct iphdr *iph;
    struct udphdr *udph;
    static atomic_t ip_ident;
    u32 local_ipaddr;

    local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);

    asguard_len = sizeof(struct asguard_payload);
    udp_len = asguard_len + sizeof(struct udphdr);
    ip_len = udp_len + sizeof(struct iphdr);

    total_len = ip_len + LL_RESERVED_SPACE_EXTRA(dev,0);

    asguard_dbg("Allocating %d Bytes for SKB\n", total_len);

    // head == data == tail
    // end = head + allocated skb size
    skb = alloc_skb(total_len, GFP_ATOMIC);
	asguard_dbg("LINE=%d\n", __LINE__);

    if (!skb) {
        asguard_error("Could not allocate SKB\n");
        return NULL;
    }
	asguard_dbg("LINE=%d\n", __LINE__);

    refcount_set(&skb->users, 1);
	asguard_dbg("LINE=%d\n", __LINE__);

    // data == tail == head + LL_RESERVED_SPACE_EXTRA(dev, 0) + ETH_HLEN + sizeof(struct iphdr)+ sizeof(struct udphdr)
    // end = head + allocated skb size
    // reserving headroom for header to expand
    skb_reserve(skb,   LL_RESERVED_SPACE_EXTRA(dev, 0) + ETH_HLEN + sizeof(struct iphdr)+ sizeof(struct udphdr));
	asguard_dbg("LINE=%d\n", __LINE__);

    skb->dev = dev;
	asguard_dbg("LINE=%d\n", __LINE__);

    // data == tail - sizeof(struct asguard_payload)
    // reserve space for payload
    skb_put(skb, sizeof(struct asguard_payload));
	asguard_dbg("LINE=%d\n", __LINE__);

    // init skb user payload with 0
	memset(skb->tail - sizeof(struct asguard_payload), 0,sizeof(struct asguard_payload));
	asguard_dbg("LINE=%d\n", __LINE__);

    // data = data - sizeof(struct udphdr)
    skb_push(skb, sizeof(struct udphdr));
	asguard_dbg("LINE=%d\n", __LINE__);

    skb_reset_transport_header(skb);
	asguard_dbg("LINE=%d\n", __LINE__);

    udph = udp_hdr(skb);

    udph->source = htons((u16) 1111);

    udph->dest = htons((u16) 3319);

    udph->len = htons(udp_len);

    udph->check = 0;

    udph->check = csum_tcpudp_magic(local_ipaddr, dst_ip, udp_len, IPPROTO_UDP,csum_partial(udph, udp_len, 0));
	asguard_dbg("LINE=%d\n", __LINE__);

    if (udph->check == 0)
        udph->check = CSUM_MANGLED_0;

    // data = data - sizeof(struct iphdr)
    skb_push(skb, sizeof(struct iphdr));
	asguard_dbg("LINE=%d\n", __LINE__);

    skb_reset_network_header(skb);

    iph = ip_hdr(skb);

    put_unaligned(0x45, (unsigned char *)iph);

    iph->tos      = 0;
    put_unaligned(htons(ip_len), &(iph->tot_len));
    iph->id       = htons(atomic_inc_return(&ip_ident));
    iph->frag_off = 0;
    iph->ttl      = 64;
    iph->protocol = IPPROTO_UDP;
    iph->check    = 0;
	asguard_dbg("LINE=%d\n", __LINE__);

    put_unaligned(local_ipaddr, &(iph->saddr));
    put_unaligned(dst_ip, &(iph->daddr));
	asguard_dbg("LINE=%d\n", __LINE__);

    iph->check
        = ip_fast_csum((unsigned char *)iph, iph->ihl);
	asguard_dbg("LINE=%d\n", __LINE__);

    // data = data - ETH_HLEN
    eth = skb_push(skb, ETH_HLEN);
	asguard_dbg("LINE=%d\n", __LINE__);

    skb_reset_mac_header(skb);
    skb->protocol = eth->h_proto = htons(ETH_P_IP);
	asguard_dbg("LINE=%d\n", __LINE__);

    ether_addr_copy(eth->h_source, dev->dev_addr);
    ether_addr_copy(eth->h_dest, dst_mac);
	asguard_dbg("LINE=%d\n", __LINE__);

    skb->dev = dev;
	asguard_dbg("LINE=%d\n", __LINE__);

	return skb;
}
EXPORT_SYMBOL(asguard_reserve_skb);


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


