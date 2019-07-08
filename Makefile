# Core SASSY Components
obj-$(CONFIG_SASSY) 			:= connection_layer/core/
obj-$(CONFIG_SASSY) 			+= control_layer/

# NIC Integration
obj-$(CONFIG_SASSY_MLX5) 		+= connection_layer/mlx5/
obj-$(CONFIG_SASSY_NFP) 		+= connection_layer/nfp/

# In-Kernel Applications
obj-$(CONFIG_SASSY_FD) 			+= logic_layer/fd/
obj-$(CONFIG_SASSY_ECHO) 		+= logic_layer/echo/
obj-$(CONFIG_SASSY_CONSENSUS) 	+= logic_layer/consensus/

