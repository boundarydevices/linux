##########################################
########## Amlogic Drivers ###############
##########################################

##########################################
## The order of directories matter.
## Do not change.
##########################################

KBUILD_CFLAGS += -Wlarger-than=28792
KBUILD_CFLAGS += -Wstack-usage=1856

# These 2 marked sentence is just for generate warning messages
#KBUILD_CFLAGS += -Wno-error=larger-than=28792
#KBUILD_CFLAGS += -Wno-error=stack-usage=1856

obj-$(CONFIG_CPU_FREQ)		+= cpufreq/

obj-$(CONFIG_AMLOGIC_UART)	+= uart/

obj-$(CONFIG_AMLOGIC_PINCTRL)	+= pinctrl/

obj-$(CONFIG_AMLOGIC_IOMAP)	+= iomap/

obj-$(CONFIG_AMLOGIC_SEC)               += secmon/
obj-$(CONFIG_AMLOGIC_USB)		+= usb/

obj-$(CONFIG_AMLOGIC_CPU_VERSION)       += cpu_version/

obj-$(CONFIG_AMLOGIC_I2C)		+= i2c/

obj-$(CONFIG_AMLOGIC_SPICC_MASTER) += spicc/

obj-$(CONFIG_AMLOGIC_CPU_INFO)          += cpu_info/

obj-$(CONFIG_AMLOGIC_MHU_MBOX)		+= mailbox/

obj-$(CONFIG_AMLOGIC_REG_ACCESS)	+= reg_access/

obj-$(CONFIG_AMLOGIC_TIMER)		+= clocksource/

obj-$(CONFIG_AMLOGIC_CLK)		+= clk/

obj-$(CONFIG_AMLOGIC_CRYPTO)		+= crypto/

obj-$(CONFIG_AMLOGIC_INPUT)		+= input/

obj-$(CONFIG_AMLOGIC_EFUSE)             += efuse/

obj-$(CONFIG_AMLOGIC_REBOOT)		+= reboot/

obj-$(CONFIG_AMLOGIC_INTERNAL_PHY)	+= ethernet/phy/

obj-$(CONFIG_AMLOGIC_CPU_HOTPLUG)	+= cpu_hotplug/

obj-$(CONFIG_AMLOGIC_PWM)	+= pwm/

obj-$(CONFIG_AMLOGIC_MEDIA_ENABLE)	+=	media/

obj-$(CONFIG_AMLOGIC_MMC)             += mmc/

obj-$(CONFIG_AMLOGIC_NAND)	+= mtd/

obj-$(CONFIG_AMLOGIC_M8B_NAND)	+= mtd_meson8b/

obj-$(CONFIG_AMLOGIC_VRTC)		+= vrtc/

obj-$(CONFIG_AMLOGIC_SMARTCARD)		+= smartcard/

obj-$(CONFIG_AMLOGIC_CEC)		+= cec/

obj-$(CONFIG_AMLOGIC_UNIFYKEY)    += unifykey/

obj-$(CONFIG_AMLOGIC_TEMP_SENSOR)	+= thermal/

obj-$(CONFIG_AMLOGIC_AUDIO_DSP)	+= audiodsp/

obj-$(CONFIG_AMLOGIC_AMAUDIO) += amaudio/

obj-$(CONFIG_AMLOGIC_AMAUDIO2) += amaudio2/

obj-$(CONFIG_AMLOGIC_AUDIO_INFO) += audioinfo/

obj-$(CONFIG_AMLOGIC_SUSPEND)	+= pm/

obj-$(CONFIG_AMLOGIC_LED)		+= led/

obj-$(CONFIG_AMLOGIC_JTAG)		+= jtag/

obj-$(CONFIG_AMLOGIC_WDT)		+= watchdog/

obj-$(CONFIG_AMLOGIC_ESM) += esm/

obj-$(CONFIG_AMLOGIC_BT_DEVICE)		+= bluetooth/

obj-$(CONFIG_AMLOGIC_WIFI)      += wifi/

obj-$(CONFIG_AMLOGIC_POWER)		+= power/

obj-$(CONFIG_AMLOGIC_PCIE)      += pci/

obj-$(CONFIG_AMLOGIC_IRBLASTER) += irblaster/

obj-$(CONFIG_AMLOGIC_IIO)       += iio/

obj-$(CONFIG_AMLOGIC_DDR_TOOL)       += ddr_tool/

obj-$(CONFIG_DRM_MESON)	+= drm/

obj-$(CONFIG_AMLOGIC_M8B_SM) += secure_monitor/

obj-$(CONFIG_AMLOGIC_TEE) += tee/

obj-$(CONFIG_AMLOGIC_MEMORY_EXTEND) += memory_ext/

obj-$(CONFIG_AMLOGIC_LEDRING) += ledring/

obj-$(CONFIG_AMLOGIC_GPIO_IRQ) += irqchip/

obj-$(CONFIG_AMLOGIC_ATV_DEMOD) += atv_demod/
