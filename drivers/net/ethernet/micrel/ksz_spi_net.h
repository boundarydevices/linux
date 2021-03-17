/**
 * Microchip SPI switch common header
 *
 * Copyright (c) 2015-2019 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2012-2015 Micrel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef KSZ_SPI_NET_H
#define KSZ_SPI_NET_H


#if defined(_LINUX_I2C_H)
/**
 * struct i2c_hw_priv - I2C device private data structure
 * @i2cdev:		Adapter device information.
 * @rxd:		Buffer for receiving I2C data.
 * @txd:		Buffer for transmitting I2C data.
 */
struct i2c_hw_priv {
	struct i2c_client *i2cdev;

	u8 rxd[8];
	u8 txd[32];
};
#endif

#if defined(__LINUX_SPI_H)
/**
 * struct spi_hw_priv - SPI device private data structure
 * @spidev:		Adapter device information.
 * @spi_msg1:		Used for SPI transfer with one message.
 * @spi_msg2:		Used for SPI transfer with two messages.
 * @spi_xfer1:		Used for SPI transfer with one message.
 * @spi_xfer2:		Used for SPI transfer with two messages.
 * @rx_1msg:		Flag to receive SPI data with single message.
 * @rxd:		Buffer for receiving SPI data.
 * @txd:		Buffer for transmitting SPI data.
 */
struct spi_hw_priv {
	struct spi_device *spidev;
	struct spi_message spi_msg1;
	struct spi_message spi_msg2;
	struct spi_transfer spi_xfer1;
	struct spi_transfer spi_xfer2[2];
	int rx_1msg;

	u8 rxd[128];
	u8 txd[128];
};
#endif

struct smi_hw_priv {
	struct mii_bus *bus;
	int phyid;
	int (*read)(struct mii_bus *bus, int phy_id, int regnum);
	int (*write)(struct mii_bus *bus, int phy_id, int regnum, u16 val);
};

/**
 * struct sw_priv - Switch device private data structure
 * @hw_dev:		Pointer to hardware access device structure.
 * @dev:		Pointer to Linux base device of hardware device.
 * @intr_mode:		Indicate which interrupt mode to use.
 * @irq:		A copy of the hardware device interrupt.
 * @sysfs:		Sysfs structure.
 * @proc_sem:		Semaphore for sysfs accessing.
 * @hwlock:
 * @lock:
 * @link_read:		Work queue for detecting link.
 * @mib_read:		Work queue for reading MIB counters.
 * @stp_monitor:	Work queue for STP monitoring.
 * @mib_timer_info:	Timer information for reading MIB counters.
 * @monitor_timer_info:	Timer information for monitoring.
 * @counter:		MIB counter data.
 * @ports:		Virtual switch ports.
 * @debug_root:
 * @debug_file:
 * @irq_gpio:		GPIO pin used for interrupt.
 * @gpio_val:		GPIO value during interrupt.
 * @phy_id:		Point to active PHY.
 * @intr_working:	Working interrupt indications.
 * @intr_mask:
 * @pdev:		Point to platform device.
 * @bus:		Point to MDIO bus.
 * @bus_irqs:
 * @name:
 * @phydev:		Point to active PHY device.
 * @sw:			Virtual switch structure.
 */
struct sw_priv {
	void *hw_dev;
	struct device *dev;
	int intr_mode;
	int irq;

	struct ksz_sw_sysfs sysfs;
#ifdef CONFIG_1588_PTP
	struct ksz_ptp_sysfs ptp_sysfs;
#endif
	struct semaphore proc_sem;

	struct mutex hwlock;
	struct mutex lock;

	struct work_struct irq_work;
	struct delayed_work link_read;
	struct work_struct mib_read;
	struct delayed_work stp_monitor;
	struct ksz_timer_info mib_timer_info;
	struct ksz_timer_info monitor_timer_info;
	struct ksz_counter_info counter[TOTAL_PORT_NUM];
	struct ksz_port ports[TOTAL_PORT_NUM + 1];

	struct dentry *debug_root;
	struct dentry *debug_file;

	int irq_gpio;
	int gpio_val;
	int intr_working;
	uint intr_mask;

	struct platform_device *pdev;
	struct mii_bus *bus;
	int bus_irqs[PHY_MAX_ADDR];
	char name[40];
	struct phy_device *phydev;

	/* Switch structure size can be variable. */
	struct ksz_sw sw;
};

/**
 * struct dev_priv - Network device private data structure
 * @adapter:		Adapter device information.
 * @dev:
 * @parent:
 * @port:
 * @monitor_timer_info:	Timer information for monitoring.
 * @stats:		Network statistics.
 * @phydev:		The PHY device associated with the device.
 * @phy_pause:		Workqueue to pause the PHY state machine.
 * @id:			Device ID.
 * @mii_if:		MII interface information.
 * @advertising:	Temporary variable to store advertised settings.
 * @msg_enable:		The message flags controlling driver output.
 * @media_state:	The connection status of the device.
 * @multicast:		The all multicast state of the device.
 * @promiscuous:	The promiscuous state of the device.
 */
struct dev_priv {
	void *adapter;
	struct net_device *dev;
	void *parent;
	struct ksz_port port;
	struct ksz_timer_info monitor_timer_info;
	struct net_device_stats stats;

	struct phy_device dummy_phy;
	struct phy_device *phydev;
	struct work_struct phy_pause;

	int id;

	struct mii_if_info mii_if;
	u32 advertising;

	u32 msg_enable;
	int media_state;
	int multicast;
	int promiscuous;
	u8 phy_addr;
	u8 state;
	u8 multi_list_size;

#ifdef MAX_MULTICAST_LIST
	u8 multi_list[MAX_MULTICAST_LIST][ETH_ALEN];
#endif
};

#endif

