#pragma once

#include <asgard/asgard.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

struct echo_priv {
    struct asgard_logger echo_logger;
    struct asgard_device *sdev;
    struct proto_instance *ins;
    struct proc_dir_entry *echo_pupu_entry;
    bool echo_pmpu_entry;
    struct proc_dir_entry *echo_pmpm_entry;
    struct proc_dir_entry *echo_pupm_entry;
    struct proc_dir_entry *echo_ppwt_entry;

    int pong_waiting_interval;
    uint64_t last_echo_ts;
    bool fire_ping;
};

enum echo_logger_events {
	LOG_ECHO_RX_PING_UNI = 0,
	LOG_ECHO_RX_PONG_UNI = 1,
	LOG_ECHO_UNI_LATENCY_DELTA = 2, // timestamp is delta of rx_pong and tx_ping
    LOG_ECHO_RX_PING_MULTI = 3,
    LOG_ECHO_MULTI_LATENCY_DELTA = 4,
};



/* MUST not exceed ASGARD_PAYLOAD_BYTES in size! */
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


void set_echo_opcode(unsigned char *pkt, enum echo_opcode opco,
                     s32 sender_id, s32 receiver_id,
                     s64 ts1, s64 ts2, s64 ts3);


void setup_echo_msg_multi(struct proto_instance *ins, struct pminfo *spminfo,
                          s32 sender_cluster_id, s32 receiver_cluster_id,
                          uint64_t ts1, uint64_t ts2, uint64_t ts3,
                          enum echo_opcode opcode);


void setup_echo_msg_uni(struct proto_instance *ins, struct pminfo *spminfo,
                        int remote_lid, s32 sender_cluster_id, s32 receiver_cluster_id,
                        uint64_t ts1, uint64_t ts2, uint64_t ts3,
                        enum echo_opcode opcode);


void init_ping_ctrl_interfaces(struct echo_priv *priv);

void remove_ping_ctrl_interfaces(struct echo_priv *priv);