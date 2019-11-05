#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Distributed Systems Group");
MODULE_DESCRIPTION("SASSY MLX5 Connection");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CON][MLX5]"

/* Initialized in asguard_mlx5_con_init*/
static struct asguard_mlx5_con_info **infos;
static int mlx5_devices;


int asguard_mlx5_con_register_device(int ifindex)
{
	int asguard_id = asguard_core_register_nic(ifindex);

	if (asguard_id < 0) {
		return -1;
	}

	infos[asguard_id] =
		kmalloc(sizeof(struct asguard_mlx5_con_info), GFP_KERNEL);

	if (!infos[asguard_id]) {
		asguard_error("Allocation error in function %s\n", __FUNCTION__);
		return -1;
	}
	
	asguard_dbg("Register MLX5 Device with ifindex=%d\n", ifindex);
	asguard_dbg("Assigned asguard_id (%d) to ifindex (%d)\n", asguard_id,
		  ifindex);

	mlx5_devices++;
	return asguard_id;
}
EXPORT_SYMBOL(asguard_mlx5_con_register_device);

int asguard_mlx5_post_optimistical_timestamp(int asguard_id, uint64_t cycle_ts)
{

	if(infos[asguard_id]->asguard_post_ts != NULL)
		infos[asguard_id]->asguard_post_ts(asguard_id, cycle_ts);

	return 0;
}
EXPORT_SYMBOL(asguard_mlx5_post_optimistical_timestamp);

int asguard_mlx5_post_payload(int asguard_id, void *va, u32 frag_size, u16 headroom,
			    u32 cqe_bcnt)
{
	u8 *payload = (u8 *)va;

	if(infos[asguard_id]->asguard_post_payload != NULL)
		infos[asguard_id]->asguard_post_payload(asguard_id, payload + headroom + 6,
			   payload + headroom + 6 + 6 + 14 + 4 + 8 + 4, cqe_bcnt);

	return 0;
}
EXPORT_SYMBOL(asguard_mlx5_post_payload);

int asguard_mlx5_con_check_ix(int asguard_id, int ix)
{
	if (asguard_id < 0 || asguard_id >= SASSY_MLX5_DEVICES_LIMIT) {
		asguard_error("asguard_id was %d in %s\n", asguard_id, __FUNCTION__);
		return 0;
	}

	return infos[asguard_id]->ix == ix;
}
EXPORT_SYMBOL(asguard_mlx5_con_check_ix);

int asguard_mlx5_con_check_cqn(int asguard_id, int cqn)
{
	if (asguard_id < 0 || asguard_id >= SASSY_MLX5_DEVICES_LIMIT) {
		asguard_error("asguard_id was %d in %s\n", asguard_id, __FUNCTION__);
		return 0;
	}

	return infos[asguard_id]->cqn == cqn;
}
EXPORT_SYMBOL(asguard_mlx5_con_check_cqn);

void *asguard_mlx5_get_channel(int asguard_id)
{
	if (!infos[asguard_id] || !infos[asguard_id]->c) {
		asguard_error("Can not get channel.\n");
		return NULL;
	}
	return infos[asguard_id]->c;
}
EXPORT_SYMBOL(asguard_mlx5_get_channel);

int asguard_mlx5_con_register_channel(int asguard_id, int ix, int cqn, void *c)
{

	if (asguard_id < 0 || asguard_id >= SASSY_MLX5_DEVICES_LIMIT) {
		asguard_error("asguard_id was %d in %s\n", asguard_id, __FUNCTION__);
		return 0;
	}

	infos[asguard_id]->ix = ix;
	infos[asguard_id]->cqn = cqn;
	infos[asguard_id]->c = c;

	asguard_dbg("Channel %d registered with cqn=%d", ix, cqn);

	return 0;
}
EXPORT_SYMBOL(asguard_mlx5_con_register_channel);

static int __init asguard_mlx5_con_init(void)
{

	infos = kmalloc_array(SASSY_MLX5_DEVICES_LIMIT,
			      sizeof(struct asguard_mlx5_con_info *), GFP_KERNEL);

	return 0;
}

static void __exit asguard_mlx5_con_exit(void)
{
	int i;

	for (i = 0; i < mlx5_devices; i++)
		kfree(infos[i]);

	kfree(infos);
}

module_init(asguard_mlx5_con_init);
module_exit(asguard_mlx5_con_exit);
