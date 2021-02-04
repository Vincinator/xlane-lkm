#!/usr/bin/env bash

SRC_DIR="src/"

lkm=0
plain=0
dpdk=0

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -l|--lkm) lkm=1;;
        -p|--plain) plain=1;;
        -d|--dpdk) dpdk=1 ;;
        -k|--kerneldir) kerneldir="$2"; shift ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

if [ -z ${kerneldir} ] && [ ${lkm} -eq 1 ]; then
  echo "[ASGARD build.sh] the kerneldir path variable is not set"
fi

# export kerneldir=/home/vincent/workspace/asgard-kernel

build_lkm() {
  echo "[ASGARD build.sh] Starting Build for loadable linux kernel asgard version"
  echo "[ASGARD build.sh] kerneldir is set to '$kerneldir'"

  cd $SRC_DIR || (echo "Directory $SRC_DIR not found" && exit)

  export ASGARD_DPDK=0
  export ASGARD_KERNEL_MODULE=1
  cmake . -G "Unix Makefiles"
  cmake --build .  --target module
}

build_dpdk() {
  echo "[ASGARD BUILD] Starting Build for dpdk asgard version"

  cd $SRC_DIR || (echo "Directory $SRC_DIR not found" && exit)

  export ASGARD_DPDK=1
  export ASGARD_KERNEL_MODULE=0
  cmake . -G "Unix Makefiles"
  cmake --build .  --target runner-dpdk
}

build_plain() {
  echo "[ASGARD BUILD] Starting Build for plain asgard version"

  cd $SRC_DIR || (echo "Directory $SRC_DIR not found" && exit)

  export ASGARD_DPDK=0
  export ASGARD_KERNEL_MODULE=0
  cmake . -G "Unix Makefiles"
  cmake --build .  --target runner-plain
}


if [ ${lkm} -eq 1 ]; then
  build_lkm
fi

if [ ${dpdk} -eq 1 ]; then
  build_dpdk
fi

if [ ${plain} -eq 1 ]; then
  build_plain
fi;