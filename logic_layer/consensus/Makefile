# ifeq ($(KERNELRELEASE),)
# $(info Building SASSY modules out of kernel tree)

# KERNEL_SRC ?= /Volumes/workspace/asguard/asguard-kernel/
# EXTRA_CFLAGS += -I../../common/

# all:
# 	make -C $(KERNEL_SRC) M=$(PWD) modules

# clean:
# 	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

# else
EXTRA_CFLAGS+=-I$(src)/../../common/

obj-m := asguard_consensus.o asguard_consensus_ops.o state_transitions.o leader.o follower.o candidate.o asguard_consensus_config_ctrl.o
obj-m += asguard_consensus_request_gen.o log.o asguard_consensus_eval_ctrl.o
#obj-m += event_log/event_logger.o event_log/event_logger_ctrl.o

# endif