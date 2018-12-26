#
# Makefile for evs driver
#

ccflags-y := -std=gnu99 -Wno-declaration-after-statement -DPB_SYSTEM_HEADER=\"pb_system.h\" -I$(srctree)/../../../external/nanopb-c/

obj-y += ../../../../../../external/nanopb-c/

obj-$(CONFIG_VEHICLE_RPMSG) += \
	rpmsg_vehicle.o \
	vehicle_protocol_callback.o \
	vehiclehalproto.pb.o
