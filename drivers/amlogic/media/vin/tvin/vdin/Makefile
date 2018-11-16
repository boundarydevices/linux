#
# Makefile for Vdin.
#
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y := -DDEBUG_SUPPORT
else
ccflags-y := -DDEBUG
endif
obj-$(CONFIG_AMLOGIC_MEDIA_VDIN) = tvin_vdin.o
tvin_vdin-objs += vdin_v4l2.o
tvin_vdin-objs += vdin_vf.o
tvin_vdin-objs += vdin_debug.o
tvin_vdin-objs += vdin_drv.o
tvin_vdin-objs += vdin_ctl.o
tvin_vdin-objs += vdin_sm.o
tvin_vdin-objs += vdin_canvas.o
tvin_vdin-objs += vdin_afbce.o

