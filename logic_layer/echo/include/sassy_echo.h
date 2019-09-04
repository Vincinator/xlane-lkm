#pragma once

#include <sassy/sassy.h>

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


int setup_echo_msg(struct pminfo *spminfo, u32 target_id, uint64_t ts, enum echo_opcode opcode);