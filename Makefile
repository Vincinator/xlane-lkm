ifeq ($(KERNELRELEASE),)

EXTRA_CFLAGS += -I$(src)/common/
EXTRA_CFLAGS += -I$(src)/deps/Synbuf/kernel-space/include

EXTRA_CFLAGS += -g -DDEBUG

# Core ASGUARD Components
obj-m := core/
obj-m += tests/core/kunit_basic_test.o
obj-m += tests/raft/kunit_follower.o


# Synbuf
# obj-m += deps/Synbuf/kernel-space/synbuf-chardev.o
# NIC Integration
#obj-m += connection_layer/mlx5/

# In-Kernel Applications
#obj-m += logic_layer/fd/
#obj-m += logic_layer/echo/
#obj-m += logic_layer/consensus/

ASGUARD_MODULES_WORKING_DIR = $(shell pwd)

install:
	rm -Rf build
	mkdir build
	make -C $(ASGUARD_KERNEL_SRC) M=$(ASGUARD_MODULES_WORKING_DIR) modules
	cp core/asguard.ko build/

pull:
    git subtree pull --prefix=core/Synbuf --squash git@github.com:Distributed-Systems-Programming-Group/Synbuf.git synbuf

clean:
	test $(ASGUARD_KERNEL_SRC)
	rm -Rf build
	make -C $(ASGUARD_KERNEL_SRC) M=$(ASGUARD_MODULES_WORKING_DIR) clean

else
$(info =================== ASGuard Top Level Make ===================)

#GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"
EXTRA_CFLAGS += -I$(src)/common/
EXTRA_CFLAGS += -I$(src)/deps/Synbuf/kernel-space/include

#define DEVNAME asgard

#EXTRA_CFLAGS += -DASGUARD_MODULE_VERSION=\"$(GIT_VERSION)\"

# Core ASGUARD Components
obj-m := core/


endif
