ifeq ($(KERNELRELEASE),)
$(info Building SASSY modules out of kernel tree)

KERNEL_SRC ?= ../../../SASSY-Kernel/

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.symvers *.order

else

obj-m := sassy_connection_core.o

endif