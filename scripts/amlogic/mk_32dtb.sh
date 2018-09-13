#! /bin/bash

export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

make ARCH=arm meson64_a32_defconfig
make ARCH=arm txl_t962_p321.dtb || echo "Compile dtb Fail !!"
make ARCH=arm txlx_t962x_r311_1g.dtb || echo "Compile dtb Fail !!"



