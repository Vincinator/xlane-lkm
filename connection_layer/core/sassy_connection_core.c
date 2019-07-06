#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>

#include "sassy_core.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY Connection Core");
MODULE_VERSION("0.01");

#define LOG_PREFIX "[SASSY][CORE]"

/* RX Data */
struct sassy_core *score;

static int __init sassy_connection_core_init(void)
{

	sassy_dbg("init\n");

	return 0;
}

static void __exit sassy_connection_core_exit(void) 
{

	sassy_dbg("exit\n");

}

module_init(sassy_connection_core_init);
module_exit(sassy_connection_core_exit);