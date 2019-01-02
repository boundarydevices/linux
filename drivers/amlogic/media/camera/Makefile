#
# Makefile for the amlogic platform camera interface device drivers.
#
amlflash-objs := common/flashlight.o
amlcamera-objs := common/plat_ctrl.o
#common/util.o
amlvm-objs := common/vm.o
cam_prober-objs := common/cam_prober.o common/config_parser.o
gc2145dri-objs := gc2145.o

obj-y  += amlflash.o
obj-y  += amlvm.o
obj-y  += amlcamera.o
obj-y  += cam_prober.o
obj-$(CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2145) += gc2145dri.o
