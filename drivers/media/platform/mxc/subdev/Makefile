#Makefile for mxc csi driver

obj-$(CONFIG_VIDEO_MXC_CSI_CAMERA)	+= mx6s_capture.o
obj-$(CONFIG_MXC_VADC) += mxc_vadc.o

ov5640_camera-objs := ov5640.o
obj-$(CONFIG_MXC_CAMERA_SUBDEV_OV5640) += ov5640_camera.o

ov5642_camera-objs := ov5642.o
obj-$(CONFIG_MXC_CAMERA_SUBDEV_OV5642) += ov5642_camera.o
