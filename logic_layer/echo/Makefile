ifeq ($(KERNELRELEASE),)
$(info Building SASSY modules out of kernel tree)

KERNEL_SRC ?= /opt/workspace/sassy/SASSY-Kernel/

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
    rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

else

obj-m := sassy_app_echo.o

endif