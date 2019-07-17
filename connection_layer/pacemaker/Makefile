ifeq ($(KERNELRELEASE),)
$(info Building SASSY modules out of kernel tree)

KERNEL_SRC ?= ../../../SASSY-Kernel/
EXTRA_CFLAGS += -I../../common/

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.symvers *.order

else

EXTRA_CFLAGS+=-I$(src)/../../common/

obj-y := sassy_pacemaker.o
obj-y += sassy_pacemaker_test.o

endif