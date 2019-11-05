EXTRA_CFLAGS += -I$(src)/../common/


obj-m := asguard.o

asguard-objs := asguard_core.o asguard_core_ctrl.o 
asguard-objs += event_logger/event_logger.o event_logger/event_logger_ctrl.o 
asguard-objs += pm/asguard_pm.o pm/asguard_pm_ctrl.o
asguard-objs += net/asguard_net.o
asguard-objs += proto/proto_mgmt.o proto/proto_instance_ctrl.o  
asguard-objs += ts/asguard_ts.o ts/asguard_ts_ctrl.o asguard_payload_helper.o


all:
	make -C /home/dsp/asguard-kernel-src M=$(PWD) modules

clean:
	make -C /home/dsp/asguard-kernel-src M=$(PWD) clean
