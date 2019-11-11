#!/usr/bin/env python3



# convert your ssh key to openssl .pem format
# ssh-keygen -f my-rsa-key -m pem -p
# ... and set SSH_PRIV_KEY_PATH env variable to the path of that pem file.

from subprocess import call
import argparse
import os
import multiprocessing
import configparser
import fileinput
import subprocess
from os import environ

from logger import *
from build_host_tasks import *

try:
    import pexpect
    remote_support = True
except ImportError:
    remote_support = False
from subprocess import check_output

show_help = False
build_host_ip = "10.68.235.180"
build_host_user = "dsp"
priv_key_path = os.environ.get('SSH_PRIV_KEY_PATH')

def compile(warnings):
    tasks = [
        "cd asguard-test-src && git pull",
        "export ASGUARD_KERNEL_SRC=~/asguard-kernel-src && cd asguard-test-src && make clean",
    ]

    if warnings:
        tasks.append("export ASGUARD_KERNEL_SRC=~/asguard-kernel-src && cd asguard-test-src && make")
    else:
        tasks.append('export ASGUARD_KERNEL_SRC=~/asguard-kernel-src && cd asguard-test-src && make KBUILD_CFLAGS+="-w" -s')

    for t in tasks:
        cur_task = build_host_task(build_host_ip, build_host_user, priv_key_path, t)
        cur_task.run()


def main():
    setup_logger("uptest.log")
    parser = argparse.ArgumentParser()

    parser.add_argument('--compile', choices=['errors', 'warnings'],
                        help='Compile asguard on build server and print output')
  
    parser.add_argument('--upload', type=str,
                        help='Create a new commit and push to GitHub')

    args = parser.parse_args()

    if priv_key_path is None:
        print_error("SSH_PRIV_KEY_PATH is not set")
        exit(1)

    if args.compile is not None:
        if args.compile == 'errors':
            compile(False)
        if args.compile == 'warnings':
            compile(True)

    if show_help:
        parser.print_help()



if __name__ == '__main__':
    main()


# echo "Commit Message: $*"

# COMMIT_MSG="$*"
# git commit -m "$COMMIT_MSG"
# git push

# ssh dsp@dsp-build 'export ASGUARD_KERNEL_SRC=~/asguard-kernel-src && cd asguard-test-src && git pull && make CFLAGS="-w"'


