#ifndef _ASGUARD_DEV_H_
#define _ASGUARD_DEV_H_

#include <linux/types.h>

#define MAX_PROTOCOLS 4

#define MAX_NIC_DEVICES 8
#define ASGUARD_PACKET_PAYLOAD_SIZE 32

#define RX_CYCLE_SIZE                                                          \
	1 /* How many packets per remote host to hold in asguard memory */

/* 
 * The size of payload does not exceed the ASGUARD_PACKET_PAYLOAD_SIZE value.
 * This is true for all asguard packet types!
 */
typedef enum {
	ASGUARD_HB_TYPE = 0, /* Used by failure detector logic */
	ASGUARD_CONSENSUS_TYPE = 1, /* Used by consensus logic */
	ASGUARD_RAW_TYPE = 2, /* Unspecified payload structure */
} asguard_packet_type;

/*
 * Represents a single asguard packet
 */
struct asguard_packet {
	asguard_packet_type
		ptype; /* Tells Logic Layer how to interpret the payload */
	u8 payload[ASGUARD_PACKET_PAYLOAD_SIZE]; /* Byte addressable array of tasty asguard packet payload*/
};

/*  Packet Buffer for one remote host */
struct asguard_rx_buffer {
	struct asguard_packet packets[RX_CYCLE_SIZE];
	int next_index;
};

/*
 * Table that maps (implicitly) rhostid (by array index) to struct asguard_rx_buffer
 */
struct asguard_rx_table {
	struct asguard_rx_buffer **rhost_buffers;
};

/*
 * Each NIC port gets a unique table. 
 * This struct holds references to all tables.
 * ifindex of NIC PORT corresponds to array position of struct asguard_rx_table *tables. 
 */
struct asguard_core {
	/* NIC independent Data */
	struct asguard_rx_table **
		rx_tables; /* Each NIC port (identified by ifindex) has a own table */

	/* NIC specific Data */
	struct asguard_device **sdevices;
};

//void init_asguard_proto_info_interfaces(void);
//void clean_asguard_proto_info_interfaces(void);

void init_proto_selector(struct asguard_device *sdev);
void remove_proto_selector(struct asguard_device *sdev);

struct asguard_core *asguard_core(void);

int register_protocol_instance(struct asguard_device *sdev, int instance_id, int protocol_id);
void clear_protocol_instances(struct asguard_device *sdev);
#endif /* _ASGUARD_DEV_H_ */
