#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asgard/logger.h>
#include <asgard/asgard.h>
#include <linux/slab.h>

#include "include/asgard_fd_ops.h"
#include "include/asgard_fd.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][PROTOCOL][FD]"

static struct asgard_fd_priv fd_priv;

static const struct asgard_protocol_ctrl_ops fd_ops = {
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

struct proto_instance *get_fd_proto_instance(struct asgard_device *sdev)
{
	struct proto_instance *ins;
	struct asgard_fd_priv *fpriv;

    // freed by clear_protocol_instances
    ins = kmalloc(sizeof(struct proto_instance), GFP_KERNEL);

	if (!ins)
		goto error;

	ins->proto_type = ASGARD_PROTO_FD;
	ins->ctrl_ops = fd_ops;
	ins->logger.ifindex = sdev->ifindex;

	ins->proto_data = (void *)&fd_priv;

	fpriv = (struct asgard_fd_priv *)ins->proto_data;

	fpriv->sdev = sdev;
	fpriv->ins = ins;

	return ins;
error:
	asgard_dbg("Error in %s", __func__);
	return NULL;
}
EXPORT_SYMBOL(get_fd_proto_instance);
