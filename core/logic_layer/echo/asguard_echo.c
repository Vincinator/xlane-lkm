#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asguard/logger.h>

#include <linux/random.h>
#include <linux/timer.h>

#include <asguard/asguard.h>
#include <linux/slab.h>

#include <asguard/payload_helper.h>

#include "include/asguard_echo.h"


void set_echo_opcode(unsigned char *pkt, enum echo_opcode opcode)
{
	SET_ECHO_PAYLOAD(pkt, protocol_id, ASGUARD_PROTO_ECHO);
	SET_ECHO_PAYLOAD(pkt, opcode, opcode);
}

void set_echo_tx_ts(unsigned char *pkt, uint64_t ts)
{
	SET_ECHO_PAYLOAD(pkt, tx_ts, ts);
}

int setup_echo_msg(struct pminfo *spminfo, u32 target_id, uint64_t ts, enum echo_opcode opcode)
{
	struct asguard_payload *pkt_payload;
	int hb_passive_ix;

	hb_passive_ix =
	     !!!spminfo->pm_targets[target_id].pkt_data.hb_active_ix;

	pkt_payload =
     	spminfo->pm_targets[target_id].pkt_data.pkt_payload[hb_passive_ix];

	set_echo_opcode((unsigned char*)pkt_payload, opcode);
	set_echo_tx_ts((unsigned char*)pkt_payload, ts);

	// TODO: Synchronize payload update with leader election..
	//			... otherwise, leader election and protocol would overwrite each other
	//			.... which will lead to a race condition "who updates last before emit wins!"
	spminfo->pm_targets[target_id].pkt_data.hb_active_ix = hb_passive_ix;

	return 0;
}
EXPORT_SYMBOL(setup_echo_msg);