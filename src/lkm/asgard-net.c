//
// Created by vincent on 1/25/21.
//

#include "asgard-net.h"




struct asgard_payload *get_payload_ptr(struct asgard_async_pkt *pkt)
{
    unsigned char *tail_ptr;
    unsigned char *data_ptr;

    tail_ptr = skb_tail_pointer(pkt->skb);
    data_ptr = (tail_ptr - ASGARD_PAYLOAD_BYTES);

    return (struct asgard_payload *) data_ptr;

}
EXPORT_SYMBOL(get_payload_ptr);

void asgard_hex_to_ip(char *retval, u32 dst_ip)
{
    sprintf(retval, "%d.%d.%d.%d", ((dst_ip & 0xff000000) >> 24),
            ((dst_ip & 0x00ff0000) >> 16), ((dst_ip & 0x0000ff00) >> 8),
            ((dst_ip & 0x000000ff)));
}


#ifndef CONFIG_KUNIT
struct net_device *asgard_get_netdevice(int ifindex)
{
    struct net_device *ndev = NULL;

    ndev = first_net_device(&init_net);

    while (ndev != NULL) {
        if (ndev->ifindex == ifindex)
            return ndev;
        
        ndev = next_net_device(ndev);
    }

    asgard_error("Netdevice is NULL %s\n", __func__);
    return NULL;
}
EXPORT_SYMBOL(asgard_get_netdevice);
#else
struct net_device *asgard_get_netdevice(int ifindex)
{
	return NULL;
}
EXPORT_SYMBOL(asgard_get_netdevice);
#endif



int is_ip_local(struct net_device *dev,	u32 ip_addr)
{
    u32 local_ipaddr;

    if(!dev ||! dev->ip_ptr ||! dev->ip_ptr->ifa_list || !dev->ip_ptr->ifa_list->ifa_address) {
        asgard_error("Network Device not initialized properly!\n");
        return 0;
    }

    local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);

    return local_ipaddr == ip_addr;
}
EXPORT_SYMBOL(is_ip_local);

struct sk_buff *asgard_reserve_skb(struct net_device *dev, u32 dst_ip, unsigned char *dst_mac, struct asgard_payload *payload)
{
    int ip_len, udp_len, asgard_len, total_len;
    struct sk_buff *skb;
    struct ethhdr *eth;
    struct iphdr *iph;
    struct udphdr *udph;
    static atomic_t ip_ident;
    u32 local_ipaddr;

    local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);

    asgard_len = sizeof(struct asgard_payload);
    udp_len = asgard_len + sizeof(struct udphdr);
    ip_len = udp_len + sizeof(struct iphdr);

    total_len = ip_len + ETH_HLEN;

    //asgard_dbg("Allocating %d Bytes for SKB\n", total_len);
    //asgard_dbg("asgard_len = %d Bytes\n", asgard_len);

    //  asgard_dbg("LL_RESERVED_SPACE_EXTRA(dev,0) = %d Bytes\n", LL_RESERVED_SPACE_EXTRA(dev,0));

    // head == data == tail
    // end = head + allocated skb size
    skb = alloc_skb(total_len, GFP_KERNEL);

    if (!skb) {
        asgard_error("Could not allocate SKB\n");
        return NULL;
    }

    refcount_set(&skb->users, 1);

    // data == tail == head + ETH_HLEN + sizeof(struct iphdr)+ sizeof(struct udphdr)
    // end = head + allocated skb size
    // reserving headroom for header to expand
    skb_reserve(skb,   ETH_HLEN + sizeof(struct iphdr)+ sizeof(struct udphdr));

    skb->dev = dev;

    // data == tail - sizeof(struct asgard_payload)
    // reserve space for payload
    skb_put(skb, sizeof(struct asgard_payload));

    // init skb user payload with 0
    memset(skb_tail_pointer(skb) - sizeof(struct asgard_payload), 0, sizeof(struct asgard_payload));

    // data = data - sizeof(struct udphdr)
    skb_push(skb, sizeof(struct udphdr));

    skb_reset_transport_header(skb);

    udph = udp_hdr(skb);

    udph->source = htons((u16) 1111);

    udph->dest = htons((u16) 3319);

    udph->len = htons(udp_len);

    udph->check = 0;

    udph->check = csum_tcpudp_magic(local_ipaddr, dst_ip, udp_len, IPPROTO_UDP,csum_partial(udph, udp_len, 0));

    if (udph->check == 0)
        udph->check = CSUM_MANGLED_0;

    // data = data - sizeof(struct iphdr)
    skb_push(skb, sizeof(struct iphdr));

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

    put_unaligned(local_ipaddr, &(iph->saddr));
    put_unaligned(dst_ip, &(iph->daddr));

    iph->check
            = ip_fast_csum((unsigned char *)iph, iph->ihl);

    // data = data - ETH_HLEN
    eth = skb_push(skb, ETH_HLEN);

    skb_reset_mac_header(skb);
    skb->protocol = eth->h_proto = htons(ETH_P_IP);

    ether_addr_copy(eth->h_source, dev->dev_addr);
    ether_addr_copy(eth->h_dest, dst_mac);

    skb->dev = dev;

    return skb;
}
EXPORT_SYMBOL(asgard_reserve_skb);



#ifndef CONFIG_KUNIT
void send_pkt(struct net_device *ndev, struct sk_buff *skb)
{
    int ret;
    struct netdev_queue *txq;

    txq = skb_get_tx_queue(ndev, skb);
    skb_get(skb); /* keep this. otherwise this thread locks the system */

    HARD_TX_LOCK(ndev, txq, smp_processor_id());

    if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
        asgard_error("Device Busy unlocking (send_pkt).\n");
        goto unlock;
    }

    ret = netdev_start_xmit(skb, ndev, txq, 0);
    switch (ret) {
        case NETDEV_TX_OK:
            asgard_dbg("NETDEV TX OK\n");
            break;
        case NET_XMIT_DROP:
            asgard_dbg("NETDEV TX DROP\n");
            break;
        case NET_XMIT_CN:
            asgard_dbg("NETDEV XMIT CN\n");
            break;
        default:
            asgard_dbg("NETDEV UNKNOWN \n");
            /* fall through */
        case NETDEV_TX_BUSY:
            asgard_dbg("NETDEV TX BUSY\n");
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



