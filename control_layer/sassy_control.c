#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/sassy.h>
#include <sassy/logger.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY MLX5 Connection");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CTRL]"



static int __init sassy_control_init(void)
{

	sassy_dbg("init\n");

	return 0;
}


static void __exit sassy_control_exit(void) 
{

	sassy_dbg("exit\n");

}


module_init(sassy_control_init);
module_exit(sassy_control_exit);