# SPDX-License-Identifier: GPL-2.0
#
# Makefile for industrial I/O pressure drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_ABP060MG) += abp060mg.o
obj-$(CONFIG_BMP280) += bmp280.o
bmp280-objs := bmp280-core.o bmp280-regmap.o
obj-$(CONFIG_BMP280_I2C) += bmp280-i2c.o
obj-$(CONFIG_BMP280_SPI) += bmp280-spi.o
obj-$(CONFIG_IIO_CROS_EC_BARO) += cros_ec_baro.o
obj-$(CONFIG_HID_SENSOR_PRESS)   += hid-sensor-press.o
obj-$(CONFIG_HP03) += hp03.o
obj-$(CONFIG_MPL115) += mpl115.o
obj-$(CONFIG_MPL115_I2C) += mpl115_i2c.o
obj-$(CONFIG_MPL115_SPI) += mpl115_spi.o
obj-$(CONFIG_MPL3115) += mpl3115.o
obj-$(CONFIG_MS5611) += ms5611_core.o
obj-$(CONFIG_MS5611_I2C) += ms5611_i2c.o
obj-$(CONFIG_MS5611_SPI) += ms5611_spi.o
obj-$(CONFIG_MS5637) += ms5637.o
obj-$(CONFIG_IIO_ST_PRESS) += st_pressure.o
st_pressure-y := st_pressure_core.o
st_pressure-$(CONFIG_IIO_BUFFER) += st_pressure_buffer.o
obj-$(CONFIG_T5403) += t5403.o
obj-$(CONFIG_HP206C) += hp206c.o
obj-$(CONFIG_ZPA2326) += zpa2326.o
obj-$(CONFIG_ZPA2326_I2C) += zpa2326_i2c.o
obj-$(CONFIG_ZPA2326_SPI) += zpa2326_spi.o

obj-$(CONFIG_IIO_ST_PRESS_I2C) += st_pressure_i2c.o
obj-$(CONFIG_IIO_ST_PRESS_SPI) += st_pressure_spi.o

obj-$(CONFIG_ST_LPS22HB_IIO) += st_lps22hb.o
obj-$(CONFIG_ST_LPS22HB_I2C_IIO) += st_lps22hb_i2c.o
obj-$(CONFIG_ST_LPS22HB_SPI_IIO) += st_lps22hb_spi.o

st_lps22hb-y += st_lps22hb_core.o st_lps22hb_buffer.o

obj-$(CONFIG_ST_LPS33HW_IIO) += st_lps33hw.o
obj-$(CONFIG_ST_LPS33HW_I2C_IIO) += st_lps33hw_i2c.o
obj-$(CONFIG_ST_LPS33HW_SPI_IIO) += st_lps33hw_spi.o

st_lps33hw-y += st_lps33hw_core.o st_lps33hw_buffer.o

obj-$(CONFIG_ST_LPS22HH_IIO) += st_lps22hh.o
obj-$(CONFIG_ST_LPS22HH_I2C_IIO) += st_lps22hh_i2c.o
obj-$(CONFIG_ST_LPS22HH_SPI_IIO) += st_lps22hh_spi.o

st_lps22hh-y += st_lps22hh_core.o st_lps22hh_buffer.o
