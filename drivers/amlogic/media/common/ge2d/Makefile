ge2d-objs = bitblt.o \
	fillrect.o \
	ge2d_hw.o \
	ge2dgen.o \
	ge2d_wq.o \
	stretchblt.o \
	ge2d_main.o \
	blend.o \
	ge2d_dmabuf.o

obj-$(CONFIG_AMLOGIC_MEDIA_GE2D) += ge2d.o
ccflags-y += -Iinclude/linux/media/ge2d/
ccflags-y += -Idrivers/amlogic/media/osd/
ccflags-y += -I$(srctree)/drivers/staging/android
ccflags-y += -Idrivers/amlogic/media/common/ion_dev/
