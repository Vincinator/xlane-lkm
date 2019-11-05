# Core SASSY Components
obj-m := core/

# NIC Integration
#obj-m += connection_layer/mlx5/

# In-Kernel Applications
#obj-m += logic_layer/fd/
#obj-m += logic_layer/echo/
obj-m += logic_layer/consensus/

all:
	rm -Rf build
	mkdir build
	make -C /home/dsp/asguard-kernel-src M=$(PWD) modules
	cp core/asguard_core.ko build/
	cp connection_layer/mlx5/asguard_mlx5.ko build/
	cp logic_layer/consensus/asguard_consensus.ko build/

clean:
	rm -Rf build
	make -C /home/dsp/asguard-kernel-src M=$(PWD) clean

