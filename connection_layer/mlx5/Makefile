EXTRA_CFLAGS+=-I$(src)/../../common/

obj-m := asguard_mlx5.o
asguardmlx5-objs := mlx5_con.o

all:
	make -C /home/dsp/asguard-kernel-src M=$(PWD) modules

clean:
	make -C /home/dsp/asguard-kernel-src M=$(PWD) clean
