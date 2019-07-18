# Core SASSY Components
obj-$(CONFIG_SASSY) := core/

# NIC Integration
obj-$(CONFIG_SASSY_MLX5) += connection_layer/mlx5/

# In-Kernel Applications
obj-$(CONFIG_SASSY_FD) += logic_layer/fd/
obj-$(CONFIG_SASSY_ECHO) += logic_layer/echo/
obj-$(CONFIG_SASSY_CONSENSUS) += logic_layer/consensus/



