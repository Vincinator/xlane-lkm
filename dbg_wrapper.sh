#!/bin/bash

gdb asguard.ko -ex "directory /usr/src/linux-headers-$(eval uname -r)" -ex 'list *(start_follower)'

# test something here

# A little more to write here, mkay!?