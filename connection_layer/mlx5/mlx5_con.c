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


/* Initialized in sassy_mlx5_con_init*/
struct sassy_mlx5_con_info **infos;

int device_counter = 0;

int sassy_mlx5_con_register_device(int ifindex) {
	if(device_counter >= SASSY_MLX5_DEVICES_LIMIT) {
		sassy_error("Reached Limit of maximum connected mlx5 devices.\n");
		sassy_error("Limit=%d, device_counter=%d\n", SASSY_MLX5_DEVICES_LIMIT, device_counter);
		return -1;
	}

	infos[device_counter] = kmalloc(sizeof(struct sassy_mlx5_con_info), GFP_ATOMIC);

	if(!infos[device_counter]) {
		sassy_error("Allocation error in function %s ", __FUNCTION__);
		return -1;
	}

	sassy_dbg("Register MLX5 Device with ifindex=%d", ifindex);
	sassy_dbg("Assigned sassy_id (%d) to ifindex (%d)", device_counter, ifindex);

	sassy_core_register_nic(device_counter);

	return device_counter++;
}
EXPORT_SYMBOL(sassy_mlx5_con_register_device);

int sassy_mlx5_post_optimistical_timestamp(int sassy_id, uint64_t cycle_ts) {
    sassy_dbg("Not implemented: %s", __FUNCTION__);
	return 0;
}
EXPORT_SYMBOL(sassy_mlx5_post_optimistical_timestamp);

int sassy_mlx5_post_payload(int sassy_id){
    sassy_dbg("Not implemented: %s", __FUNCTION__);
	return 0;
}
EXPORT_SYMBOL(sassy_mlx5_post_payload);

int sassy_mlx5_con_check_ix(int sassy_id, int ix){
	
	if(sassy_id < 0 || sassy_id >= SASSY_MLX5_DEVICES_LIMIT ){
		sassy_error("sassy_id was %d in %s\n", sassy_id __FUNCTION__);
		return 0;
	}

	return infos[sassy_id]->ix == ix;
} 
EXPORT_SYMBOL(sassy_mlx5_con_check_ix);

int sassy_mlx5_con_check_cqn(int sassy_id, int cqn){

	if(sassy_id < 0 || sassy_id >= SASSY_MLX5_DEVICES_LIMIT ){
		sassy_error("sassy_id was %d in %s\n", sassy_id __FUNCTION__);
		return 0;
	}

	return infos[sassy_id]->cqn == cqn;
}
EXPORT_SYMBOL(sassy_mlx5_con_check_cqn);

int sassy_mlx5_con_register_channel(int sassy_id, int ix, int cqn){
	if(sassy_id < 0 || sassy_id >= SASSY_MLX5_DEVICES_LIMIT ){
		sassy_error("sassy_id was %d in %s\n", sassy_id __FUNCTION__);
		return 0;
	}

    infos[sassy_id]->ix = ix;
    infos[sassy_id]->cqn = cqn;

    sassy_dbg("Channel %d registered with cqn=%d", ix, cqn);

    return 0;
}
EXPORT_SYMBOL(sassy_mlx5_con_register_channel);


static int __init sassy_mlx5_con_init(void)
{

    sassy_dbg("init\n");

    infos = kmalloc_array(SASSY_MLX5_DEVICES_LIMIT, sizeof(struct sassy_mlx5_con_info *), GFP_ATOMIC);

    sassy_dbg("init done\n");
    return 0;
}


static void __exit sassy_mlx5_con_exit(void) 
{
    sassy_dbg("exiting.. \n");
    kfree(infos);
    sassy_dbg("exited\n");
}


subsys_initcall(sassy_mlx5_con_init);
module_exit(sassy_mlx5_con_exit); 