#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <linux/slab.h>

#include "include/sassy_fd_ops.h"
#include "include/sassy_fd.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY app failure detector");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PROTOCOL][FD]"

static struct sassy_fd_priv fd_priv;

static const struct sassy_protocol_ctrl_ops fd_ops = {
	.init = fd_init,
	.start = fd_start,
	.stop = fd_stop,
	.clean = fd_clean,
	.info = fd_info,
	.post_payload = fd_post_payload,
	.post_ts = fd_post_ts,
	.init_payload = fd_init_payload,
	.us_update = fd_us_update,

};

static int __init sassy_fd_init(void)
{

	return 0;
}

struct proto_instance *get_fd_proto_instance(struct sassy_device *sdev)
{
	struct proto_instance *ins;
	struct sassy_fd_priv *fpriv; 

	ins = kmalloc(sizeof(struct proto_instance), GFP_KERNEL);

	if(!ins)
		goto error;

	ins->proto_type = SASSY_PROTO_FD;
	ins->ctrl_ops = fd_ops;
	ins->name = "fd";
	ins->priv = (void *)&fd_priv;

	fpriv = (struct sassy_fd_priv *)ins->priv;

	fpriv->sdev = sdev;
	fpriv->ins = ins;

	return proto;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_fd_proto_instance);

static void __exit sassy_fd_exit(void)
{
	
	sassy_dbg("exit\n");
}

module_init(sassy_fd_init);
module_exit(sassy_fd_exit);
