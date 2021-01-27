#!/usr/bin/env bash

ASGARD_LKM_REL_PATH=bin/asgard.ko
ASGARD_DPDK_REL_PATH=bin/testrunner-dpdk
ASGARD_USERSPACE_REL_PATH=bin/testrunner

FOUND_KERNEL_MODULE=false
FOUND_DPDK_APP=false
FOUND_USERSPACE_APP=false


mkdir -p packages

echo "Checking if binaries are present:"
if [[ -f "$ASGARD_LKM_REL_PATH" ]]; then
    FOUND_KERNEL_MODULE=true
    echo " ... Found $ASGARD_LKM_REL_PATH"
fi

if [[ -f "$ASGARD_DPDK_REL_PATH" ]]; then
    FOUND_DPDK_APP=true
    echo " ... Found $ASGARD_DPDK_REL_PATH"
fi

if [[ -f "$ASGARD_USERSPACE_REL_PATH" ]]; then
    FOUND_USERSPACE_APP=true
    echo " ... Found $ASGARD_USERSPACE_REL_PATH"
fi


echo "Packaging archives for found binaries:"
if [ "$FOUND_KERNEL_MODULE" = true ] ; then
  echo ""
fi

if [ "$FOUND_DPDK_APP" = true ] ; then
  echo ""
fi

if [ "$FOUND_USERSPACE_APP" = true ] ; then
    echo ""
fi