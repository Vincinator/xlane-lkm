#!/usr/bin/env bash

ASGARD_LKM_REL_PATH=bin/asgard.ko
ASGARD_DPDK_REL_PATH=bin/runner-dpdk
ASGARD_USERSPACE_REL_PATH=bin/runner

FOUND_KERNEL_MODULE=false
FOUND_DPDK_APP=false
FOUND_USERSPACE_APP=false

ASGARD_KERNEL_MODULE_PKG_NAME=asgard-lkm
ASGARD_DPDK_APP_PKG_NAME=asgard-dpdk
ASGARD_USERSPACE_PKG_NAME=asgard-app

PACKAGE_FOLDER=packages

mkdir -p ${PACKAGE_FOLDER}

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
  tar -czf "$PACKAGE_FOLDER/$ASGARD_KERNEL_MODULE_PKG_NAME.tar.gz" $ASGARD_LKM_REL_PATH
  echo " ... created $PACKAGE_FOLDER/$ASGARD_KERNEL_MODULE_PKG_NAME.tar.gz"

fi

if [ "$FOUND_DPDK_APP" = true ] ; then
  tar -czf "$PACKAGE_FOLDER/$ASGARD_DPDK_APP_PKG_NAME.tar.gz" $ASGARD_DPDK_REL_PATH
  echo " ... created $PACKAGE_FOLDER/$ASGARD_DPDK_APP_PKG_NAME.tar.gz"

fi

if [ "$FOUND_USERSPACE_APP" = true ] ; then
  tar -czf "$PACKAGE_FOLDER/$ASGARD_USERSPACE_PKG_NAME.tar.gz" $ASGARD_USERSPACE_REL_PATH
  echo " ... created $PACKAGE_FOLDER/$ASGARD_USERSPACE_PKG_NAME.tar.gz"

fi