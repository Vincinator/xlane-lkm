ifndef ASGUARD_KERNEL_SRC
$(error ASGUARD_KERNEL_SRC is not set)
endif


EXTRA_CFLAGS += -I$(src)/common/

# Core ASGUARD Components
obj-m := core/
obj-y += tests/core/kunit_basic_test.o

# NIC Integration
#obj-m += connection_layer/mlx5/

# In-Kernel Applications
#obj-m += logic_layer/fd/
#obj-m += logic_layer/echo/
#obj-m += logic_layer/consensus/

ASGUARD_MODULES_WORKING_DIR = $(shell pwd)

all:
	rm -Rf build
	mkdir build
	make -C $(ASGUARD_KERNEL_SRC) M=$(ASGUARD_MODULES_WORKING_DIR) modules
	cp core/asguard.ko build/

clean:
	rm -Rf build
	make -C $(ASGUARD_KERNEL_SRC) M=$(ASGUARD_MODULES_WORKING_DIR) clean
