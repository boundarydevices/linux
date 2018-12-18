# SPDX-License-Identifier: GPL-2.0
#
# Makefile for industrial I/O accelerometer drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_ADXL345) += adxl345_core.o
obj-$(CONFIG_ADXL345_I2C) += adxl345_i2c.o
obj-$(CONFIG_ADXL345_SPI) += adxl345_spi.o
obj-$(CONFIG_BMA180) += bma180.o
obj-$(CONFIG_BMA220) += bma220_spi.o
obj-$(CONFIG_BMC150_ACCEL) += bmc150-accel-core.o
obj-$(CONFIG_BMC150_ACCEL_I2C) += bmc150-accel-i2c.o
obj-$(CONFIG_BMC150_ACCEL_SPI) += bmc150-accel-spi.o
obj-$(CONFIG_DA280)	+= da280.o
obj-$(CONFIG_DA311)	+= da311.o
obj-$(CONFIG_DMARD06)	+= dmard06.o
obj-$(CONFIG_DMARD09)	+= dmard09.o
obj-$(CONFIG_DMARD10)	+= dmard10.o
obj-$(CONFIG_HID_SENSOR_ACCEL_3D) += hid-sensor-accel-3d.o
obj-$(CONFIG_KXCJK1013) += kxcjk-1013.o
obj-$(CONFIG_KXSD9)	+= kxsd9.o
obj-$(CONFIG_KXSD9_SPI)	+= kxsd9-spi.o
obj-$(CONFIG_KXSD9_I2C)	+= kxsd9-i2c.o
obj-$(CONFIG_MC3230)	+= mc3230.o

obj-$(CONFIG_MMA7455)		+= mma7455_core.o
obj-$(CONFIG_MMA7455_I2C)	+= mma7455_i2c.o
obj-$(CONFIG_MMA7455_SPI)	+= mma7455_spi.o

obj-$(CONFIG_MMA7660)	+= mma7660.o

obj-$(CONFIG_MMA8452)	+= mma8452.o

obj-$(CONFIG_MMA9551_CORE)	+= mma9551_core.o
obj-$(CONFIG_MMA9551)		+= mma9551.o
obj-$(CONFIG_MMA9553)		+= mma9553.o

obj-$(CONFIG_MXC4005)		+= mxc4005.o
obj-$(CONFIG_MXC6255)		+= mxc6255.o

obj-$(CONFIG_SCA3000)		+= sca3000.o

obj-$(CONFIG_STK8312)		+= stk8312.o
obj-$(CONFIG_STK8BA50)		+= stk8ba50.o

obj-$(CONFIG_IIO_SSP_SENSORS_COMMONS) += ssp_accel_sensor.o

obj-$(CONFIG_IIO_ST_ACCEL_3AXIS) += st_accel.o
st_accel-y := st_accel_core.o
st_accel-$(CONFIG_IIO_BUFFER) += st_accel_buffer.o

obj-$(CONFIG_IIO_ST_ACCEL_I2C_3AXIS) += st_accel_i2c.o
obj-$(CONFIG_IIO_ST_ACCEL_SPI_3AXIS) += st_accel_spi.o

obj-$(CONFIG_IIO_ST_LIS2DS12) += lis2ds12.o
obj-$(CONFIG_IIO_ST_LIS2DS12_I2C) += st_lis2ds12_i2c.o
obj-$(CONFIG_IIO_ST_LIS2DS12_SPI) += st_lis2ds12_spi.o

lis2ds12-y += st_lis2ds12_core.o st_lis2ds12_buffer.o st_lis2ds12_trigger.o

obj-$(CONFIG_IIO_ST_LIS2HH12) += lis2hh12.o
obj-$(CONFIG_IIO_ST_LIS2HH12_I2C) += st_lis2hh12_i2c.o
obj-$(CONFIG_IIO_ST_LIS2HH12_SPI) += st_lis2hh12_spi.o

ism303dac_accel-y += st_ism303dac_accel_core.o st_ism303dac_accel_buffer.o st_ism303dac_accel_trigger.o

obj-$(CONFIG_IIO_ST_ISM303DAC_ACCEL) += ism303dac_accel.o
obj-$(CONFIG_IIO_ST_ISM303DAC_ACCEL_I2C) += st_ism303dac_accel_i2c.o
obj-$(CONFIG_IIO_ST_ISM303DAC_ACCEL_SPI) += st_ism303dac_accel_spi.o

lis2hh12-y += st_lis2hh12_core.o st_lis2hh12_buffer.o st_lis2hh12_trigger.o

st_acc33-y := st_acc33_core.o st_acc33_buffer.o
obj-$(CONFIG_IIO_ST_ACC33) += st_acc33.o
obj-$(CONFIG_IIO_ST_ACC33_I2C) += st_acc33_i2c.o
obj-$(CONFIG_IIO_ST_ACC33_SPI) += st_acc33_spi.o

st_lis2dw12-y:= st_lis2dw12_core.o st_lis2dw12_buffer.o
obj-$(CONFIG_IIO_ST_LIS2DW12) += st_lis2dw12.o
obj-$(CONFIG_IIO_ST_LIS2DW12_I2C) += st_lis2dw12_i2c.o
obj-$(CONFIG_IIO_ST_LIS2DW12_SPI) += st_lis2dw12_spi.o

st-lis3dhh-y := st_lis3dhh.o st_lis3dhh_buffer.o
obj-$(CONFIG_IIO_ST_LIS3DHH) += st-lis3dhh.o
