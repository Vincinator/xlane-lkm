#/bin/bash

cd $TARGET_BIN/modules

sudo cp *.ko /lib/modules/`uname -r`/kernel/drivers/net/

sudo depmod
sudo modprobe asguard.ko -f



