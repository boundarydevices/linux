#! /bin/bash

export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-


make ARCH=arm meson8b_m200.dtb \
	|| echo "Compile dtb Fail !!"


make ARCH=arm meson8b_m400.dtb \
	|| echo "Compile dtb Fail !!"

make ARCH=arm meson8b_skt.dtb \
	|| echo "Compile dtb Fail !!"
