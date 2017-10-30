meson-y := meson_drv.o

ifeq ($(CONFIG_DRM_MESON_BYPASS_MODE),y)
	meson-y += am_meson_plane.o am_meson_crtc.o am_meson_hdmi.o
	ccflags-y += -Idrivers/amlogic/media/osd/
else
	meson-y += meson_viu.o meson_vpp.o meson_venc.o meson_vclk.o meson_canvas.o meson_plane.o meson_crtc.o meson_venc_cvbs.o
endif

ifeq ($(CONFIG_DRM_MESON_USE_ION),y)
	meson-y += am_meson_gem.o am_meson_fb.o
	ccflags-y += -Idrivers/staging/android/
endif

ifeq ($(CONFIG_DRM_MESON_EMULATE_FBDEV),y)
        meson-y += am_meson_fbdev.o
endif

obj-$(CONFIG_DRM_MESON) += meson.o
