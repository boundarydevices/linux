obj-$(CONFIG_AMLOGIC_ATV_DEMOD) += aml_atvdemod.o

aml_atvdemod-objs = atvdemod_func.o atvauddemod_func.o atv_demod_v4l2.o atv_demod_driver.o atv_demod_ops.o

ccflags-y += -I.
ccflags-y += -Idrivers/media/dvb-core
#ccflags-y += -Idrivers/amlogic/tuners/mxl661
#ccflags-y += -Idrivers/amlogic/tuners/si2151

#obj-m += aml_atvdemod.o
#
#aml_atvdemod-objs = atvdemod_func.o atvauddemod_func.o
#
#aml_atvdemod-objs += atv_demod_v4l2.o atv_demod_driver.o atv_demod_ops.o
#
#ccflags-y += -I.
#ccflags-y += -Idrivers/media/dvb-core
#
#TARGET_PRODUCT=t962e_r321
#KERNEL_ARCH=arm64
#CROSS_COMPILE=aarch64-linux-gnu-
#PRODUCT_OUT=../../../../out/target/product/$(TARGET_PRODUCT)/obj/KERNEL_OBJ
#
#KBUILD_EXTRA_SYMBOLS += $(shell pwd)/../tuner/si2151_4.9/Module.symvers
#
#export KBUILD_EXTRA_SYMBOLS
#
#all:
#	$(MAKE) -C $(PRODUCT_OUT) M=$(shell pwd) ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
#
#clean:
#	rm -rf *.ko .*.ko.cmd *.o *.order *.symvers -f *.cmd .*.o.cmd *.mod.c .tmp_versions/