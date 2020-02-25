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
	asguard_dbg("%s, %d", __FUNCTION__, __LINE__);
    mutex_lock(&spminfo->pm_targets[target_id].pkt_data.pkt_lock);

	pkt_payload =
		spminfo->pm_targets[target_id].pkt_data.pkt_payload;

	set_echo_opcode((unsigned char *)pkt_payload, opcode);
	set_echo_tx_ts((unsigned char *)pkt_payload, ts);
    mutex_unlock(&spminfo->pm_targets[target_id].pkt_data.pkt_lock);
    asguard_dbg("%s, %d", __FUNCTION__, __LINE__);
	return 0;
}
EXPORT_SYMBOL(setup_echo_msg);
