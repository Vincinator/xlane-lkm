#pragma once

#include <sassy/sassy.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif



enum echo_opcode {
	SASSY_PING = 0,
	SASSY_PONG = 1,
};

/* MUST not exceed SASSY_PAYLOAD_BYTES in size! */
struct echo_payload {
	u8 protocol_id; 		// must be the first element
	enum echo_opcode opcode; 	// PING or PONG?
	uint64_t tx_ts; 		// tx of ping  
	//uint64_t rx_ts; 		// rx of pong (on some iface as tx!)
	//uint64_t counter; 		// incremented for each packet, used to check order of packets 
};

#define GET_ECHO_PAYLOAD(p, fld) (*(u32*)((p+offsetof(struct echo_payload, fld))))

#define SET_ECHO_PAYLOAD(p, fld, v) do { \
  *((u32*)(((unsigned char *)p) + offsetof(struct echo_payload, fld))) = v; \
} while (0)


int setup_echo_msg(struct pminfo *spminfo, u32 target_id, uint64_t ts, enum echo_opcode opcode);
