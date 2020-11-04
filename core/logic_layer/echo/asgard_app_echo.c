#include <linux/module.h>
#include <linux/kernel.h>
#include <asgard/logger.h>
#include <asgard/asgard.h>
#include <linux/slab.h>

#include "include/asgard_echo_ops.h"
#include <asgard/asgard_echo.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][PROTO][ECHO]"

static const struct asgard_protocol_ctrl_ops echo_ops = {
	.init = echo_init,
	.start = echo_start,
	.stop = echo_stop,
	.clean = echo_clean,
	.info = echo_info,
	.post_payload = echo_post_payload,
	.post_ts = echo_post_ts,
	.init_payload = echo_init_payload,

};

struct proto_instance *get_echo_proto_instance(struct asgard_device *sdev)
{
	struct proto_instance *ins;
	struct echo_priv *epriv;

    // freed by clear_protocol_instances
    ins = kmalloc(sizeof(struct proto_instance), GFP_KERNEL);

	if (!ins)
		goto error;

	ins->proto_type = ASGARD_PROTO_ECHO;
	ins->ctrl_ops = echo_ops;
    ins->logger.instance_id = ins->instance_id;
    ins->logger.ifindex = sdev->ifindex;

    // freed by clear_protocol_instances
    ins->proto_data = kmalloc(sizeof(struct echo_priv), GFP_KERNEL);

	if (!ins->proto_data)
		goto error;

	epriv = (struct echo_priv *)ins->proto_data;

	epriv->sdev = sdev;
	epriv->ins = ins;
	epriv->fire_ping = 0;
    epriv->pong_waiting_interval = 0;
    epriv->last_echo_ts = RDTSC_ASGARD;
    epriv->echo_logger.instance_id = ins->instance_id;
    epriv->echo_logger.ifindex = sdev->ifindex;


	return ins;
error:
	asgard_dbg("Error in %s", __func__);
	return NULL;
}
EXPORT_SYMBOL(get_echo_proto_instance);
