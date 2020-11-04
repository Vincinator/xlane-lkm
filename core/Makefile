ifeq ($(KERNELRELEASE),)

ifndef ASGARD_KERNEL_SRC
$(error ASGARD_KERNEL_SRC is not set)
endif

all:
	make -C $(ASGARD_KERNEL_SRC) M=$(src) modules

clean:
	make -C $(ASGARD_KERNEL_SRC) M=$(src) clean

else



$(info =================== Asgard Core Make ===================)
EXTRA_CFLAGS += -I$(src)/../common/
EXTRA_CFLAGS += -I$(src)/Synbuf/kernel-space/include

obj-m := asgard.o

asgard-objs := asgard_core.o asgard_core_ctrl.o
asgard-objs += event_logger/event_logger.o event_logger/event_logger_ctrl.o
asgard-objs += net/asgard_net.o net/asgard_async_queue.o
asgard-objs += proto/proto_mgmt.o proto/proto_instance_ctrl.o
asgard-objs += ts/asgard_ts.o ts/asgard_ts_ctrl.o asgard_payload_helper.o
asgard-objs += pm/asgard_pm.o pm/asgard_pm_ctrl.o

# uFace (abstraction on top of synbuf)
asgard-objs += uface/cluster_membership.o uface/consensus_request.o uface/apply_data.o
asgard-objs += uface/ringbuffer.o

# Synbuf
asgard-objs += Synbuf/kernel-space/synbuf-chardev.o


# consensus protocol
asgard-objs += logic_layer/consensus/asgard_consensus.o
asgard-objs += logic_layer/consensus/asgard_consensus_ops.o logic_layer/consensus/state_transitions.o logic_layer/consensus/leader.o logic_layer/consensus/follower.o logic_layer/consensus/candidate.o logic_layer/consensus/asgard_consensus_config_ctrl.o
asgard-objs += logic_layer/consensus/asgard_consensus_request_gen.o logic_layer/consensus/log.o logic_layer/consensus/asgard_consensus_eval_ctrl.o

# echo protocol
asgard-objs += logic_layer/echo/asgard_app_echo.o  logic_layer/echo/asgard_echo_ops.o logic_layer/echo/asgard_echo.o logic_layer/echo/asgard_ping_ctrl.o

# multicast
asgard-objs += multicast/multicast_ctrl.o

endif
