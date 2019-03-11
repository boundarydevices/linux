#! /bin/bash

export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-

make ARCH=arm64 gxm_q200_2g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxm_q201_2g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxm_skt.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxl_p212_2g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxl_p212_1g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxl_sei210_2g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxl_sei210_1g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxl_p400_2g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxl_p401_2g.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 gxl_skt.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 axg_pxp.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 axg_a113d_skt.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 axg_s400.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 axg_s420.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 axg_s400_v03.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 axg_s420_v03.dtb || echo "Compile dtb Fail !!"

make ARCH=arm64 g12a_pxp.dtb || echo "Compile dtb Fail!!"

make ARCH=arm64 sm1_pxp.dtb || echo "Compile dtb Fail!!"

make ARCH=arm64 g12a_s905d2_skt.dtb || echo "Compile dtb Fail!!"

make ARCH=arm64 g12b_pxp.dtb || echo "Compile dtb Fail!!"

make ARCH=arm64 g12b_a311d_skt.dtb || echo "Compile dtb Fail!!"

make ARCH=arm64 g12b_a311d_w400.dtb || echo "Compile dtb Fail!!"
