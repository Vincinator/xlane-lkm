#ifndef _ASGARD_DEV_H_
#define _ASGARD_DEV_H_

#include <linux/types.h>

#define MAX_PROTOCOLS 4

#define MAX_NIC_DEVICES 8
#define ASGARD_PACKET_PAYLOAD_SIZE 32

#define RX_CYCLE_SIZE                                                          \
	1 /* How many packets per remote host to hold in asgard memory */

/*
 * The size of payload does not exceed the ASGARD_PACKET_PAYLOAD_SIZE value.
 * This is true for all asgard packet types!
 */
typedef enum {
	ASGARD_HB_TYPE = 0, /* Used by failure detector logic */
	ASGARD_CONSENSUS_TYPE = 1, /* Used by consensus logic */
	ASGARD_RAW_TYPE = 2, /* Unspecified payload structure */
} asgard_packet_type;

/*
 * Represents a single asgard packet
 */
struct asgard_packet {
	asgard_packet_type
		ptype; /* Tells Logic Layer how to interpret the payload */
	u8 payload[ASGARD_PACKET_PAYLOAD_SIZE]; /* Byte addressable array of tasty asgard packet payload*/
};

/*  Packet Buffer for one remote host */
struct asgard_rx_buffer {
	struct asgard_packet packets[RX_CYCLE_SIZE];
	int next_index;
};

/*
 * Table that maps (implicitly) rhostid (by array index) to struct asgard_rx_buffer
 */
struct asgard_rx_table {
	struct asgard_rx_buffer **rhost_buffers;
};

/*
 * Each NIC port gets a unique table.
 * This struct holds references to all tables.
 * ifindex of NIC PORT corresponds to array position of struct asgard_rx_table *tables.
 */
struct asgard_core {

	/* NIC specific Data */
	struct asgard_device **sdevices;

	/* Number of registered asgard devices */
	int num_devices;
};

//void init_asgard_proto_info_interfaces(void);
//void clean_asgard_proto_info_interfaces(void);

void init_proto_selector(struct asgard_device *sdev);
void remove_proto_selector(struct asgard_device *sdev);

struct asgard_core *asgard_core(void);

int register_protocol_instance(struct asgard_device *sdev, int instance_id, int protocol_id);
void clear_protocol_instances(struct asgard_device *sdev);
#endif /* _ASGARD_DEV_H_ */
