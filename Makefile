# Core SASSY Components
obj-y := core/

# NIC Integration
obj-y += connection_layer/mlx5/

# In-Kernel Applications
obj-y += logic_layer/fd/
obj-y += logic_layer/echo/
obj-$(CONFIG_SASSY_CONSENSUS) += logic_layer/consensus/



