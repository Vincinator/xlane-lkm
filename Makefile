# Core SASSY Components
obj-y := core/

# NIC Integration
obj-y += connection_layer/mlx5/

# In-Kernel Applications
obj-y += logic_layer/fd/
obj-y += logic_layer/echo/
obj-y += logic_layer/consensus/