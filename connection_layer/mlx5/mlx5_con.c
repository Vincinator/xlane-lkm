#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>
#include <linux/slab.h>

#include <sassy/mlx5_con.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY MLX5 Connection");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CON][MLX5]"

struct sassy_mlx5_con_info *info;


int sassy_mlx5_con_check_ix(int ix){
	return info->ix == ix;
} 
EXPORT_SYMBOL(sassy_mlx5_con_check_ix);

int sassy_mlx5_con_check_cqn(int cqn){
	return info->cqn == cqn;
}
EXPORT_SYMBOL(sassy_mlx5_con_check_cqn);

int sassy_mlx5_con_register_channel(int ix, int cqn){

    info->ix = ix;
    info->cqn = cqn;
    sassy_dbg("Channel %d registered with cqn=%d", ix, cqn);

    return 0;
}
EXPORT_SYMBOL(sassy_mlx5_con_register_channel);


static int __init sassy_mlx5_con_init(void)
{

    sassy_dbg("init\n");

    info = kmalloc(sizeof(struct sassy_mlx5_con_info), GFP_ATOMIC);

    sassy_dbg("init done\n");
    return 0;
}


static void __exit sassy_mlx5_con_exit(void) 
{
    sassy_dbg("exiting.. \n");
    kfree(info);
    sassy_dbg("exited\n");
}


subsys_initcall(sassy_mlx5_con_init);
module_exit(sassy_mlx5_con_exit); 