EXTRA_CFLAGS=-I$(PWD)/common


obj-$(CONFIG_SASSY) := connection_layer/core/
obj-$(CONFIG_SASSY) += connection_layer/mlx5/

obj-$(CONFIG_SASSY) += control_layer/
obj-$(CONFIG_SASSY) += control_layer/
obj-$(CONFIG_SASSY) += logic_layer/
