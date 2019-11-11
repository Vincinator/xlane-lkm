#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <linux/slab.h>

#include "include/asguard_echo_ops.h"
#include "include/asguard_echo.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][PROTO][ECHO]"

static const struct asguard_protocol_ctrl_ops echo_ops = {
	.init = echo_init,
	.start = echo_start,
	.stop = echo_stop,
	.clean = echo_clean,
	.info = echo_info,
	.post_payload = echo_post_payload,
	.post_ts = echo_post_ts,
	.init_payload = echo_init_payload,

};

struct proto_instance *get_echo_proto_instance(struct asguard_device *sdev)
{
	struct proto_instance *ins;
	struct asguard_echo_priv *epriv; 
	ins = kmalloc(sizeof(struct proto_instance), GFP_KERNEL);

	if(!ins)
		goto error;

	ins->proto_type = ASGUARD_PROTO_ECHO;
	ins->ctrl_ops = echo_ops;
	ins->name = "echo";
	ins->logger.name = "echo";
	ins->logger.ifindex = sdev->ifindex;
	ins->proto_data = kmalloc(sizeof(struct asguard_echo_priv), GFP_KERNEL);

	if(!ins->proto_data)
		goto error;

	epriv = (struct asguard_echo_priv *)ins->proto_data;

	epriv->sdev = sdev;
	epriv->ins = ins;

    strncpy(epriv->echo_logger.name, "echo", MAX_LOGGER_NAME);
    epriv->echo_logger.ifindex = sdev->ifindex;

	return ins;
error:
	asguard_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_echo_proto_instance);

static void __exit asguard_app_echo_exit(void)
{
	asguard_dbg("exit\n");
}