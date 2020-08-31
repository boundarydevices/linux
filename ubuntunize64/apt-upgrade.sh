#!/bin/sh
# -*- indent-tabs-mode: t; tab-width: 2 -*-

# accept parameters from the end of the command line
set -e

# don't run flash-kernel
export FLASH_KERNEL_SKIP=1

# clean apt cache
apt-get clean

# update
apt-get update

# fix broken upgrade at trusty
DISTRO=`lsb_release -sc`
if [ "$DISTRO" = "trusty" ] ; then
  apt-get -y install nitrogen-firmware libgdk-pixbuf2.0-0
fi

# dist-upgrade, keeping old configuration files as default
apt-get -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" dist-upgrade

#show me the free disk space
df -h
