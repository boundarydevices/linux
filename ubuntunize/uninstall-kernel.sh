#!/bin/sh
# -*- indent-tabs-mode: t; tab-width: 2 -*-

set -e

# don't run flash-kernel
export FLASH_KERNEL_SKIP=1

# purge old kernel packages
KERNEL_PKG=`dpkg -l | grep -e linux-boundary- -e linux-headers- -e linux-image- | awk '{print $2}'`
if [ "x$KERNEL_PKG" != "x" ] ; then
  apt-get -y purge $KERNEL_PKG
fi

#remove qcacld-module package
QCACLD_PKG=`dpkg -l | grep -e qcacld-module | awk '{print $2}'`
if [ "x$QCACLD_PKG" != "x" ] ; then
  apt-get -y purge $QCACLD_PKG
fi
