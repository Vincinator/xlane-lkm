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


static const struct sassy_fd_priv priv = {
	.state = 0,
	.sdev = NULL,
};

const struct sassy_protocol_ops echo_ops = {
	.init = echo_init,
	.start = echo_start,
	.stop = echo_stop,
	.clean = echo_clean,
	.info = echo_info,
};

struct sassy_protocol echo_protocol = {
	.app_id = 2,
	.name = "echo",
	.ops = echo_ops,
	.priv = priv;
};


static int __init sassy_app_echo_init(void)
{

	sassy_dbg("init\n");

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