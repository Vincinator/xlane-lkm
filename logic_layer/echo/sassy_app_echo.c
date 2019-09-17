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

struct proto_instance *get_echo_proto_instance(struct sassy_device *sdev)
{
	struct proto_instance *ins;
	struct sassy_echo_priv *epriv; 
	ins = kmalloc(sizeof(struct sassy_protocol), GFP_KERNEL);

	if(!ins)
		goto error;

	ins->proto_type = SASSY_PROTO_ECHO;
	ins->ctrl_ops = echo_ops;
	ins->name = "echo";
	ins->priv = kmalloc(sizeof(struct sassy_echo_priv), GFP_KERNEL);

	if(!ins->priv)
		goto error;

	epriv = (struct sassy_echo_priv *)ins->priv;

	epriv->sdev = sdev;
	epriv->ins = ins;

    strncpy(epriv->echo_logger.name, "echo", MAX_LOGGER_NAME);
    epriv->echo_logger.ifindex = sdev->ifindex;

	return ins;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_echo_proto_instance);

static void __exit sassy_app_echo_exit(void)
{
	sassy_dbg("exit\n");
}

module_init(sassy_app_echo_init);
module_exit(sassy_app_echo_exit);

