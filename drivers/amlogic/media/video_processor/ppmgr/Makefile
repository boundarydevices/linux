#
# Makefile for the amlogic post process manager drivers.
#

ppmgr-objs = ppmgr_drv.o ppmgr_vpp.o
ppmgr3d-objs = ppmgr_3d.o

obj-$(CONFIG_AMLOGIC_POST_PROCESS_MANAGER) += ppmgr.o
obj-$(CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS) += ppmgr3d.o
obj-$(CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS) += ppmgr_3d_tv.o
ccflags-y += -Iinclude/linux/ge2d/