#include "kernel-net.h"
#include "../module.h"


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
