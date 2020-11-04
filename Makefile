ifeq ($(KERNELRELEASE),)

EXTRA_CFLAGS += -I$(src)/common/
EXTRA_CFLAGS += -I$(src)/deps/Synbuf/kernel-space/include

EXTRA_CFLAGS += -g -DDEBUG

# Core ASGARD Components
obj-m := core/
#obj-m += tests/core/kunit_basic_test.o
#obj-m += tests/raft/kunit_follower.o


# Synbuf
# obj-m += deps/Synbuf/kernel-space/synbuf-chardev.o
# NIC Integration
#obj-m += connection_layer/mlx5/

# In-Kernel Applications
#obj-m += logic_layer/fd/
#obj-m += logic_layer/echo/
#obj-m += logic_layer/consensus/

ASGARD_MODULES_WORKING_DIR = $(shell pwd)

install:
	rm -Rf build
	mkdir build
	make -C $(ASGARD_KERNEL_SRC) M=$(ASGARD_MODULES_WORKING_DIR)
	cp core/asgard.ko build/



pull:
    git subtree pull --prefix=core/Synbuf --squash git@github.com:Distributed-Systems-Programming-Group/Synbuf.git synbuf

clean:
	test $(ASGARD_KERNEL_SRC)
	rm -Rf build
	make -C $(ASGARD_KERNEL_SRC) M=$(ASGARD_MODULES_WORKING_DIR) clean $(KBUILD_CFLAGS)

else
$(info =================== Asgard Top Level Make ===================)


#GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"
EXTRA_CFLAGS += -I$(src)/common/
EXTRA_CFLAGS += -I$(src)/deps/Synbuf/kernel-space/include

#define DEVNAME asgard

#EXTRA_CFLAGS += -DASGARD_MODULE_VERSION=\"$(GIT_VERSION)\"

# Core ASGARD Components
obj-m := core/


endif
