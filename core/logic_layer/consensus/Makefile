EXTRA_CFLAGS+=-I$(src)/../../common/
EXTRA_CFLAGS+=-I$(src)/../Synbuf/kernel-space/include

obj-m := asgard_consensus.o

asgard_consensus-objs := asgard_consensus_ops.o state_transitions.o leader.o follower.o candidate.o asgard_consensus_config_ctrl.o
asgard_core-objs += asgard_consensus_request_gen.o log.o asgard_consensus_eval_ctrl.o

all:
	make -C $(ASGARD_KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(ASGARD_KERNEL_SRC) M=$(PWD) clean
