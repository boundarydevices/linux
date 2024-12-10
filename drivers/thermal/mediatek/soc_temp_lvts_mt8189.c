// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/bits.h>
#include <linux/string.h>
#include <linux/iopoll.h>
#include "soc_temp_lvts_mt8189.h"
#include <dt-bindings/thermal/mediatek,lvts-thermal.h>
//#include "thermal_interface.h"
//#include "thermal_core.h"
//#include "thermal_hwmon.h"

/*==================================================
 * LVTS debug patch
 *==================================================
 */
#define DUMP_MORE_LOG

#ifdef DUMP_MORE_LOG
#define NUM_LVTS_DEVICE_REG (9)
#define LVTS_CONTROLLER_DEBUG_NUM (12)

static const unsigned int g_lvts_device_addrs[NUM_LVTS_DEVICE_REG] = {
	RG_TSFM_DATA_0,
	RG_TSFM_CTRL_1,
	RG_TSV2F_CTRL_0,
	RG_TSV2F_CTRL_4,
	RG_TEMP_DATA_0,
	RG_RC_DATA_0,
	RG_DIV_DATA_0,
	RG_DBG_FQMTR,
	RG_DID_LVTS};

static unsigned int g_lvts_device_value_b[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_DEVICE_REG];
static unsigned int g_lvts_device_value_e[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_DEVICE_REG];

#define NUM_LVTS_CONTROLLER_REG (32)
static const unsigned int g_lvts_controller_addrs[NUM_LVTS_CONTROLLER_REG] = {
	LVTSMONCTL0_0,
	LVTSMONCTL1_0,
	LVTSMONCTL2_0,
	LVTSMONINT_0,
	LVTSMONINTSTS_0,
	LVTSMONIDET3_0,
	LVTSMSRCTL0_0,
	LVTSMSRCTL1_0,
	LVTS_ID_0,
	LVTS_CONFIG_0,
	LVTSEDATA00_0,
	LVTSEDATA01_0,
	LVTSEDATA02_0,
	LVTSEDATA03_0,
	LVTSATPTGT_0,
	LVTSGSLOPE_0,
	LVTSOVSP0_0,
	LVTSOVSP1_0,
	LVTSOVSP2_0,
	LVTSOVSP3_0,
	LVTSMSR0_0,
	LVTSMSR1_0,
	LVTSMSR2_0,
	LVTSMSR3_0,
	LVTSRDATA0_0,
	LVTSRDATA1_0,
	LVTSRDATA2_0,
	LVTSRDATA3_0,
	LVTSPROTCTL_0,
	LVTSPROTTC_0,
	LVTSCLKEN_0,
	LVTSSPARE3_0};
static unsigned int g_lvts_controller_value_b[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_CONTROLLER_REG];
static unsigned int g_lvts_controller_value_e[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_CONTROLLER_REG];
#endif


/*==================================================
 * LVTS local common code
 *==================================================
 */
static int lvts_raw_to_temp_v1(struct formula_coeff *co, unsigned int sensor_id,
	unsigned int msr_raw)
{
	/* This function returns degree mC */

	int temp;

	temp = (co->a[0] * ((unsigned long long)msr_raw)) >> 14;
	temp = temp + co->golden_temp * 500 - co->a[0];

	return temp;
}

static unsigned int lvts_temp_to_raw_v1(struct formula_coeff *co, unsigned int sensor_id,
	int temp)
{
	unsigned int msr_raw = 0;

	msr_raw = ((long long)(((long long)co->golden_temp * 500 - co->a[0] - temp)) << 14)
		/ (-1 * co->a[0]);

	return msr_raw;
}

static noinline int __used lvts_read_tc_msr_raw(unsigned int *msr_reg)
{
	if (msr_reg == 0)
		return 0;

	return readl(msr_reg) & MRS_RAW_MASK;
}

static int __used lvts_read_all_tc_temperature(struct lvts_data *lvts_data, bool in_isr)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, s_index, msr_raw;
	int max_temp = THERMAL_TEMP_INVALID, current_temp, x;
	void __iomem *base;
	struct platform_ops *ops = &lvts_data->ops;
	struct device *dev = lvts_data->dev;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			if (tc[i].sensor_on_off[j] != SEN_ON)
				continue;

			s_index = tc[i].sensor_map[j];
			x = j + tc[i].ts_offset;

			msr_raw = lvts_read_tc_msr_raw(LVTSMSR0_0 + base + 0x4 * (x));

			if (msr_raw == 0)
				current_temp = THERMAL_TEMP_INVALID;
			else
				current_temp = ops->lvts_raw_to_temp(&(tc[i].coeff), j, msr_raw);

			if (current_temp > max_temp)
				max_temp = current_temp;

			if (!in_isr)
				mutex_lock(&lvts_data->sen_data_lock);

			lvts_data->sen_data[s_index].msr_raw = msr_raw;
			lvts_data->sen_data[s_index].temp = current_temp;
			if (!in_isr)
				mutex_unlock(&lvts_data->sen_data_lock);

			if (lvts_data->enable_runtime_log)
				dev_info(dev, "sen_data[%d].temp=%d\n",
					s_index, lvts_data->sen_data[s_index].temp);
		}
	}

	return max_temp;
}

static int lvts_read_tc_temperature(struct lvts_data *lvts_data, unsigned int tz_id, bool in_isr)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, msr_raw;
	unsigned int s_index = tz_id - 1;
	int current_temp, x;
	void __iomem *base;
	struct platform_ops *ops = &lvts_data->ops;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			if (s_index != tc[i].sensor_map[j])
				continue;

			x = j + tc[i].ts_offset;
			msr_raw = lvts_read_tc_msr_raw(LVTSMSR0_0 + base + 0x4 * (x));

			if (msr_raw == 0)
				current_temp = THERMAL_TEMP_INVALID;
			else
				current_temp = ops->lvts_raw_to_temp(&(tc[i].coeff), j, msr_raw);

			if (!in_isr)
				mutex_lock(&lvts_data->sen_data_lock);

			lvts_data->sen_data[s_index].msr_raw = msr_raw;
			lvts_data->sen_data[s_index].temp = current_temp;
			if (!in_isr)
				mutex_unlock(&lvts_data->sen_data_lock);

			return current_temp;
		}
	}

	return THERMAL_TEMP_INVALID;
}

static int soc_temp_lvts_read_temp(struct thermal_zone_device *tz, int *temperature)
{
	struct soc_temp_tz *lvts_tz = (struct soc_temp_tz *)tz->devdata;
	struct lvts_data *lvts_data = lvts_tz->lvts_data;

	if (lvts_tz->id == 0)
		*temperature = lvts_read_all_tc_temperature(lvts_data, false);
	else if (lvts_tz->id - 1 < lvts_data->num_sensor)
		*temperature = lvts_read_tc_temperature(lvts_data, lvts_tz->id, false);
	else
		return -EINVAL;

	return 0;
}

static void lvts_write_device(struct lvts_data *lvts_data, unsigned int data,
	int tc_id)
{
	void __iomem *base;
	struct device *dev = lvts_data->dev;
	int ret;

	base = GET_BASE_ADDR(tc_id);

	writel(data, LVTS_CONFIG_0 + base);

	udelay(5);
	ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				 !(data & DEVICE_ACCESS_STARTUS),
				 2, 200);
	if (ret)
		dev_err(dev,
			"write device err: LVTS %d didn't ready, data 0x%x\n", tc_id, data);
}

static unsigned int lvts_read_device(struct lvts_data *lvts_data,
	unsigned int reg_idx, int tc_id)
{
	struct device *dev = lvts_data->dev;
	void __iomem *base;
	unsigned int data;
	int ret;

	base = GET_BASE_ADDR(tc_id);
	writel(READ_DEVICE_REG(reg_idx), LVTS_CONFIG_0 + base);


	ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				 !(data & DEVICE_ACCESS_STARTUS),
				 2, 200);
	if (ret)
		dev_err(dev,
			"read device err: LVTS %d didn't ready, reg_idx 0x%x\n", tc_id, reg_idx);

	data = (readl(LVTSRDATA0_0 + base));

	return data;
}

static void lvts_reset(struct lvts_data *lvts_data)
{
	int i;

	for (i = 0; i < lvts_data->num_domain; i++) {
		if (lvts_data->domain[i].reset)
			reset_control_assert(lvts_data->domain[i].reset);

		usleep_range(20, 30);
		if (lvts_data->domain[i].reset)
			reset_control_deassert(lvts_data->domain[i].reset);
	}
}

static void device_identification_v1(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, data;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		writel(ENABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);

		lvts_write_device(lvts_data, RESET_ALL_DEVICES, i);

		lvts_write_device(lvts_data, READ_BACK_DEVICE_ID, i);

		/* Check LVTS device ID */
		data = (readl(LVTS_ID_0 + base) & GENMASK(7, 0));
		if (data != (0x81 + i))
			dev_err(dev, "LVTS_TC_%d, Device ID should be 0x%x, but 0x%x\n",
				i, (0x81 + i), data);
	}
}

static void disable_all_sensing_points(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		writel(DISABLE_SENSING_POINT, LVTSMONCTL0_0 + base);
	}
}

static void enable_all_sensing_points(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, num;
	void __iomem *base;
	unsigned int flag;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		num = tc[i].num_sensor;

		if (num > ALL_SENSING_POINTS) {
			dev_err(dev,
				"%s, LVTS%d, illegal number of sensors: %d\n",
				__func__, i, tc[i].num_sensor);
			continue;
		}

		flag = LVTS_SINGLE_SENSE;
		for (j = 0; j < tc[i].num_sensor; j++) {
			if (tc[i].sensor_on_off[j] != SEN_ON)
				continue;

			flag = flag | (0x1<<j);
		}

		if ((tc[i].ts_offset == 1) && (num == 1))
			writel(LVTS_SINGLE_SENSE | (0x1 << tc[i].ts_offset),
			       LVTSMONCTL0_0 + base);
		else
			writel(flag, LVTSMONCTL0_0 + base);
	}
}

#ifdef DUMP_MORE_LOG
static void read_controller_reg_before_active(struct lvts_data *lvts_data)
{
	int i, j, temp;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			temp = readl(LVTSMONCTL0_0 + g_lvts_controller_addrs[j]
				+ base);
			g_lvts_controller_value_b[i][j] = temp;
		}
	}
}

static void read_controller_reg_when_error(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;
	int temp, j;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			temp = readl(g_lvts_controller_addrs[j] + base);
			g_lvts_controller_value_e[i][j] = temp;
		}
	}
}

static int lvts_write_device_reg(struct lvts_data *lvts_data, unsigned int config,
	unsigned int dev_reg_idx, unsigned int data, int tc_id)
{
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	dev_reg_idx &= 0xFF;
	data &= 0xFF;

	config = config | (dev_reg_idx << 8) | data;


	writel(config, LVTS_CONFIG_0 + base);

	udelay(5);

	return 1;
}

static void read_device_reg_before_active(struct lvts_data *lvts_data)
{
	int i, j;
	unsigned int addr, data;

	for (i = 0; i < lvts_data->num_tc; i++) {
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			addr = g_lvts_device_addrs[j];
			data =  lvts_read_device(lvts_data, addr, i);
			g_lvts_device_value_b[i][j] = data;
		}
	}
}

static void read_device_reg_when_error(struct lvts_data *lvts_data)
{
	int i, j;
	unsigned int addr;
	void __iomem *base;
	int cnt;
	struct device *dev = lvts_data->dev;

	for (i = 0; i < lvts_data->num_tc; i++) {

		base = GET_BASE_ADDR(i);

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			addr = g_lvts_device_addrs[j];

			lvts_write_device_reg(lvts_data, 0x81020000, addr, 0x00, i);

			/* wait 2us + 3us buffer*/
			udelay(5);
			/* Check ASIF bus status for transaction finished
			 * Wait until DEVICE_ACCESS_START = 0
			 */
			cnt = 0;
			while ((readl(LVTS_CONFIG_0 + base) & BIT(24))) {
				cnt++;

				if (cnt == 100) {
					dev_info(dev, "Error: DEVICE_ACCESS_START didn't ready\n");
					break;
				}
				udelay(2);
			}

			g_lvts_device_value_e[i][j] = (readl(LVTSRDATA0_0 + base));
		}
	}
}

static void clear_lvts_register_value_array(struct lvts_data *lvts_data)
{
	int i, j;

	for (i = 0; i < lvts_data->num_tc; i++) {
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			g_lvts_controller_value_b[i][j] = 0;
			g_lvts_controller_value_e[i][j] = 0;
		}

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			g_lvts_device_value_b[i][j] = 0;
			g_lvts_device_value_e[i][j] = 0;
		}
	}
}

static void dump_lvts_device_register_value(struct lvts_data *lvts_data)
{
	int i, j, offset;
	char buffer[512];
	struct device *dev = lvts_data->dev;

	for (i = 0; i < lvts_data->num_tc; i++) {
		dev_info(dev, "[LVTS_ERROR][BEFROE][CONTROLLER_%d][DUMP]\n", i);
		offset = snprintf(buffer, sizeof(buffer), "[LVTS_ERROR][BEFORE][DEVICE][DUMP] ");
		if (offset < 0)
			return;
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			offset += snprintf(buffer + offset, sizeof(buffer) - offset, "0x%x:%x ",
				g_lvts_device_addrs[j], g_lvts_device_value_b[i][j]);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);


		offset = snprintf(buffer, sizeof(buffer), "[LVTS_ERROR][AFTER][DEVICE][DUMP] ");
		if (offset < 0)
			return;

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			offset += snprintf(buffer + offset, sizeof(buffer) - offset, "0x%x:%x ",
				g_lvts_device_addrs[j], g_lvts_device_value_e[i][j]);
		}
		if (offset < 0)
			return;
		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);
	}
}

static void dump_lvts_controller_register_value(struct lvts_data *lvts_data)
{
	int i, j, offset;
	char buffer[512];
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;


	for (i = 0; i < lvts_data->num_tc; i++) {
		dev_info(dev, "[LVTS_ERROR][BEFROE][CONTROLLER_%d]\n", i);

		offset = snprintf(buffer, sizeof(buffer),
			"[LVTS_ERROR][BEFORE][TC][DUMP] ");
		if (offset < 0)
			return;
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			offset += snprintf(buffer + offset,
					sizeof(buffer) - offset, "(0x%x:%x)",
					tc[i].addr_offset + g_lvts_controller_addrs[j],
					g_lvts_controller_value_b[i][j]);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);


		dev_info(dev, "[LVTS_ERROR][AFTER][CONTROLLER_%d]\n", i);

		offset = snprintf(buffer, sizeof(buffer),
						"[LVTS_ERROR][AFTER][TC][DUMP] ");
		if (offset < 0)
			return;

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			offset += snprintf(buffer + offset,
						sizeof(buffer) - offset, "(0x%x:%x)",
						tc[i].addr_offset + g_lvts_controller_addrs[j],
						g_lvts_controller_value_e[i][j]);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);
	}
}

static void dump_lvts_controller_temp_and_raw(struct lvts_data *lvts_data)
{
	int i, offset;
	char buffer[512];
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int j, s_index;

	for (i = 0; i < lvts_data->num_tc; i++) {

		dev_info(dev, "[LVTS_ERROR][CONTROLLER_%d] ", i);

		offset = snprintf(buffer, sizeof(buffer),
			"[LVTS_ERROR][TEMP][MSR_RAW] ");

		if (offset < 0)
			return;
		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];

			if (s_index >= lvts_data->num_sensor)
				continue;
			offset += snprintf(buffer + offset,
					sizeof(buffer) - offset, "[0x%x,%d]",
					lvts_data->sen_data[s_index].msr_raw,
					lvts_data->sen_data[s_index].temp);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);
	}
}

/*
 * lvts_thermal_check_all_sensing_point_idle -
 * Check if all sensing points are idle
 * Return: 0 if all sensing points are idle
 *         an error code if one of them is busy
 * error code[31:16]: an index of LVTS thermal controller
 * error code[2]: bit 10 of LVTSMSRCTL1
 * error code[1]: bit 7 of LVTSMSRCTL1
 * error code[0]: bit 0 of LVTSMSRCTL1
 */
static int lvts_thermal_check_all_sensing_point_idle(struct lvts_data *lvts_data)
{
	int i, temp, error_code;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		temp = readl(base + LVTSMSRCTL1_0);
		/* Check if bit10=bit7=bit0=0 */
		if ((temp & 0x481) != 0) {
			error_code = (i << 16) + ((temp & BIT(10)) >> 8) +
				((temp & BIT(7)) >> 6) +
				(temp & BIT(0));

			return error_code;
		}
	}

	return 0;
}

static void lvts_wait_for_all_sensing_point_idle(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	int cnt = 0, i, error_code, mask;
	int temp;
	void __iomem *base;

	mask = BIT(10) | BIT(7) | BIT(0);
	/*
	 * Wait until all sensoring points idled.
	 * No need to check LVTS status when suspend/resume,
	 * this will spend extra 100us of suspend flow.
	 * LVTS status will be reset after resume.
	 */
	while (cnt < 80) {
		temp = lvts_thermal_check_all_sensing_point_idle(lvts_data);
		if (temp == 0)
			goto TAIL;

		udelay(2);
		cnt++;
	}
	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		temp = readl(LVTSMSRCTL1_0 + base);
		if ((temp & mask) != 0) {
			error_code = ((temp & BIT(10)) >> 8) +
				((temp & BIT(7)) >> 6) +
				(temp & BIT(0));

			dev_info(dev, "Error LVTS %d sensing points aren't idle, error_code %d\n",
				i, error_code);
		}
	}
TAIL:
	return;
}


static void dump_lvts_error_info(struct lvts_data *lvts_data)
{
	/*dump controller registers*/
	read_controller_reg_when_error(lvts_data);
	dump_lvts_controller_temp_and_raw(lvts_data);

	dump_lvts_controller_register_value(lvts_data);

	lvts_wait_for_all_sensing_point_idle(lvts_data);

	read_device_reg_when_error(lvts_data);
	dump_lvts_device_register_value(lvts_data);
}
#endif

static void set_polling_speed(struct lvts_data *lvts_data, int tc_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int lvtsMonCtl1, lvtsMonCtl2;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	lvtsMonCtl1 = (((tc[tc_id].tc_speed.group_interval_delay
			<< 20) & GENMASK(29, 20)) |
			(tc[tc_id].tc_speed.period_unit &
			GENMASK(9, 0)));
	lvtsMonCtl2 = (((tc[tc_id].tc_speed.filter_interval_delay
			<< 16) & GENMASK(25, 16)) |
			(tc[tc_id].tc_speed.sensor_interval_delay
			& GENMASK(9, 0)));
	/*
	 * Clock source of LVTS thermal controller is 26MHz.
	 * Period unit is a base for all interval delays
	 * All interval delays must multiply it to convert a setting to time.
	 * Filter interval delay is a delay between two samples of the same sensor
	 * Sensor interval delay is a delay between two samples of different sensors
	 * Group interval delay is a delay between different rounds.
	 * For example:
	 *     If Period unit = C, filter delay = 1, sensor delay = 2, group delay = 1,
	 *     and two sensors, TS1 and TS2, are in a LVTS thermal controller
	 *     and then
	 *     Period unit = C * 1/26M * 256 = 12 * 38.46ns * 256 = 118.149us
	 *     Filter interval delay = 1 * Period unit = 118.149us
	 *     Sensor interval delay = 2 * Period unit = 236.298us
	 *     Group interval delay = 1 * Period unit = 118.149us
	 *
	 *     TS1    TS1 ... TS1    TS2    TS2 ... TS2    TS1...
	 *        <--> Filter interval delay
	 *                       <--> Sensor interval delay
	 *                                             <--> Group interval delay
	 */
	writel(lvtsMonCtl1, LVTSMONCTL1_0 + base);
	writel(lvtsMonCtl2, LVTSMONCTL2_0 + base);
}

static void set_hw_filter(struct lvts_data *lvts_data, int tc_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int option;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);
	option = tc[tc_id].hw_filter & 0x7;
	/* hw filter
	 * 000: Get one sample
	 * 001: Get 2 samples and average them
	 * 010: Get 4 samples, drop max and min, then average the rest of 2 samples
	 * 011: Get 6 samples, drop max and min, then average the rest of 4 samples
	 * 100: Get 10 samples, drop max and min, then average the rest of 8 samples
	 * 101: Get 18 samples, drop max and min, then average the rest of 16 samples
	 */
	option = (option << 9) | (option << 6) | (option << 3) | option;

	writel(option, LVTSMSRCTL0_0 + base);
}

static int get_dominator_index(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	int d_index;

	if (tc[tc_id].dominator_sensing_point == ALL_SENSING_POINTS) {
		d_index = ALL_SENSING_POINTS;
	} else if ((tc[tc_id].dominator_sensing_point <
		tc[tc_id].num_sensor) || (tc[tc_id].ts_offset != 0)) {
		d_index = tc[tc_id].dominator_sensing_point;
	} else {
		dev_err(dev,
			"Error: LVTS%d, dominator_sensing_point= %d should smaller than num_sensor= %d\n",
			tc_id, tc[tc_id].dominator_sensing_point,
			tc[tc_id].num_sensor);

		dev_err(dev, "Use the sensing point 0 as the dominated sensor\n");
		d_index = SENSING_POINT0;
	}

	return d_index;
}

static void disable_hw_reboot_interrupt(struct lvts_data *lvts_data, int tc_id)
{
	unsigned int temp;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	/* LVTS thermal controller has two interrupts for thermal HW reboot
	 * One is for AP SW and the other is for RGU
	 * The interrupt of AP SW can turn off by a bit of a register, but
	 * the other for RGU cannot.
	 * To prevent rebooting device accidentally, we are going to add
	 * a huge offset to LVTS and make LVTS always report extremely low
	 * temperature.
	 */

	/* After adding the huge offset 0x3FFF, LVTS alawys adds the
	 * offset to MSR_RAW.
	 * When MSR_RAW is larger, SW will convert lower temperature/
	 */
	temp = readl(LVTSPROTCTL_0 + base);
	writel(temp | 0x3FFF, LVTSPROTCTL_0 + base);

	/* Disable the interrupt of AP SW */
	temp = readl(LVTSMONINT_0 + base);

	temp = temp & ~(STAGE3_INT_EN);

	if (lvts_data->enable_dump_log) {
		temp = temp & ~(HIGH_OFFSET3_INT_EN |
						HIGH_OFFSET2_INT_EN |
						HIGH_OFFSET1_INT_EN |
						HIGH_OFFSET0_INT_EN);

		temp = temp & ~(LOW_OFFSET3_INT_EN |
						LOW_OFFSET2_INT_EN |
						LOW_OFFSET1_INT_EN |
						LOW_OFFSET0_INT_EN);
	}

	writel(temp, LVTSMONINT_0 + base);
}

static void enable_hw_reboot_interrupt(struct lvts_data *lvts_data, int tc_id)
{
	unsigned int temp;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	/* Enable the interrupt of AP SW */
	temp = readl(LVTSMONINT_0 + base);

	if (lvts_data->enable_dump_log) {
		temp = temp | HIGH_OFFSET3_INT_EN |
				HIGH_OFFSET2_INT_EN |
				HIGH_OFFSET1_INT_EN |
				HIGH_OFFSET0_INT_EN;

		temp = temp | LOW_OFFSET3_INT_EN |
				LOW_OFFSET2_INT_EN |
				LOW_OFFSET1_INT_EN |
				LOW_OFFSET0_INT_EN;
	} else {
		temp = temp | STAGE3_INT_EN;
	}

	writel(temp, LVTSMONINT_0 + base);

	/* Clear the offset */
	temp = readl(LVTSPROTCTL_0 + base);
	writel(temp & ~PROTOFFSET, LVTSPROTCTL_0 + base);
}

static void set_tc_hw_reboot_threshold(struct lvts_data *lvts_data,
	int trip_point, int tc_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int msr_raw, cur_msr_raw, temp, config, d_index, i;
	void __iomem *base;
	struct platform_ops *ops = &lvts_data->ops;

	base = GET_BASE_ADDR(tc_id);
	d_index = get_dominator_index(lvts_data, tc_id);

	disable_hw_reboot_interrupt(lvts_data, tc_id);

	temp = readl(LVTSPROTCTL_0 + base);
	if (d_index == ALL_SENSING_POINTS) {
		/* Maximum of 4 sensing points */
		config = (0x1 << 16);
		writel(config | temp, LVTSPROTCTL_0 + base);
		msr_raw = 0;
		for (i = 0; i < tc[tc_id].num_sensor; i++) {
			cur_msr_raw = ops->lvts_temp_to_raw(&(tc[tc_id].coeff), i, trip_point);
			if (msr_raw < cur_msr_raw)
				msr_raw = cur_msr_raw;
		}
	} else {
		/* Select protection sensor */
		config = ((d_index << 2) + 0x2) << 16;
		writel(config | temp, LVTSPROTCTL_0 + base);
		msr_raw = ops->lvts_temp_to_raw(&(tc[tc_id].coeff), d_index, trip_point);
	}

	if (lvts_data->enable_dump_log) {
		/* high offset INT */
		writel(msr_raw, LVTSOFFSETH_0 + base);

		/*
		 * lowoffset INT
		 * set a big msr_raw = 0xffff(very low temperature)
		 * to let lowoffset INT not be triggered
		 */
		writel(0xffff, LVTSOFFSETL_0 + base);
	} else {
		writel(msr_raw, LVTSPROTTC_0 + base);
	}

	enable_hw_reboot_interrupt(lvts_data, tc_id);
}

static void set_all_tc_hw_reboot(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	int i, trip_point;

	disable_all_sensing_points(lvts_data);
	lvts_wait_for_all_sensing_point_idle(lvts_data);
	for (i = 0; i < lvts_data->num_tc; i++) {
		trip_point = tc[i].hw_reboot_trip_point;

		if (tc[i].num_sensor == 0)
			continue;

		if (trip_point == DISABLE_THERMAL_HW_REBOOT) {
			disable_hw_reboot_interrupt(lvts_data, i);
			continue;
		}

		set_tc_hw_reboot_threshold(lvts_data, trip_point, i);
	}
	enable_all_sensing_points(lvts_data);
}

static void update_all_tc_hw_reboot_point(struct lvts_data *lvts_data,
	int trip_point)
{
	struct tc_settings *tc = lvts_data->tc;
	int i;

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].hw_reboot_trip_point = trip_point;
}

static int soc_temp_lvts_set_trip_temp(struct thermal_zone_device *tz,
		int trip, int temp)
{
	struct soc_temp_tz *lvts_tz = (struct soc_temp_tz *)tz->devdata;
	struct lvts_data *lvts_data = lvts_tz->lvts_data;
	const struct thermal_trip *trip_points;

	trip_points = lvts_data->tz_dev->trips;
	if (!trip_points)
		return -EINVAL;

	if (trip_points[trip].type != THERMAL_TRIP_CRITICAL || lvts_tz->id != 0)
		return 0;

	update_all_tc_hw_reboot_point(lvts_data, temp);
	set_all_tc_hw_reboot(lvts_data);

	return 0;
}

static const struct thermal_zone_device_ops soc_temp_lvts_ops = {
	.get_temp = soc_temp_lvts_read_temp,
	.set_trip_temp = soc_temp_lvts_set_trip_temp,
};

static void check_cal_data_v1(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct tc_settings *tc = lvts_data->tc;
	int i;

	cal_data->use_fake_efuse = 1;
	if (cal_data->golden_temp != 0) {
		cal_data->use_fake_efuse = 0;
	} else {
		for (i = 0; i < lvts_data->num_sensor; i++) {
			if (cal_data->count_r[i] != 0 ||
				cal_data->count_rc[i] != 0) {
				cal_data->use_fake_efuse = 0;
				break;
			}
		}
	}

	if (cal_data->use_fake_efuse) {
		/* It means all efuse data are equal to 0 */
		dev_err(dev,
			"[lvts_cal] This sample is not calibrated, fake !!\n");

		cal_data->golden_temp = cal_data->default_golden_temp;
		for (i = 0; i < lvts_data->num_sensor; i++) {
			cal_data->count_r[i] = cal_data->default_count_r;
			cal_data->count_rc[i] = cal_data->default_count_rc;
		}
	}

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].coeff.golden_temp = cal_data->golden_temp;

}

static int lvts_init(struct lvts_data *lvts_data)
{
	struct platform_ops *ops = &lvts_data->ops;
	struct device *dev = lvts_data->dev;
	int ret;

	if (!lvts_data->clock_gate_no_need) {
		ret = clk_prepare_enable(lvts_data->clk);
		if (ret) {
			dev_info(dev,
				"Error: Failed to enable lvts controller clock: %d\n",
				ret);
			return ret;
		}
	}

	if (!lvts_data->reset_no_need)
		lvts_reset(lvts_data);

	if (ops->device_identification)
		ops->device_identification(lvts_data);

	if (ops->device_enable_and_init)
		ops->device_enable_and_init(lvts_data);

	if (IS_ENABLE(FEATURE_DEVICE_AUTO_RCK)) {
		if (ops->device_enable_auto_rck)
			ops->device_enable_auto_rck(lvts_data);
	} else {
		if (ops->device_read_count_rc_n)
			ops->device_read_count_rc_n(lvts_data);
	}

	if (ops->set_cal_data)
		ops->set_cal_data(lvts_data);

	disable_all_sensing_points(lvts_data);
	lvts_wait_for_all_sensing_point_idle(lvts_data);
	if (ops->init_controller)
		ops->init_controller(lvts_data);

#ifdef DUMP_MORE_LOG
	clear_lvts_register_value_array(lvts_data);
	read_controller_reg_before_active(lvts_data);
	read_device_reg_before_active(lvts_data);
#endif

	enable_all_sensing_points(lvts_data);

	set_all_tc_hw_reboot(lvts_data);

	lvts_data->init_done = true;

	return 0;
}

static int prepare_calibration_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct platform_ops *ops = &lvts_data->ops;
	struct tc_settings *tc = lvts_data->tc;
	int i, offset, size, ret;
	char buffer[512];

	cal_data->count_r = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->count_r), GFP_KERNEL);
	if (!cal_data->count_r)
		return -ENOMEM;

	cal_data->count_rc = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->count_rc), GFP_KERNEL);
	if (!cal_data->count_rc)
		return -ENOMEM;

	if (ops->efuse_to_cal_data)
		ops->efuse_to_cal_data(lvts_data);

	if (ops->check_cal_data)
		ops->check_cal_data(lvts_data);

	dev_info(dev, "[lvts_cal] cali_mode = %d\n", cal_data->cali_mode);

	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[lvts_cal] num:g_count:g_count_rc ");
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;
	for (i = 0; i < lvts_data->num_sensor; i++) {
		ret = snprintf(buffer + offset, size - offset, "%d:%d:%d ",
				i, cal_data->count_r[i], cal_data->count_rc[i]);

		if (ret < 0) {
			dev_err(dev, "%s, %d, encode wrong\n", __func__, __LINE__);
			return -EINVAL;
		} else if (ret >= size - offset) {
			buffer[offset] = '\0';
			dev_info(dev, "%s\n", buffer);
			i--;
			offset = 0;
		} else {
			offset += ret;
		}
	}

	dev_info(dev, "%s\n", buffer);

	offset = snprintf(buffer, size, "[lvts_cal] num_tc:g_golden_temp");
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;
	for (i = 0; i < lvts_data->num_tc; i++) {
		ret = snprintf(buffer + offset, size - offset, "%d:%d ",
				i, tc[i].coeff.golden_temp);
		if (ret < 0) {
			dev_err(dev, "%s, %d, encode wrong\n", __func__, __LINE__);
			return -EINVAL;
		} else if (ret >= size - offset) {
			buffer[offset] = '\0';
			dev_info(dev, "%s\n", buffer);
			i--;
			offset = 0;
		} else {
			offset += ret;
		}
	}
	dev_info(dev, "%s\n", buffer);

	return 0;
}

static int get_calibration_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	char cell_name[8] = "e_data0";
	struct nvmem_cell *cell;
	u32 *buf;
	size_t len = 0;
	int i, j, index = 0, ret;

	lvts_data->efuse = devm_kcalloc(dev, lvts_data->num_efuse_addr,
				sizeof(*lvts_data->efuse), GFP_KERNEL);
	if (!lvts_data->efuse)
		return -ENOMEM;

	for (i = 0; i < lvts_data->num_efuse_block; i++) {
		cell_name[6] = '1' + i;
		cell = nvmem_cell_get(dev, cell_name);
		if (IS_ERR(cell)) {
			dev_err(dev, "Error: Failed to get nvmem cell %s\n",
				cell_name);
			return PTR_ERR(cell);
		}

		buf = (u32 *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);

		if (IS_ERR(buf))
			return PTR_ERR(buf);

		for (j = 0; j < (len / sizeof(u32)); j++) {
			if (index >= lvts_data->num_efuse_addr) {
				dev_err(dev, "Array efuse is going to overflow");
				kfree(buf);
				return -EINVAL;
			}

			lvts_data->efuse[index] = buf[j];
			index++;
		}

		kfree(buf);
	}

	ret = prepare_calibration_data(lvts_data);

	return ret;
}

static int of_update_lvts_data(struct lvts_data *lvts_data,
	struct platform_device *pdev)
{
	struct device *dev = lvts_data->dev;
	struct power_domain *domain;
	struct resource *res;
	struct platform_ops *ops = &lvts_data->ops;
	unsigned int i;
	int ret;
	int irq;

	if (!lvts_data->clock_gate_no_need) {
		lvts_data->clk = devm_clk_get(dev, "lvts_clk");
		if (IS_ERR(lvts_data->clk))
			return PTR_ERR(lvts_data->clk);

	}
	domain = devm_kcalloc(dev, lvts_data->num_domain, sizeof(*domain),
			GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	for (i = 0; i < lvts_data->num_domain; i++) {
		/* Get base address */
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(dev, "No IO resource, index %d\n", i);
			return -ENXIO;
		}

		domain[i].base = devm_ioremap_resource(dev, res);
		if (IS_ERR(domain[i].base)) {
			dev_err(dev, "Failed to remap io, index %d\n", i);
			return PTR_ERR(domain[i].base);
		}

		/* Get interrupt number */
		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			dev_err(dev, "No IRQ resource at index %d\n", i);
			return -ENOENT;
		}
		dev_info(dev, "domain[%d] irq_num=%d\n", i, irq);
		domain[i].irq_num = irq;

		if (!lvts_data->reset_no_need) {
			/* Get reset control */
			domain[i].reset = devm_reset_control_get_by_index(dev, i);
			if (IS_ERR(domain[i].reset)) {
				dev_err(dev, "Failed to get, index %d\n", i);
				return PTR_ERR(domain[i].reset);
			}
		}
	}

	lvts_data->domain = domain;

	lvts_data->sen_data = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*lvts_data->sen_data), GFP_KERNEL);
	if (!lvts_data->sen_data)
		return -ENOMEM;

	ret = get_calibration_data(lvts_data);
	if (ret)
		return ret;

	if (ops->update_coef_data)
		ops->update_coef_data(lvts_data);

	return 0;
}

static void lvts_device_close(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		lvts_write_device(lvts_data, RESET_ALL_DEVICES, i);
		writel(DISABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);
	}
}

static void lvts_close(struct lvts_data *lvts_data)
{
	disable_all_sensing_points(lvts_data);
	lvts_wait_for_all_sensing_point_idle(lvts_data);
	lvts_device_close(lvts_data);
	if (!lvts_data->clock_gate_no_need)
		clk_disable_unprepare(lvts_data->clk);

}

static void tc_irq_handler(struct lvts_data *lvts_data, int tc_id, char thermintst_str[])
{
	struct device *dev = lvts_data->dev;
	unsigned int ret = 0;
	void __iomem *base;
	int temp;

	if (IS_ENABLE(FEATURE_6989_SCP_OC)) {
		base = GET_BASE_ADDR(tc_id);
		ret = readl(LVTSMONINTSTS_0 + base);
		if (!(ret & THERMAL_PROTECTION_STAGE_3))
			return;

		dev_info(dev, "%s", thermintst_str);
	}

#ifdef DUMP_MORE_LOG
	temp = lvts_read_all_tc_temperature(lvts_data, true);
	dump_lvts_error_info(lvts_data);
#endif

	base = GET_BASE_ADDR(tc_id);

	ret = readl(LVTSMONINTSTS_0 + base);
	/* Write back to clear interrupt status */
	writel(ret, LVTSMONINTSTS_0 + base);

	dev_info(dev, "[Thermal IRQ] LVTS thermal controller %d, LVTSMONINTSTS=0x%08x, T=%d\n",
		tc_id, ret, temp);

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset0 interrupt triggered, sw reset\n");
	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset1 interrupt triggered, sw reset\n");
	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset2 interrupt triggered, sw reset\n");
	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset3 interrupt triggered, sw reset\n");


	if (ret & THERMAL_PROTECTION_STAGE_3)
		dev_info(dev,
			"[Thermal IRQ]: Thermal protection stage 3 interrupt triggered, HW reboot\n");
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct lvts_data *lvts_data = (struct lvts_data *) dev_id;
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i;
	void __iomem *base;
	char thermintst_str[200];
	int thermintst_str_size = sizeof(thermintst_str);
	int thermintst_str_offset = 0;

	if (IS_ENABLE(FEATURE_6989_SCP_OC)) {
		for (i = 0; i < lvts_data->num_tc; i++) {
			base = GET_BASE_ADDR(i);
			if (readl(LVTSMONINTSTS_0 + base) & THERMAL_PROTECTION_STAGE_3) {
				disable_all_sensing_points(lvts_data);
				break;
			}
		}
	} else {
		disable_all_sensing_points(lvts_data);
	}
	for (i = 0; i < lvts_data->num_domain; i++) {
		base = lvts_data->domain[i].base;
		lvts_data->irq_bitmap[i] = readl(THERMINTST + base);
		if ((!IS_ENABLE(FEATURE_SCP_OC)) && (!IS_ENABLE(FEATURE_6989_SCP_OC)))
			dev_info(dev, "%s : THERMINTST = 0x%x\n", __func__,
				lvts_data->irq_bitmap[i]);
		else
			thermintst_str_offset += snprintf(thermintst_str + thermintst_str_offset,
				thermintst_str_size - thermintst_str_offset,
				"%s : THERMINTST = 0x%x\n", __func__, lvts_data->irq_bitmap[i]);
	}
	if (thermintst_str_offset < 0)
		return -EINVAL;
	if (thermintst_str_offset >= thermintst_str_size)
		return -ENOMEM;

	thermintst_str[thermintst_str_offset] = '\0';

	for (i = 0; i < lvts_data->num_tc; i++) {
		if ((lvts_data->irq_bitmap[tc[i].domain_index] & tc[i].irq_bit) == 0 &&
			lvts_data->irq_bitmap[tc[i].domain_index] != 0)
			tc_irq_handler(lvts_data, i, thermintst_str);
	}

	return IRQ_HANDLED;
}

static int lvts_register_irq_handler(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, *irq_bitmap;
	int ret;

	irq_bitmap = devm_kcalloc(dev, lvts_data->num_domain, sizeof(*irq_bitmap),
			GFP_KERNEL);
	if (!irq_bitmap)
		return -ENOMEM;

	lvts_data->irq_bitmap = irq_bitmap;

	for (i = 0; i < lvts_data->num_domain; i++) {
		ret = devm_request_irq(dev, lvts_data->domain[i].irq_num,
			irq_handler, IRQF_TRIGGER_HIGH, "mtk_lvts", lvts_data);

		if (ret) {
			dev_err(dev,
				"Failed to register LVTS IRQ, ret %d, domain %d irq_num %d\n",
				ret, i, lvts_data->domain[i].irq_num);
			return ret;
		}
	}

	return 0;
}

static int lvts_register_thermal_zone(int id, struct lvts_data *lvts_data,
		struct thermal_zone_device **tzdev)
{
	struct device *dev = lvts_data->dev;
	struct soc_temp_tz *lvts_tz;
	int ret = 0;

	lvts_tz = devm_kzalloc(dev, sizeof(*lvts_tz), GFP_KERNEL);
	if (!lvts_tz)
		return -ENOMEM;

	lvts_tz->id = id;
	lvts_tz->lvts_data = lvts_data;

	*tzdev = devm_thermal_of_zone_register(dev, lvts_tz->id,
			lvts_tz, &soc_temp_lvts_ops);

	if (IS_ERR(*tzdev)) {
		ret = PTR_ERR(*tzdev);
		dev_err(dev,
			"Error: Failed to register lvts tz %d, ret = %d\n",
			lvts_tz->id, ret);
	}
	return ret;
}

static int lvts_register_thermal_zones(struct lvts_data *lvts_data)
{
	struct thermal_zone_device *tzdev;
	int i, j, s_index, ret = 0;

	ret = lvts_register_thermal_zone(0, lvts_data, &tzdev);
	if (ret)
		return ret;

	lvts_data->tz_dev = tzdev;

	for (i = 0; i < lvts_data->num_tc; i++) {
		for (j = 0; j < lvts_data->tc[i].num_sensor; j++) {
			if (lvts_data->tc[i].sensor_on_off[j] != SEN_ON)
				continue;

			s_index = lvts_data->tc[i].sensor_map[j];

			ret = lvts_register_thermal_zone(s_index + 1, lvts_data, &tzdev);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static void check_runtime_log_from_dts(struct lvts_data *lvts_data,
	struct platform_device *pdev)
{
	int enable_runtime_log = 0;
	struct device_node *node = NULL;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		if (of_property_read_u32(node, "enable-runtime-log", &enable_runtime_log)) {
			lvts_data->enable_runtime_log = 0;
			return;
		}
		pr_info("[%s] Check enable_runtime_log(%d)\n", __func__, enable_runtime_log);
		lvts_data->enable_runtime_log = ((enable_runtime_log == 0) ? 0 : 1);
	} else {
		pr_notice("[%s] can't find LVTS compatible node\n", __func__);
		lvts_data->enable_runtime_log = 0;
	}
}

static int lvts_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lvts_data *lvts_data;
	int ret;

	lvts_data = (struct lvts_data *) of_device_get_match_data(dev);
	if (!lvts_data)	{
		dev_err(dev, "Error: Failed to get lvts platform data\n");
		return -ENODATA;
	}

	lvts_data->dev = &pdev->dev;

	check_runtime_log_from_dts(lvts_data, pdev);

	ret = of_update_lvts_data(lvts_data, pdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, lvts_data);

	if (lvts_data->init_in_lk)
		dev_info(dev, "[Thermal/LVTS] init done in lk2\n");
	else {
		ret = lvts_init(lvts_data);
		if (ret)
			return ret;
	}

#ifdef DUMP_MORE_LOG
	clear_lvts_register_value_array(lvts_data);
	read_controller_reg_before_active(lvts_data);
#endif

	ret = lvts_register_irq_handler(lvts_data);
	if (ret)
		return ret;

	mutex_init(&lvts_data->sen_data_lock);

	ret = lvts_register_thermal_zones(lvts_data);
	if (ret)
		return ret;

	return 0;
}

static int lvts_remove(struct platform_device *pdev)
{
	return 0;
}

static int lvts_suspend_noirq(struct device *dev)
{
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) dev_get_drvdata(dev);
	dev_info(dev, "[Thermal/LVTS]%s, suspend_support = %d\n", __func__,
		lvts_data->is_suspend_support);

	if (lvts_data->is_suspend_support)
		lvts_close(lvts_data);

	return 0;
}

static int lvts_resume_noirq(struct device *dev)
{
	int ret;
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) dev_get_drvdata(dev);
	dev_info(dev, "[Thermal/LVTS]%s, suspend_support = %d\n", __func__,
		lvts_data->is_suspend_support);

	if (lvts_data->is_suspend_support) {
		ret = lvts_init(lvts_data);
		if (ret)
			return ret;
	} else {
	#ifdef DUMP_MORE_LOG
		clear_lvts_register_value_array(lvts_data);
		read_controller_reg_before_active(lvts_data);
	#endif
	}
	return 0;
}
/*==================================================
 * LVTS v4 common code
 *==================================================
 */
#define STOP_COUNTING_V4 (DEVICE_WRITE | RG_TSFM_CTRL_0 << 8 | 0x00)
#define SET_RG_TSFM_LPDLY_V4 (DEVICE_WRITE | RG_TSFM_CTRL_4 << 8 | 0xA6)
#define SET_COUNTING_WINDOW_20US1_V4 (DEVICE_WRITE | RG_TSFM_CTRL_2 << 8 | 0x00)
#define SET_COUNTING_WINDOW_20US2_V4 (DEVICE_WRITE | RG_TSFM_CTRL_1 << 8 | 0x20)
#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_2 << 8	\
						| 0x84)
#define TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_4 << 8	\
						| 0x7C)
#define SET_TS_RSV_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_1 << 8 | 0x8D)
#define SET_TS_EN_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF4)
#define TOGGLE_RG_TSV2F_VCO_RST1_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xFC)
#define TOGGLE_RG_TSV2F_VCO_RST2_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF4)

#define SET_LVTS_AUTO_RCK_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_6 << 8 | 0x01)
#define SELECT_SENSOR_RCK_V4(id) (DEVICE_WRITE | RG_TSV2F_CTRL_5 << 8 | id)
#define SET_DEVICE_SINGLE_MODE_V4 (DEVICE_WRITE | RG_TSFM_CTRL_3 << 8 | 0x78)
#define KICK_OFF_RCK_COUNTING_V4 (DEVICE_WRITE | RG_TSFM_CTRL_0 << 8 | 0x02)
#define SET_SENSOR_NO_RCK_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_5 << 8 | 0x10)
#define SET_DEVICE_LOW_POWER_SINGLE_MODE_V4 (DEVICE_WRITE | RG_TSFM_CTRL_3 << 8	\
						| 0xB8)

static void device_enable_auto_rck_v4(struct lvts_data *lvts_data)
{
	unsigned int i;

	for (i = 0; i < lvts_data->num_tc; i++)
		lvts_write_device(lvts_data, SET_LVTS_AUTO_RCK_V4, i);
}

static int device_read_count_rc_n_v4(struct lvts_data *lvts_data)
{

	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int size, s_index, data;
	void __iomem *base;
	int ret, i, j, offset;
	char buffer[512];


	if (lvts_data->init_done) {

		for (i = 0; i < lvts_data->num_tc; i++) {
			lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
			lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
		}
		return 0;
	}


	cal_data->count_rc_now = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->count_rc_now), GFP_KERNEL);

	cal_data->efuse_data = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->efuse_data), GFP_KERNEL);

	if ((!cal_data->count_rc_now) || (!cal_data->efuse_data))
		return -ENOMEM;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];

			lvts_write_device(lvts_data, SELECT_SENSOR_RCK_V4(j), i);
			lvts_write_device(lvts_data, SET_DEVICE_SINGLE_MODE_V4, i);
			udelay(10);

			lvts_write_device(lvts_data, KICK_OFF_RCK_COUNTING_V4, i);
			udelay(30);

			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_SENSING_STATUS),
				2, 200);
			if (ret)
				dev_err(dev,
					"Error: LVTS %d DEVICE_SENSING_STATUS didn't ready\n",
					i);

			data = lvts_read_device(lvts_data, 0x00, i);

			cal_data->count_rc_now[s_index] = (data & GENMASK(23, 0));

			/*count data here that want to set to efuse later*/
			cal_data->efuse_data[s_index] = (((unsigned long long)
				cal_data->count_rc_now[s_index]) *
				cal_data->count_r[s_index]) >> 14;
		}

		/* Recover Setting for Normal Access on
		 * temperature fetch
		 */
		lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
		lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
	}

	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[COUNT_RC_NOW] ");
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d ",
				i, cal_data->count_rc_now[i]);
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

static void set_calibration_data_v4(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int i, j, s_index, e_data, x;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		for (j = 0; j < tc[i].num_sensor; j++) {
			if (tc[i].sensor_on_off[j] != SEN_ON)
				continue;

			s_index = tc[i].sensor_map[j];
			x = j + tc[i].ts_offset;

			if (IS_ENABLE(FEATURE_DEVICE_AUTO_RCK))
				e_data = cal_data->count_r[s_index];
			else
				e_data = cal_data->efuse_data[s_index];

			writel(e_data, LVTSEDATA00_0 + base + 0x4 * (x));
		}
	}
}


static void init_controller_v4(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
		writel(0x13121110, LVTSTSSEL_0 + base);
		writel(SET_CALC_SCALE_RULES, LVTSCALSCALE_0 + base);

		set_polling_speed(lvts_data, i);
		set_hw_filter(lvts_data, i);
	}
}

/*==================================================
 * LVTS MT6985
 *==================================================
 */
#define SET_TS_DIS_V6 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8	\
						| 0xB1)

#define SET_LVTS_MANUAL_RCK_OPERATION_V6 (DEVICE_WRITE | RG_TSV2F_CTRL_6 << 8 | 0x00)


#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V6_1 (DEVICE_WRITE | RG_TSV2F_CTRL_2 << 8	\
						| 0x30)

#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V6_2 (DEVICE_WRITE | RG_TSV2F_CTRL_3 << 8	\
						| 0x03)

#define TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V6 (DEVICE_WRITE | RG_TSV2F_CTRL_4 << 8	\
						| 0xFF)

#define SET_DEVICE_LOW_POWER_SINGLE_MODE_V6 (DEVICE_WRITE | RG_TSFM_CTRL_3 << 8	\
						| 0xF8)

#define SET_TS_RSV_V6 (DEVICE_WRITE | RG_TSV2F_CTRL_1 << 8 | 0x95)
#define SET_TS_CHOP_CTRL_V6 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF1)
#define SET_TS_DIV_EN_V6 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xB5)
#define SET_VCO_RST_V6 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xBD)
#define COF_A_T_SLP_GLD_V6 224280
#define COF_A_COUNT_R_GLD_V6 14698
#define COF_A_CONST_OFS_V6 280000
#define COF_A_OFS_V6 (COF_A_T_SLP_GLD_V6 - COF_A_CONST_OFS_V6)
#define COF_A_T_SLP_GLD_HT_V6 279280
#define COF_A_COUNT_R_GLD_HT_V6 18302
#define COF_A_CONST_OFS_HT_V6 170000
#define COF_A_OFS_HT_V6 (COF_A_T_SLP_GLD_HT_V6 - COF_A_CONST_OFS_HT_V6)

#define SET_COUNTING_WINDOW_47US2 (DEVICE_WRITE | RG_TSFM_CTRL_1 << 8 | 0x47)

static void device_enable_and_init_v6(struct lvts_data *lvts_data)
{
	unsigned int i;

	for (i = 0; i < lvts_data->num_tc; i++) {
		lvts_write_device(lvts_data, STOP_COUNTING_V4, i);
		lvts_write_device(lvts_data, SET_RG_TSFM_LPDLY_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US1_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_47US2, i);
		lvts_write_device(lvts_data, TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V6_1, i);
		lvts_write_device(lvts_data, TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V6_2, i);
		lvts_write_device(lvts_data, TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V6, i);
		lvts_write_device(lvts_data, SET_TS_RSV_V6, i);
		lvts_write_device(lvts_data, SET_TS_CHOP_CTRL_V6, i);
	}

	lvts_data->counting_window_us = 20;
}

/*==================================================
 * LVTS MT8189
 *==================================================
 */
enum mt8189_lvts_domain {
	MT8189_AP_DOMAIN,
	MT8189_MCU_DOMAIN,
	MT8189_NUM_DOMAIN
};

enum mt8189_lvts_sensor_enum {
	MT8189_TS1_0,
	MT8189_TS1_1,
	MT8189_TS1_2,
	MT8189_TS1_3,
	MT8189_TS2_0,
	MT8189_TS2_1,
	MT8189_TS2_2,
	MT8189_TS2_3,
	MT8189_TS3_0,
	MT8189_TS3_1,
	MT8189_TS3_2,//10
	MT8189_TS3_3,
	MT8189_TS4_0,
	MT8189_TS4_1,
	MT8189_TS4_2,
	MT8189_TS4_3,
	MT8189_TS5_0,
	MT8189_TS5_1,
	MT8189_NUM_TS //18
};


enum mt8189_lvts_controller_enum {
	MT8189_LVTS_MCU_CTRL0,
	MT8189_LVTS_MCU_CTRL1,
	MT8189_LVTS_MCU_CTRL2,
	MT8189_LVTS_AP_CTRL0,
	MT8189_LVTS_GPU_CTRL0,
	MT8189_LVTS_CTRL_NUM
};

static void mt8189_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct tc_settings *tc = lvts_data->tc;
	int i = 0;

	cal_data->cali_mode = GET_CAL_DATA_BIT(0, 31);
	cal_data->golden_temp_ht = GET_CAL_DATA_BITMASK(0, 15, 8);
	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 7, 0);

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].coeff.golden_temp = cal_data->golden_temp;

	if (cal_data->cali_mode == 1) {
		for (i = 0; i < lvts_data->num_tc; i++) {
			if (tc[i].coeff.cali_mode == CALI_HT)
				tc[i].coeff.golden_temp = cal_data->golden_temp_ht;
		}
	}

	cal_data->count_r[MT8189_TS1_0] =  GET_CAL_DATA_BITMASK(1, 23, 0);
	cal_data->count_r[MT8189_TS1_1] =  GET_CAL_DATA_BITMASK(2, 23, 0);
	cal_data->count_r[MT8189_TS1_2] =  GET_CAL_DATA_BITMASK(3, 23, 0);
	cal_data->count_r[MT8189_TS1_3] =  GET_CAL_DATA_BITMASK(4, 23, 0);

	cal_data->count_r[MT8189_TS2_0] =  GET_CAL_DATA_BITMASK(6, 23, 0);
	cal_data->count_r[MT8189_TS2_1] =  GET_CAL_DATA_BITMASK(7, 23, 0);
	cal_data->count_r[MT8189_TS2_2] =  GET_CAL_DATA_BITMASK(8, 23, 0);
	cal_data->count_r[MT8189_TS2_3] =  GET_CAL_DATA_BITMASK(9, 23, 0);

	cal_data->count_r[MT8189_TS3_0] =  GET_CAL_DATA_BITMASK(11, 23, 0);
	cal_data->count_r[MT8189_TS3_1] =  GET_CAL_DATA_BITMASK(12, 23, 0);
	cal_data->count_r[MT8189_TS3_2] =  GET_CAL_DATA_BITMASK(13, 23, 0);
	cal_data->count_r[MT8189_TS3_3] =  GET_CAL_DATA_BITMASK(14, 23, 0);

	cal_data->count_r[MT8189_TS4_0] =  GET_CAL_DATA_BITMASK(16, 23, 0);
	cal_data->count_r[MT8189_TS4_1] =  GET_CAL_DATA_BITMASK(17, 23, 0);
	cal_data->count_r[MT8189_TS4_2] =  GET_CAL_DATA_BITMASK(18, 23, 0);
	cal_data->count_r[MT8189_TS4_3] =  GET_CAL_DATA_BITMASK(19, 23, 0);

	cal_data->count_r[MT8189_TS5_0] =  GET_CAL_DATA_BITMASK(21, 23, 0);
	cal_data->count_r[MT8189_TS5_1] =  GET_CAL_DATA_BITMASK(22, 23, 0);

	/* GET_CAL_DATA_BITMASK(13, 15, 0); GET_CAL_DATA_BITMASK(14, 15, 0); */

	cal_data->count_rc[MT8189_LVTS_MCU_CTRL0] =  GET_CAL_DATA_BITMASK(0, 31, 8);
	cal_data->count_rc[MT8189_LVTS_MCU_CTRL1] =  GET_CAL_DATA_BITMASK(5, 23, 0);
	cal_data->count_rc[MT8189_LVTS_MCU_CTRL2] =  GET_CAL_DATA_BITMASK(10, 23, 0);

	cal_data->count_rc[MT8189_LVTS_AP_CTRL0] =  GET_CAL_DATA_BITMASK(15, 23, 0);

	cal_data->count_rc[MT8189_LVTS_GPU_CTRL0] =  GET_CAL_DATA_BITMASK(20, 23, 0);
	/* GET_CAL_DATA_BITMASK(23, 23, 0); */

}

static struct tc_settings mt8189_tc_settings[] = {
	[MT8189_LVTS_MCU_CTRL0] = {
		.domain_index = MT8189_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 4,
		.sensor_map = {MT8189_TS1_0, MT8189_TS1_1, MT8189_TS1_2, MT8189_TS1_3},
		.sensor_on_off = {SEN_ON, SEN_ON, SEN_ON, SEN_ON},
		.tc_speed = SET_TC_SPEED_IN_US(10, 2460, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = ALL_SENSING_POINTS,
		.hw_reboot_trip_point = 119000,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-250460},
		},
	},
	[MT8189_LVTS_MCU_CTRL1] = {
		.domain_index = MT8189_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 4,
		.sensor_map = {MT8189_TS2_0, MT8189_TS2_1, MT8189_TS2_2, MT8189_TS2_3},
		.sensor_on_off = {SEN_ON, SEN_ON, SEN_ON, SEN_ON},
		.tc_speed = SET_TC_SPEED_IN_US(10, 2460, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = ALL_SENSING_POINTS,
		.hw_reboot_trip_point = 119000,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-250460},
		},
	},
	[MT8189_LVTS_MCU_CTRL2] = {
		.domain_index = MT8189_MCU_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 4,
		.sensor_map = {MT8189_TS3_0, MT8189_TS3_1, MT8189_TS3_2, MT8189_TS3_3},
		.sensor_on_off = {SEN_ON, SEN_ON, SEN_ON, SEN_ON},
		.tc_speed = SET_TC_SPEED_IN_US(10, 4490, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = ALL_SENSING_POINTS,
		.hw_reboot_trip_point = 119000,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
		},
	},
	[MT8189_LVTS_AP_CTRL0] = {
		.domain_index = MT8189_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 4,
		.sensor_map = {MT8189_TS4_0, MT8189_TS4_1, MT8189_TS4_2, MT8189_TS4_3},
		.sensor_on_off = {SEN_ON, SEN_ON, SEN_ON, SEN_ON},
		.tc_speed = SET_TC_SPEED_IN_US(10, 2100, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = ALL_SENSING_POINTS,
		.hw_reboot_trip_point = 119000,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-250460},
		},
	},
	[MT8189_LVTS_GPU_CTRL0] = {
		.domain_index = MT8189_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT8189_TS5_0, MT8189_TS5_1},
		.sensor_on_off = {SEN_ON, SEN_ON},
		.tc_speed = SET_TC_SPEED_IN_US(10, 1380, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = ALL_SENSING_POINTS,
		.hw_reboot_trip_point = 119000,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-250460},
		},
	},
};

static struct lvts_data mt8189_lvts_data = {
	.num_domain = MT8189_NUM_DOMAIN,
	.num_tc = MT8189_LVTS_CTRL_NUM,
	.tc = mt8189_tc_settings,
	.num_sensor = MT8189_NUM_TS,
	.ops = {
		.device_identification = device_identification_v1,
		.efuse_to_cal_data = mt8189_efuse_to_cal_data,
		.device_enable_and_init = device_enable_and_init_v6,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = device_read_count_rc_n_v4,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v1,
		.lvts_raw_to_temp = lvts_raw_to_temp_v1,
		.check_cal_data = check_cal_data_v1,
	},
	.feature_bitmap = FEATURE_DEVICE_AUTO_RCK | FEATURE_CK26M_ACTIVE,
	.num_efuse_addr = 24,
	.num_efuse_block = 2,
	.cal_data = {
		.default_golden_temp = 50,
		.default_count_r = 35000,
		.default_count_rc = 2750,
	},
	.init_done = false,
	.enable_dump_log = 0,
	.clock_gate_no_need = true,
	.reset_no_need = true,
	.init_in_lk = true, /* lvts init in lk */
};

/*==================================================
 * Support chips
 *==================================================
 */
static const struct dev_pm_ops lvts_pm_ops = {
	.suspend_noirq = lvts_suspend_noirq,
	.resume_noirq = lvts_resume_noirq,
};

static const struct of_device_id lvts_of_match[] = {
	{
		.compatible = "mediatek,mt8189-lvts",
		.data = (void *)&mt8189_lvts_data,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, lvts_of_match);
/*==================================================*/
static struct platform_driver soc_temp_lvts = {
	.probe = lvts_probe,
	.remove = lvts_remove,
	.driver = {
		.name = "mtk-soc-temp-lvts-mt8189",
		.of_match_table = lvts_of_match,
		.pm = &lvts_pm_ops,
	},
};

module_platform_driver(soc_temp_lvts);
MODULE_AUTHOR("Yu-Chia Chang <ethan.chang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek soc temperature driver");
MODULE_LICENSE("GPL");
