#
# Makefile for the amlogic video device interface device drivers.
#
asflags-y =-mfloat-abi=softfp -mfpu=neon
amlvideodri-objs := amlvideo.o
amlcm-objs := common/vfutil.o
amlvideo2dri-objs := amlvideo2.o

obj-$(CONFIG_AMLOGIC_V4L_VIDEO) +=amlcm.o
obj-$(CONFIG_AMLOGIC_V4L_VIDEO) +=amlvideodri.o
obj-$(CONFIG_AMLOGIC_V4L_VIDEO2) +=amlvideo2dri.o

