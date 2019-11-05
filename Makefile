# Core SASSY Components
obj-y := core/

# NIC Integration
obj-y += connection_layer/mlx5/

# In-Kernel Applications
obj-y += logic_layer/fd/
obj-y += logic_layer/echo/
obj-y += logic_layer/consensus/

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean