#ifndef _SASSY_DEV_H_
#define _SASSY_DEV_H_

#include <linux/types.h>


#define MAX_NIC_DEVICES 8
#define SASSY_PACKET_PAYLOAD_SIZE 32

#define RX_CYCLE_SIZE 1 			/* How many packets per remote host to hold in sassy memory */

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

/*  Packet Buffer for one remote host */
struct sassy_rx_buffer {
	struct sassy_packet packets[RX_CYCLE_SIZE];
	int next_index;
};

/*
 * Table that maps (implicitly) rhostid (by array index) to struct sassy_rx_buffer
 */
struct sassy_rx_table {
	struct sassy_rx_buffer **rhost_buffers;
};

/*
 * Each NIC port gets a unique table. 
 * This struct holds references to all tables.
 * ifindex of NIC PORT corresponds to array position of struct sassy_rx_table *tables. 
 */
struct sassy_core {
	/* NIC independent Data */
	struct sassy_rx_table **rx_tables;	/* Each NIC port (identified by ifindex) has a own table */
 // struct sassy_tx_table **tx_tables; // TODO

	/* NIC specific Data */
	struct sassy_device **sdevices;	

};

#endif /* _SASSY_DEV_H_ */