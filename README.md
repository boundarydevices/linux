Index
=======
	* Introduction
	* Integration details
	* More information
	* Copyright


Introduction
==============
This repository contains Linux kernel v4.14 with STMicroelectronics MEMS sensor support. STM sensor drivers are located under the directory [drivers/iio](https://github.com/STMicroelectronics/STMems_Linux_IIO_drivers/tree/linux-4.14.y-gh/drivers/iio)  organized by sensor type:

### Inertial Module Unit (IMU):

> LSM6DS3, LSM6DS3H, LSM6DSL, LSM6DSM, LSM9DS0, LSM9DS1, ISM330DLC, LSM6DSO

### Accelerometer:

> LIS2DH, LIS2DH12, LIS3DH, LIS2DG, LSM303AH, LIS2DS12, LIS2HH12, LIS2DW12, LIS3LV02DL,
> LSM303DLH, LSM303DLHC, LSM330D, LSM330DL, LSM330DLC, LIS331DL, LIS331DLH, LSM303DL,
> LSM303DLM, LSM330, LSM303AGR, LIS3DHH, IIS2DH, ISM303DAC, IIS3DHHC

### Gyroscope:

> L3G4200D, LSM330D, LSM330DL, L3GD20, L3GD20H, L3G4IS, LSM330, LSM330DLC

### Magnetometer:

> LIS3MDL, LSM9DS1, LSM303AH, LSM303AGR, LSM303DLH, LSM303DLHC, LSM303DLM, LIS2MDL, IIS2MDC, ISM303DAC

### Humidity:

> HTS221

### Pressure:

> LPS22HB, LPS22HD, LPS25H, LPS331AP, LPS001WP, LPS33HW, LPS35HW, LPS22HH



Data collected by STM sensors are pushed to userland through the kernel buffers of Linux IIO framework. User space applications can get sensor events by reading the related IIO devices created in the /dev directory (*/dev/iio{x}*). Please see [IIO][1] for more information.

All STM MEMS sensors support *I2C/SPI* digital interface. Please refer to [I2C][2] and [SPI][3] for detailed documentation.


Integration details
=====================

In order to explain how to integrate STM sensors in a different kernel, please consider the following *LSM6DSM* IMU example

### Source code integration

> * Copy driver source code into the target directory (e.g. *drivers/iio/imu*)
> * Edit related Kconfig (e.g. *drivers/iio/imu/Kconfig*) to include *LSM6DSM* support:

>         source "drivers/iio/imu/st_lsm6dsm/Kconfig"

> * Edit related Makefile (e.g. *drivers/iio/imu/Makefile*) adding the following line:

>         obj-y += lsm6dsm/

### Device Tree configuration

> To enable driver probing, add the lsm6dsm node to the platform device tree as described below.

> **Required properties:**

> *- compatible*: "st,lsm6dsm"

> *- reg*: the I2C address or SPI chip select the device will respond to

> *- interrupt-parent*: phandle to the parent interrupt controller as documented in [interrupts][4]

> *- interrupts*: interrupt mapping for IRQ as documented in [interrupts][4]
> 
>**Recommended properties for SPI bus usage:**

> *- spi-max-frequency*: maximum SPI bus frequency as documented in [SPI][3]
> 
> **Optional properties:**

> *- st,drdy-int-pin*: MEMS sensor interrupt line to use (default 1)

> I2C example (based on Raspberry PI 3):

>		&i2c0 {
>			status = "ok";
>			#address-cells = <0x1>;
>			#size-cells = <0x0>;
>			lsm6dsm@6b {
>				compatible = "st,lsm6dsm";
>				reg = <0x6b>;
>				interrupt-parent = <&gpio>;
>				interrupts = <26 IRQ_TYPE_EDGE_RISING>;
>		};

> SPI example (based on Raspberry PI 3):

>		&spi0 {
>			status = "ok";
>			#address-cells = <0x1>;
>			#size-cells = <0x0>;
>			lsm6dsm@0 {
>				spi-max-frequency = <500000>;
>				compatible = "st,lsm6dsm";
>				reg = <0>;
>				interrupt-parent = <&gpio>;
>				interrupts = <26 IRQ_TYPE_EDGE_RISING>;
>			};


### Kernel configuration

Configure kernel with *make menuconfig* (alternatively use *make xconfig* or *make qconfig*)
 
>		Device Drivers  --->
>			<M> Industrial I/O support  --->
>				Inertial measurement units  --->
>				<M>   STMicroelectronics LSM6DSM/LSM6DSL sensor  --->


More Information
=================
[http://st.com](http://st.com)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/input)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bings/interrupt-controller/interrupts.txt](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/interrupt-controller/interrupts.txt)


Copyright
===========
Copyright (C) 2017 STMicroelectronics

This software is distributed under the GNU General Public License - see the accompanying COPYING file for more details.

[1]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio/iio_configfs.txt "IIO"
[2]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c "I2C"
[3]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi "SPI"
[4]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/interrupt-controller/interrupts.txt "interrupts"
