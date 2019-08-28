ifeq ($(KERNELRELEASE),)

KERNEL_SRC ?= /opt/workspace/sassy/SASSY-Kernel/
EXTRA_CFLAGS += -I../common/

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean

else

obj-y := fd/
obj-y += echo/
obj-y += consensus/
endif
