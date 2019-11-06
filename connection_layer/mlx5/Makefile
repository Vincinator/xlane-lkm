EXTRA_CFLAGS+=-I$(src)/../../common/

obj-m := asguard_mlx5.o
asguard_mlx5-objs := mlx5_con.o

all:
	make -C $(ASGUARD_KERNEL_SRC) M=$(PWD) modules

clean:
	make -C /home/dspmeisten/asguard-kernel-src M=$(PWD) clean
