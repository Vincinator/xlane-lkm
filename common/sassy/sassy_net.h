#ifndef _SASSY_NET_DEV_H_
#define _SASSY_NET_DEV_H_


void sassy_hex_to_ip(char *retval, int dst_ip);

/*
 * Converts an IP address from dotted numbers string to hex.
 */
int sassy_ip_convert(const char *str);

/*
 * Converts an MAC address to hex char array
 */
unsigned char *sassy_convert_mac(const char *str);

struct sk_buff *compose_skb(struct net_device *dev,
								char *src_mac, char *dst_mac,
								uint32_t src_ip, uint32_t dst_ip,
								char *bytes, int length);

#endif