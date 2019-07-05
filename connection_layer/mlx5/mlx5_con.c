#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY MLX5 Connection");
MODULE_VERSION("0.01");


static int __init sassy_mlx5_con_init(void)
{

	printk(KERN_INFO "[SASSY][CON][MLX5] init\n");

	return 0;
}


static void __exit sassy_mlx5_con_exit(void) 
{

	printk(KERN_INFO "[SASSY][CON][MLX5]  cleanup \n");

}


module_init(sassy_mlx5_con_init);
module_exit(sassy_mlx5_con_exit);