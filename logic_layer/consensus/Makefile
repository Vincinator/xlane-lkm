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

obj-m := sassy_consensus.o sassy_consensus_ops.o state_transitions.o leader.o follower.o candidate.o sassy_consensus_config_ctrl.o
obj-m += sassy_consensus_request_gen.o log.o sassy_consensus_eval_ctrl.o
#obj-m += event_log/event_logger.o event_log/event_logger_ctrl.o

endif