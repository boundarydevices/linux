#! /bin/bash

export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

make ARCH=arm meson64_a32_defconfig

make ARCH=arm gxl_p212_1g.dtb
make ARCH=arm gxl_p212_2g.dtb
make ARCH=arm meson8b_m200.dtb
make ARCH=arm meson8b_m400.dtb
make ARCH=arm meson8b_skt.dtb
make ARCH=arm txl_t962_p321.dtb
make ARCH=arm txlx_t962e_r321.dtb
make ARCH=arm txlx_t962x_r311_1g.dtb
make ARCH=arm txlx_t962x_r311_2g.dtb



