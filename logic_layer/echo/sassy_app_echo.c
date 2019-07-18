#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_echo_ops.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY app echo");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PROTO][ECHO]"


static struct sassy_echo_priv echo_priv;
struct sassy_protocol echo_protocol;

static const struct sassy_protocol_ctrl_ops echo_ops = {
	.init = echo_init,
	.start = echo_start,
	.stop = echo_stop,
	.clean = echo_clean,
	.info = echo_info,
};

static int __init sassy_app_echo_init(void)
{

	sassy_dbg("init\n");
	echo_protocol.protocol_id = 2;
	echo_protocol.name = "echo\0";
	echo_protocol.ctrl_ops = echo_ops;
	echo_protocol.priv = (void*) &echo_priv;
	sassy_register_protocol(&echo_protocol);
	return 0;
}


static void __exit sassy_app_echo_exit(void) 
{

	sassy_dbg("exit\n");
	sassy_remove_protocol(&echo_protocol);

}


module_init(sassy_app_echo_init);
module_exit(sassy_app_echo_exit);