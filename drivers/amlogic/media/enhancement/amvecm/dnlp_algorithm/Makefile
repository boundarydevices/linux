CONFIG_AMLOGIC_DNLP_ALG=m
obj-$(CONFIG_AMLOGIC_DNLP_ALG) += dnlp_alg.o

dnlp-objs = dnlp_alg.o

TARGET_PRODUCT=t962x_r311
KERNEL_ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
PRODUCT_OUT=./../../../../../../../out/target/product/$(TARGET_PRODUCT)
TARGET_PATCH=$(shell pwd)

ccflags-y  += -Idrivers/amlogic/media/enhancement/amvecm/

all:
	rm -rf *.o .*.o.d *.order *.symvers *.mod.* .*.o.cmd .*.ko.cmd *.tmp_versions *.ko
	make -C $(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(TARGET_PATCH) ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	rm -rf *.o .*.o.d *.order *.symvers *.mod.* .*.o.cmd .*.ko.cmd *.tmp_versions/
