FW_SRC := src/fw_lib/acamera_gdc.c \
	src/platform/system_gdc_io.c \
	app/gdc_main.c \
	app/gdc_module.c \
	app/gdc_dmabuf.c

FW_SRC_OBJ := $(FW_SRC:.c=.o)

obj-$(CONFIG_AMLOGIC_MEDIA_GDC) += $(FW_SRC_OBJ)

PWD=$(src)
ccflags-y +=-I$(PWD)/app -I$(PWD)/inc
ccflags-y +=-I$(PWD)/inc/api -I$(PWD)/inc/gdc -I$(PWD)/inc/sys
ccflags-y +=-I$(PWD)/src/platform -I$(PWD)/src/fw_lib
ccflags-y += -Idrivers/amlogic/media/common/ion_dev/
ccflags-y += -Idrivers/staging/android/

ccflags-y += -Wno-declaration-after-statement
