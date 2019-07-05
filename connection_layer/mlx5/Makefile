obj-y := sassy_mlx5_connection.o

KERNEL_SRC ?= /opt/workspace/sassy/SASSY-Kernel/

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean