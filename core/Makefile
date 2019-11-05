EXTRA_CFLAGS += -I$(src)/../common/

obj-m := asguard_core.o

asguard_core-objs := asguard_core.o asguard_core_ctrl.o pm.o net.o proto.o asguard_payload_helper.o
asguard_core-objs += event_logger/event_logger.o event_logger/event_logger_ctrl.o 
asguard_core-objs += pm/asguard_pm.o pm/asguard_pm_ctrl.o
asguard_core-objs += net/asguard_net.o
asguard_core-objs += proto/proto_mgmt.o proto/proto_instance_ctrl.o 
asguard_core-objs += ts/asguard_ts.o ts/asguard_ts_ctrl.o

