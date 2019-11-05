EXTRA_CFLAGS+=-I$(src)/../../common/

obj-m := asguard_consensus.o

asguard_consensus-objs := asguard_consensus_ops.o state_transitions.o leader.o follower.o candidate.o asguard_consensus_config_ctrl.o
asguard_core-objs += asguard_consensus_request_gen.o log.o asguard_consensus_eval_ctrl.o

all:
	make -C /home/dsp/asguard-kernel-src M=$(PWD) modules

clean:
	make -C /home/dsp/asguard-kernel-src M=$(PWD) clean%
