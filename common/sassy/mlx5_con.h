#ifndef _SASSY_MLX5_CON_H_
#define _SASSY_MLX5_CON_H_

#define SASSY_MLX5_DEVICES_LIMIT 5 	/* Number of allowed mlx5 devices that can connect to SASSY */


struct sassy_mlx5_con_info {
	int ix;
	int cqn;
};

int sassy_mlx5_con_register_device(int ifindex);

int sassy_mlx5_con_register_channel(int ix, int cqn);
int sassy_mlx5_con_check_cqn(int cqn);
int sassy_mlx5_con_check_ix(int ix);



int sassy_mlx5_post_optimistical_timestamp(uint64_t cycle_ts);
int sassy_mlx5_post_payload(void);


#endif /* _SASSY_MLX5_CON_H_ */
