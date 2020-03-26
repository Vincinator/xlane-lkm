EXTRA_CFLAGS+=-I$(src)/../../common/
EXTRA_CFLAGS+=-I$(src)/../Synbuf/kernel-space/include

obj-m := asguard_consensus.o

asguard_consensus-objs := asguard_consensus_ops.o state_transitions.o leader.o follower.o candidate.o asguard_consensus_config_ctrl.o
asguard_core-objs += asguard_consensus_request_gen.o log.o asguard_consensus_eval_ctrl.o

all:
	make -C $(ASGUARD_KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(ASGUARD_KERNEL_SRC) M=$(PWD) clean
