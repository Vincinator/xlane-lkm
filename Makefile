# Core SASSY Components
obj-m := core/

# NIC Integration
obj-m += connection_layer/mlx5/

# In-Kernel Applications
obj-m += logic_layer/fd/
obj-m += logic_layer/echo/
obj-m += logic_layer/consensus/

all:
	make -C /home/dsp/sassy-kernel-src M=$(PWD) modules

clean:
	make -C /home/dsp/sassy-kernel-src M=$(PWD) clean