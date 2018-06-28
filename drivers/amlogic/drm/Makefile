meson_drv-y += am_meson_drv.o
ccflags-y += -Idrivers/amlogic/media/osd/

ifeq ($(CONFIG_DRM_MESON_USE_ION),y)
	meson_drv-y += am_meson_gem.o am_meson_fb.o
	ccflags-y += -Idrivers/staging/android/
endif

ifeq ($(CONFIG_DRM_MESON_EMULATE_FBDEV),y)
	meson_drv-y += am_meson_fbdev.o
endif

ifneq ($(CONFIG_DRM_MESON_VPU),)
	meson_vpu-y += am_meson_vpu.o
endif

ifneq ($(CONFIG_DRM_MESON_HDMI),)
	meson_hdmi-y += am_meson_hdmi.o am_meson_hdcp.o
endif

ifneq ($(CONFIG_DRM_MESON_PANEL),)
	meson_lcd-y += am_meson_lcd.o
endif

obj-$(CONFIG_DRM_MESON) += meson_drv.o
obj-$(CONFIG_DRM_MESON_VPU) += meson_vpu.o
obj-$(CONFIG_DRM_MESON_HDMI) += meson_hdmi.o
obj-$(CONFIG_DRM_MESON_PANEL) += meson_lcd.o
