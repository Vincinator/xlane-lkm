#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>

#include "include/sassy_consensus_ops.h"
#include "include/sassy_consensus.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY consensus");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CONSENSUS]"

static struct sassy_protocol consensus_protocol;
static struct consensus_priv priv;


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

	consensus_protocol.proto_type = SASSY_PROTO_CONSENSUS;
	consensus_protocol.ctrl_ops = consensus_ops;
	consensus_protocol.name = "consensus";
	consensus_protocol.priv = (void *)&priv;

	sassy_register_protocol(&consensus_protocol);
	return 0;
}

static void __exit sassy_consensus_exit(void)
{
	sassy_dbg("exit consensus protocol\n");
}

module_init(sassy_consensus_init);
module_exit(sassy_consensus_exit);
