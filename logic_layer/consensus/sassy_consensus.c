#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY consensus");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CONSENSUS]"

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
	sassy_dbg("init\n");

	return 0;
}

static void __exit sassy_consensus_exit(void)
{
	sassy_dbg("exit\n");
}

module_init(sassy_consensus_init);
module_exit(sassy_consensus_exit);