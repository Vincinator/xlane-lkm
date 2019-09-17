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

struct sassy_protocol *get_fd_proto(struct sassy_device *sdev)
{
	struct sassy_protocol *proto;
	struct sassy_fd_priv *fpriv; 

	proto = kmalloc(sizeof(struct sassy_protocol), GFP_KERNEL);

	if(!proto)
		goto error;

	proto->proto_type = SASSY_PROTO_FD;
	proto->ctrl_ops = fd_ops;
	proto->name = "fd";
	proto->priv = (void *)&fd_priv;

	fpriv = (struct sassy_fd_priv *)proto->priv;

	fpriv->sdev = sdev;

	return proto;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_fd_proto);

static void __exit sassy_fd_exit(void)
{
	
	sassy_dbg("exit\n");
}

module_init(sassy_fd_init);
module_exit(sassy_fd_exit);
