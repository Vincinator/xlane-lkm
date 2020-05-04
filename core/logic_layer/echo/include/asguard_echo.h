#pragma once

#include <asguard/asguard.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif



enum echo_logger_events {
	LOG_ECHO_RX_PING_UNI = 0,
	LOG_ECHO_RX_PONG_UNI = 1,
	LOG_ECHO_UNI_LATENCY_DELTA = 2, // timestamp is delta of rx_pong and tx_ping
    LOG_ECHO_RX_PING_MULTI = 3,
    LOG_ECHO_MULTI_LATENCY_DELTA = 4,
};

enum echo_opcode {
	ASGUARD_PING_REQ_UNI = 0,
	ASGUARD_PONG_UNI = 1,
    ASGUARD_PING_REQ_MULTI = 2,
    ASGUARD_PONG_MULTI = 3,

};

/* MUST not exceed ASGUARD_PAYLOAD_BYTES in size! */
struct echo_payload {
	u16 protocol_id; 		// must be the first element
	u16 offset;
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
