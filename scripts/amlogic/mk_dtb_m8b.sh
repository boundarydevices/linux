#! /bin/bash

export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-


make ARCH=arm meson8b_m200.dtb \
	|| echo "Compile dtb Fail !!"
