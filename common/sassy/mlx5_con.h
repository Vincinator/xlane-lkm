#ifndef _SASSY_MLX5_CON_H_
#define _SASSY_MLX5_CON_H_


struct sassy_mlx5_con_info {
	int ix;
	int cqn;
};

int sassy_mlx5_con_register_channel(int ix, int cqn);
int sassy_mlx5_con_check_cqn(int cqn);
int sassy_mlx5_con_check_ix(int ix);

#endif /* _SASSY_MLX5_CON_H_ */
