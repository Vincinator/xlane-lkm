ifeq ($(KERNELRELEASE),)
$(info Building SASSY modules out of kernel tree)

KERNEL_SRC ?= /opt/workspace/sassy/SASSY-Kernel/
EXTRA_CFLAGS += -I../../common/

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
    rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

else
EXTRA_CFLAGS+=-I$(src)/../../common/

obj-y := sassy_app_fd.o
sassy_app_fd-objs := sassy_app_fd.o


endif