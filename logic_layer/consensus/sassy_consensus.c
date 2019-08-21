#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>

#include <linux/random.h>
#include <linux/timer.h>

#include <sassy/sassy.h>
#include <linux/slab.h>


#include "include/sassy_consensus_ops.h"
#include "include/sassy_consensus.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY consensus");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CONSENSUS]"

ktime_t get_rnd_timeout(void)
{
	return ktime_set(0, MIN_FTIMEOUT_NS +
			prandom_u32_max(MAX_FTIMEOUT_NS - MIN_FTIMEOUT_NS));
}

void set_le_term(struct sassy_payload *pkt_payload, u32 term)
{
	pkt_payload->lep.param1 = term;
}

void set_le_noop(struct sassy_payload *pkt_payload)
{
	pkt_payload->lep.opcode = NOOP;
}

void set_le_opcode(struct sassy_payload *pkt_payload, enum le_opcode opcode, u32 p1, u32 p2)
{
	pkt_payload->lep.opcode = opcode;
	pkt_payload->lep.param1 = p1;
	pkt_payload->lep.param2 = p2;
}

static const struct sassy_protocol_ctrl_ops consensus_ops = {
	.init = consensus_init,
	.start = consensus_start,
	.stop = consensus_stop,
	.clean = consensus_clean,
	.info = consensus_info,
	.post_payload = consensus_post_payload,
	.post_ts = consensus_post_ts,
	.init_payload = consensus_init_payload,
	.us_update = consensus_us_update,
};


static int __init sassy_consensus_init(void)
{
	sassy_dbg("init consensus protocol\n");
	return 0;
}

struct sassy_protocol *get_consensus_proto(void)
{
	struct sassy_protocol *proto;

	proto = kmalloc(sizeof(struct sassy_protocol), GFP_KERNEL);

	if(!proto)
		goto error;

	proto->proto_type = SASSY_PROTO_CONSENSUS;
	proto->ctrl_ops = consensus_ops;
	proto->name = "consensus";
	proto->priv = kmalloc(sizeof(struct consensus_priv), GFP_KERNEL);

	return proto;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_consensus_proto);

static void __exit sassy_consensus_exit(void)
{
	sassy_dbg("exit consensus protocol\n");
}

module_init(sassy_consensus_init);
module_exit(sassy_consensus_exit);
