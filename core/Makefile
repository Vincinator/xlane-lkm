ifeq ($(KERNELRELEASE),)

ifndef ASGUARD_KERNEL_SRC
$(error ASGUARD_KERNEL_SRC is not set)
endif

all:
	make -C $(ASGUARD_KERNEL_SRC) M=$(src) modules

clean:
	make -C $(ASGUARD_KERNEL_SRC) M=$(src) clean

else


EXTRA_CFLAGS += -I$(src)/../common/


obj-$(CONFIG_ASGUARD_MODULE) := asguard.o

asguard-objs := asguard_core.o asguard_core_ctrl.o
asguard-objs += event_logger/event_logger.o event_logger/event_logger_ctrl.o
asguard-objs += net/asguard_net.o net/asguard_async_queue.o
asguard-objs += proto/proto_mgmt.o proto/proto_instance_ctrl.o
asguard-objs += ts/asguard_ts.o ts/asguard_ts_ctrl.o asguard_payload_helper.o
asguard-objs += pm/asguard_pm.o pm/asguard_pm_ctrl.o


# consensus protocol
asguard-objs += logic_layer/consensus/asguard_consensus.o
asguard-objs += logic_layer/consensus/asguard_consensus_ops.o logic_layer/consensus/state_transitions.o logic_layer/consensus/leader.o logic_layer/consensus/follower.o logic_layer/consensus/candidate.o logic_layer/consensus/asguard_consensus_config_ctrl.o
asguard-objs += logic_layer/consensus/asguard_consensus_request_gen.o logic_layer/consensus/log.o logic_layer/consensus/asguard_consensus_eval_ctrl.o

# echo protocol
asguard-objs += logic_layer/echo/asguard_app_echo.o  logic_layer/echo/asguard_echo_ops.o  logic_layer/echo/asguard_echo.o


endif
