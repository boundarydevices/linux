#
# Makefile for the amlogic platform camera interface device drivers.
#
amlflash-objs := common/flashlight.o
amlcamera-objs := common/plat_ctrl.o
#common/util.o
amlvm-objs := common/vm.o
cam_prober-objs := common/cam_prober.o common/config_parser.o
gc2145dri-objs := gc2145.o
gc2145dri_mipi-objs := gc2145_mipi.o
ov5640dri-objs := ov5640.o

obj-y  += amlflash.o
obj-y  += amlvm.o
obj-y  += amlcamera.o
obj-y  += cam_prober.o
obj-$(CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2145) += gc2145dri.o
obj-$(CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2145_MIPI) += gc2145dri_mipi.o
obj-$(CONFIG_AMLOGIC_VIDEO_CAPTURE_OV5640) += ov5640dri.o