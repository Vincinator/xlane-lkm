#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>


static int device_counter;


static int asguard_generate_next_id(void)
{
	if (device_counter >= ASGUARD_MLX5_DEVICES_LIMIT) {
		asguard_error(
			"Reached Limit of maximum connected mlx5 devices.\n");
		asguard_error("Limit=%d, device_counter=%d\n",
			    ASGUARD_MLX5_DEVICES_LIMIT, device_counter);
		return -1;
	}

	return device_counter++;
}
