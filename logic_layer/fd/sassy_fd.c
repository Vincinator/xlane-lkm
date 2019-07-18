#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_fd_ops.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY app failure detector");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PROTOCOL][FD]"


static struct sassy_fd_priv fd_priv;
struct sassy_protocol fd_protocol;

static const struct sassy_protocol_ctrl_ops fd_ops = {
	.init = fd_init,
	.start = fd_start,
	.stop = fd_stop,
	.clean = fd_clean,
	.info = fd_info,
};

static int __init sassy_fd_init(void){
	sassy_dbg("init\n");

	fd_protocol.protocol_id = 1;
	fd_protocol.name = "fd\0";
	fd_protocol.ctrl_ops = fd_ops;
	fd_protocol.priv = (void*) &fd_priv;

	sassy_register_protocol(&fd_protocol);
	return 0;
}


static void __exit sassy_fd_exit(void) 
{

	sassy_dbg("exit\n");
	
	sassy_remove_protocol(&fd_protocol);
}


module_init(sassy_fd_init);
module_exit(sassy_fd_exit);