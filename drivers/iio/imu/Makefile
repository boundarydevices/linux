# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Inertial Measurement Units
#

# When adding new entries keep the list in alphabetical order
adis16400-y             := adis16400_core.o
adis16400-$(CONFIG_IIO_BUFFER) += adis16400_buffer.o
obj-$(CONFIG_ADIS16400) += adis16400.o
obj-$(CONFIG_ADIS16480) += adis16480.o

adis_lib-y += adis.o
adis_lib-$(CONFIG_IIO_ADIS_LIB_BUFFER) += adis_trigger.o
adis_lib-$(CONFIG_IIO_ADIS_LIB_BUFFER) += adis_buffer.o
obj-$(CONFIG_IIO_ADIS_LIB) += adis_lib.o

obj-y += bmi160/
obj-y += inv_mpu6050/

obj-$(CONFIG_KMX61) += kmx61.o

obj-y += st_lsm6dsx/
obj-y += st_lsm6dsm/
obj-y += st_asm330lhh/
obj-y += st_lsm6dso/
obj-y += st_lsm6ds3/
obj-y += st_lsm6ds3h/
obj-y += st_ism330dlc/
obj-y += st_imu68/
obj-y += st_lsm6dsr/
