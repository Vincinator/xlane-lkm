#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <linux/slab.h>

#include "include/asguard_fd_ops.h"
#include "include/asguard_fd.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][PROTOCOL][FD]"

static struct asguard_fd_priv fd_priv;

static const struct asguard_protocol_ctrl_ops fd_ops = {
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

struct proto_instance *get_fd_proto_instance(struct asguard_device *sdev)
{
	struct proto_instance *ins;
	struct asguard_fd_priv *fpriv; 

	ins = kmalloc(sizeof(struct proto_instance), GFP_KERNEL);

	if(!ins)
		goto error;

	ins->proto_type = ASGUARD_PROTO_FD;
	ins->ctrl_ops = fd_ops;
	ins->name = "fd";
	ins->logger.name = "fd";
	ins->logger.ifindex = sdev->ifindex;

	ins->proto_data = (void *)&fd_priv;

	fpriv = (struct asguard_fd_priv *)ins->proto_data;

	fpriv->sdev = sdev;
	fpriv->ins = ins;

	return ins;
error:
	asguard_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_fd_proto_instance);
