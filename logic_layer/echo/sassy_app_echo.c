#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <linux/slab.h>

#include "include/sassy_echo_ops.h"
#include "include/sassy_echo.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY app echo");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PROTO][ECHO]"

static struct sassy_echo_priv echo_priv;

static const struct sassy_protocol_ctrl_ops echo_ops = {
	.init = echo_init,
	.start = echo_start,
	.stop = echo_stop,
	.clean = echo_clean,
	.info = echo_info,
	.post_payload = echo_post_payload,
	.post_ts = echo_post_ts,
	.init_payload = echo_init_payload,

};

static int __init sassy_app_echo_init(void)
{
	sassy_dbg("init\n");
	return 0;
}

struct sassy_protocol *get_echo_proto(void)
{
	struct sassy_protocol *proto;

	proto = kmalloc(sizeof(struct sassy_protocol), GFP_KERNEL);

	if(!proto)
		goto error;

	proto->proto_type = SASSY_PROTO_ECHO;
	proto->ctrl_ops = echo_ops;
	proto->name = "echo";
	proto->priv = (void *)&echo_priv;
	return proto;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_fd_proto);

static void __exit sassy_app_echo_exit(void)
{
	sassy_dbg("exit\n");
}

module_init(sassy_app_echo_init);
module_exit(sassy_app_echo_exit);

