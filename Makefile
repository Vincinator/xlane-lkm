EXTRA_CFLAGS += -Icommon/

obj-$(CONFIG_SASSY) := connection_layer/core/
obj-$(CONFIG_SASSY) += connection_layer/mlx5/

