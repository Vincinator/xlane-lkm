#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <sassy/mlx5_con.h>
#include <sassy/sassy.h>
#include <sassy/logger.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY MLX5 Connection");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CON][MLX5]"

/* Initialized in sassy_mlx5_con_init*/
static struct sassy_mlx5_con_info **infos;
static int mlx5_devices;

int sassy_mlx5_con_register_device(int ifindex)
{
	int sassy_id = sassy_core_register_nic(ifindex);

	if (sassy_id < 0) {
		return -1;
	}

	infos[sassy_id] =
		kmalloc(sizeof(struct sassy_mlx5_con_info), GFP_KERNEL);

	if (!infos[sassy_id]) {
		sassy_error("Allocation error in function %s\n", __FUNCTION__);
		return -1;
	}

	sassy_dbg("Register MLX5 Device with ifindex=%d\n", ifindex);
	sassy_dbg("Assigned sassy_id (%d) to ifindex (%d)\n", sassy_id,
		  ifindex);

	mlx5_devices++;
	return sassy_id;
}
EXPORT_SYMBOL(sassy_mlx5_con_register_device);

int sassy_mlx5_post_optimistical_timestamp(int sassy_id, uint64_t cycle_ts)
{
	if (unlikely(sassy_id < 0))
		return 0;

	sassy_post_ts(sassy_id, cycle_ts);

	return 0;
}
EXPORT_SYMBOL(sassy_mlx5_post_optimistical_timestamp);

int sassy_mlx5_post_payload(int sassy_id, void *va, u32 frag_size, u16 headroom,
			    u32 cqe_bcnt)
{
	u8 *payload = (u8 *)va;

	sassy_post_payload(sassy_id, payload + headroom + 6,
			   payload + headroom + 6 + 6 + 14 + 4 + 8 + 4, cqe_bcnt);

	return 0;
}
EXPORT_SYMBOL(sassy_mlx5_post_payload);

int sassy_mlx5_con_check_ix(int sassy_id, int ix)
{
	if (sassy_id < 0 || sassy_id >= SASSY_MLX5_DEVICES_LIMIT) {
		sassy_error("sassy_id was %d in %s\n", sassy_id, __FUNCTION__);
		return 0;
	}

	return infos[sassy_id]->ix == ix;
}
EXPORT_SYMBOL(sassy_mlx5_con_check_ix);

int sassy_mlx5_con_check_cqn(int sassy_id, int cqn)
{
	if (sassy_id < 0 || sassy_id >= SASSY_MLX5_DEVICES_LIMIT) {
		sassy_error("sassy_id was %d in %s\n", sassy_id, __FUNCTION__);
		return 0;
	}

	return infos[sassy_id]->cqn == cqn;
}
EXPORT_SYMBOL(sassy_mlx5_con_check_cqn);

void *sassy_mlx5_get_channel(int sassy_id)
{
	if (!infos[sassy_id] || !infos[sassy_id]->c) {
		sassy_error("Can not get channel.\n");
		return NULL;
	}
	return infos[sassy_id]->c;
}
EXPORT_SYMBOL(sassy_mlx5_get_channel);

int sassy_mlx5_con_register_channel(int sassy_id, int ix, int cqn, void *c)
{
	if (sassy_validate_sassy_device(sassy_id))
		return -1;

	if (sassy_id < 0 || sassy_id >= SASSY_MLX5_DEVICES_LIMIT) {
		sassy_error("sassy_id was %d in %s\n", sassy_id, __FUNCTION__);
		return 0;
	}

	infos[sassy_id]->ix = ix;
	infos[sassy_id]->cqn = cqn;
	infos[sassy_id]->c = c;

	sassy_dbg("Channel %d registered with cqn=%d", ix, cqn);

	return 0;
}
EXPORT_SYMBOL(sassy_mlx5_con_register_channel);

static int __init sassy_mlx5_con_init(void)
{

	infos = kmalloc_array(SASSY_MLX5_DEVICES_LIMIT,
			      sizeof(struct sassy_mlx5_con_info *), GFP_KERNEL);

	return 0;
}

static void __exit sassy_mlx5_con_exit(void)
{
	int i;

	for (i = 0; i < mlx5_devices; i++)
		kfree(infos[i]);

	kfree(infos);
}

subsys_initcall(sassy_mlx5_con_init);
module_exit(sassy_mlx5_con_exit);
