#
# Makefile for eFuse.
#

ifneq ($(CONFIG_ARM64),y)
obj-$(CONFIG_AMLOGIC_EFUSE) += efuse_version.o efuse_hw.o efuse.o
else
obj-$(CONFIG_AMLOGIC_EFUSE) += efuse_hw64.o efuse64.o
endif