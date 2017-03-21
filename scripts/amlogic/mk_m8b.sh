#! /bin/bash

export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

make ARCH=arm meson32_defconfig


make  ARCH=arm  UIMAGE_LOADADDR=0x12000000  uImage -j20 \
	|| echo "Compile uImage Fail !!"


./scripts/amlogic/mk_dtb_m8b.sh

