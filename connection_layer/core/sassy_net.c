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
                                ((dst_ip & 0x00ff0000) >> 16),
                                ((dst_ip & 0x0000ff00) >> 8),
                                ((dst_ip & 0x000000ff)));
}

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
		return  (byte0 << 24) + (byte1 << 16) + (byte2 << 8) + byte3;

	return -EINVAL;
}

/*
 * Converts an MAC address to hex char array
 */
unsigned char *sassy_convert_mac(const char *str)
{
	unsigned int tmp_data[6];
	unsigned char *bytestring_mac = kmalloc(sizeof(unsigned char) * 6, GFP_ATOMIC);
	int i;

	if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x", &tmp_data[0], &tmp_data[1],
		&tmp_data[2], &tmp_data[3], &tmp_data[4], &tmp_data[5]) == 6) {

		for (i = 0; i < 6; i++)
			bytestring_mac[i] = (unsigned char)tmp_data[i];
		return bytestring_mac;
	}

	return NULL;
}



struct sk_buff *compose_skb(struct net_device *dev, char *src_mac, char *dst_mac, uint32_t src_ip, uint32_t dst_ip, char *bytes, int length)
{
	struct sk_buff *skb;
	struct iphdr *iph_skb_quick;
	struct ethhdr *mac_skb_quick;
	struct udphdr *uhdr;

	skb = alloc_skb(256, GFP_ATOMIC| __GFP_DMA);

	skb_reserve(skb, UDP_LENGTH + IP_LENGTH + ETH_LENGTH + length);

	skb->dev = dev;

	/* prepare reply */
	skb_push(skb, length);
	memcpy(skb->data, bytes, length);
	printk(KERN_INFO "assemble_skb: Added %d bytes of Data\n", length);
	skb_push(skb, sizeof(struct udphdr));
	uhdr = (struct udphdr *) skb->data; // udp_hdr(skb);
	uhdr->source = htons((u16) 1111);
	uhdr->dest = htons((u16) 319); // Precision Time Protocol
	uhdr->len = htons((u16) (length + UDP_LENGTH));

	/* Put IP header */
	skb_push(skb, sizeof(struct iphdr));
	skb_reset_network_header(skb);
	iph_skb_quick = ip_hdr(skb);
	iph_skb_quick->version = IP_HEADER_VERSION;
	iph_skb_quick->ihl = IP_HEADER_LENGTH;
	iph_skb_quick->id = 0x09bc; // + counter;
	iph_skb_quick->ttl = SKB_QUICK_TTL;
	iph_skb_quick->tot_len = htons((u16) (length + IP_LENGTH + UDP_LENGTH));
	iph_skb_quick->saddr = htonl(src_ip);
	iph_skb_quick->daddr = htonl(dst_ip);
	iph_skb_quick->protocol = UDP_PROTOCOL_NUMBER; //UDP
	iph_skb_quick->frag_off = htons((u16) 0x4000);
	ip_send_check(iph_skb_quick);

	/* Put MAC header */
	eth_header(skb, dev, ETH_P_802_3, dst_mac, src_mac, length + UDP_LENGTH + IP_LENGTH + ETH_LENGTH);
	skb_reset_mac_header(skb);
	mac_skb_quick = eth_hdr(skb);
	mac_skb_quick->h_proto = htons((u16) 0x0800);
	return skb;
}