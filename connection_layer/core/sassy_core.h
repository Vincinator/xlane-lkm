#ifndef _SASSY_DEV_H_
#define _SASSY_DEV_H_

#include <linux/types.h>

#define MAX_REMOTE_SOURCES 16
#define SASSY_PACKET_PAYLOAD_SIZE 32

/* 
 * The size of payload does not exceed the SASSY_PACKET_PAYLOAD_SIZE value.
 * This is true for all sassy packet types!
 */
typedef enum {
	SASSY_HB_TYPE 			= 0,	/* Used by failure detector logic */
	SASSY_CONSENSUS_TYPE 	= 1,	/* Used by consensus logic */
	SASSY_RAW_TYPE			= 2,	/* Unspecified payload structure */
} sassy_packet_type;

/*
 * Represents a single sassy packet
 */
struct sassy_packet {
	sassy_packet_type ptype;					/* Tells Logic Layer how to interpret the payload */
	u8 payload[SASSY_PACKET_PAYLOAD_SIZE]; 		/* Byte addressable array of tasty sassy packet payload*/
};

/*
 * Reference to last packet for each registered remote host
 */
struct sassy_rx_table {
	int table_id;					/* Same as the corresponding NIC ifindex */
	struct sassy_packet *packets; 	/* Index of array corresponds to sassy's remote host id */
	int num_registered_hosts;		/* Num of sassy_packets in packets array. Corresponds to amount of remote hosts */
};

/*
 * Each NIC port gets a unique table. 
 * This struct holds references to all tables.
 * ifindex of NIC PORT corresponds to array position of struct sassy_rx_table *tables. 
 */
struct sassy_core {
	struct sassy_rx_table *tables;	/* Each NIC port (identified by ifindex) has a own table */
};

#endif /* _SASSY_DEV_H_ */