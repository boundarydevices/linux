#!/bin/bash

WORKDIR=~/kernel-post

if [ ! -d ${WORKDIR} ] ; then mkdir -p ${WORKDIR} ; fi

cd ${WORKDIR}

DISTRO=`lsb_release -sc`

STAGE1=y
if [ "$DISTRO" = "trusty" ] ; then
  STAGE2=n
  STAGE3=n
else
  STAGE2=y
  STAGE3=y
fi
STAGE4=y


export KERNEL_SRC=$PWD/linux-imx6
export INSTALL_MOD_PATH=$KERNEL_SRC/ubuntunize/linux-staging
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

if [ "${STAGE1}" = "y" ] ; then
   cd ${WORKDIR}
   if [ ! -d linux-imx6 ] ; then
      git clone https://github.com/boundarydevices/linux-imx6.git && cd linux-imx6

#      git checkout boundary-imx_3.14.28_1.0.0_ga
#      git checkout boundary-imx_3.14.52_1.1.0_ga
      git checkout boundary-imx_4.1.15_2.0.0_ga
   
      make -C ubuntunize prerequisites
      make boundary_defconfig
#     make your customization, code or configuration changes here
      echo "make your customization, code or configuration changes, then run this script again" ;
      exit 1
   fi
   make zImage modules dtbs -j8
   make -C ubuntunize tarball
fi



if [ "${STAGE2}" = "y" ] ; then
   cd ${WORKDIR}
   git clone https://github.com/Freescale/kernel-module-imx-gpu-viv.git && cd kernel-module-imx-gpu-viv
#   git checkout upstream/5.0.11.p7.4
   git checkout upstream/5.0.11.p8.6
   if [ "${DISTRO}" = "stretch" ] ; then
# only for gcc 6+
      sed 's,-Werror,-Werror -Wno-error=misleading-indentation,g' -i ./kernel-module-imx-gpu-viv-src/Kbuild
   fi
   echo KERNEL_SRC=$KERNEL_SRC
   make -j8
   make modules_install
fi


if [ "${STAGE3}" = "y" ] ; then
   cd ${WORKDIR}
   git clone https://github.com/boundarydevices/qcacld-2.0.git && cd qcacld-2.0
   export CONFIG_CLD_HL_SDIO_CORE=y
   git checkout boundary-LNX.LEH.4.2.2.2-4.5.20.034
   make -j8
   make modules_install
fi


if [ "${STAGE4}" = "y" ] ; then
   cd ${WORKDIR}
# xenial
#  export ROOTFS_IMG=~/Downloads/20160913-nitrogen-4.1.15_1.2.0_ga-xenial-en_US-mate_armhf.img
# jessie
#  export ROOTFS_IMG=~/Downloads/20161207-nitrogen-4.1.15_2.0.0_ga-jessie-en_US-xfce_armhf.img
# trusty
#  export ROOTFS_IMG=~/Downloads/20160213-nitrogen-3.14.28_1.0.0_ga-trusty-en_US-lxde_armhf.img
# stretch
  export ROOTFS_IMG=~/Downloads/20170323-nitrogen-4.1.15_2.0.0_ga-stretch-en_US-devcon_armhf.img
  rm -f ${ROOTFS_IMG}
  make -C ${KERNEL_SRC}/ubuntunize -f Merge.mk
fi
