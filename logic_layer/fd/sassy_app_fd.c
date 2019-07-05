#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY app failure detector");
MODULE_VERSION("0.01");

#define LOG_PREFIX "[SASSY][APP][FD]"



static int __init sassy_connection_core_init(void)
{

	printk(KERN_INFO LOG_PREFIX "init\n");

	return 0;
}


static void __exit sassy_connection_core_exit(void) 
{

	printk(KERN_INFO LOG_PREFIX "cleanup \n");

}


module_init(sassy_connection_core_init);
module_exit(sassy_connection_core_exit);