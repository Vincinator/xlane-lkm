obj-y := sassy_connection_core.o

KERNEL_SRC ?= /opt/workspace/sassy/SASSY-Kernel/

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean