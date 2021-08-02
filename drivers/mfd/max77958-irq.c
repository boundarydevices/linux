/*******************************************************************************
* Copyright (C) Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/mfd/max77958-private.h>
#include <linux/usbc/max77958-usbc.h>


static const u8 max77958_mask_reg[] = {
	[USBC_INT] = REG_UIC_INT_M,
	[CC_INT] = REG_CC_INT_M,
	[PD_INT] = REG_PD_INT_M,
	[ACTION_INT] = REG_ACTION_INT_M,
};

static struct i2c_client *get_i2c(struct max77958_dev *max77958,
		enum max77958_irq_source src)
{
	switch (src) {
	case USBC_INT:
	case CC_INT:
	case PD_INT:
	case ACTION_INT:
		return max77958->i2c;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct max77958_irq_data {
	int mask;
	enum max77958_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct max77958_irq_data max77958_irqs[] = {

	DECLARE_IRQ(MAX77958_USBC_IRQ_APC_INT,	USBC_INT, 1 << 7),
	DECLARE_IRQ(MAX77958_USBC_IRQ_SYSM_INT,	USBC_INT, 1 << 6),
	DECLARE_IRQ(MAX77958_USBC_IRQ_VBUS_INT,	USBC_INT, 1 << 5),
	DECLARE_IRQ(MAX77958_USBC_IRQ_VBADC_INT, USBC_INT, 1 << 4),
	DECLARE_IRQ(MAX77958_USBC_IRQ_DCD_INT,	USBC_INT, 1 << 3),
	DECLARE_IRQ(MAX77958_USBC_IRQ_CHGT_INT,	USBC_INT, 1 << 1),
	DECLARE_IRQ(MAX77958_USBC_IRQ_UIDADC_INT, USBC_INT, 1 << 0),

	DECLARE_IRQ(MAX77958_CC_IRQ_VCONNCOP_INT, CC_INT, 1 << 7),
	DECLARE_IRQ(MAX77958_CC_IRQ_VSAFE0V_INT, CC_INT, 1 << 6),
	DECLARE_IRQ(MAX77958_CC_IRQ_DETABRT_INT, CC_INT, 1 << 5),
	DECLARE_IRQ(MAX77958_CC_IRQ_WTR_INT, CC_INT, 1 << 4),
	DECLARE_IRQ(MAX77958_CC_IRQ_CCPINSTAT_INT, CC_INT, 1 << 3),
	DECLARE_IRQ(MAX77958_CC_IRQ_CCISTAT_INT, CC_INT, 1 << 2),
	DECLARE_IRQ(MAX77958_CC_IRQ_CCVCNSTAT_INT, CC_INT, 1 << 1),
	DECLARE_IRQ(MAX77958_CC_IRQ_CCSTAT_INT, CC_INT, 1 << 0),

	DECLARE_IRQ(MAX77958_PD_IRQ_PDMSG_INT,	PD_INT, 1 << 7),
	DECLARE_IRQ(MAX77958_PD_IRQ_PS_RDY_INT, PD_INT, 1 << 6),
	DECLARE_IRQ(MAX77958_PD_IRQ_DATAROLE_INT, PD_INT, 1 << 5),
	/*DECLARE_IRQ(MAX77958_IRQ_VDM_ATTENTION_INT,	PD_INT, 1 << 4), */
	/*DECLARE_IRQ(MAX77958_IRQ_VDM_DP_CONFIGURE_INT, PD_INT, 1 << 3), */
	DECLARE_IRQ(MAX77958_PD_IRQ_DISPLAYPORT_INT, PD_INT, 1 << 2),
	/*DECLARE_IRQ(MAX77958_PD_IRQ_SSACC_INT, PD_INT, 1 << 1), */
	/*DECLARE_IRQ(MAX77958_PD_IRQ_FCTIDI_INT, PD_INT, 1 << 0), */

	/*DECLARE_IRQ(MAX77958_IRQ_VDM_DP_STATUS_UPDATE_INT, PD_INT, 1 << 2),*/


	DECLARE_IRQ(MAX77958_IRQ_ACTION0_INT, ACTION_INT, 1 << 0),
	DECLARE_IRQ(MAX77958_IRQ_ACTION1_INT, ACTION_INT, 1 << 1),
	DECLARE_IRQ(MAX77958_IRQ_ACTION2_INT, ACTION_INT, 1 << 2),
	DECLARE_IRQ(MAX77958_IRQ_EXTENDED_ACTION_INT, ACTION_INT, 1 << 3),
};

static void max77958_irq_lock(struct irq_data *data)
{
	struct max77958_dev *max77958 = irq_get_chip_data(data->irq);

	mutex_lock(&max77958->irqlock);
}

static void max77958_irq_sync_unlock(struct irq_data *data)
{
	struct max77958_dev *max77958 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < MAX77958_IRQ_GROUP_NR; i++) {
		u8 mask_reg = max77958_mask_reg[i];
		struct i2c_client *i2c = get_i2c(max77958, i);

		if (mask_reg == MAX77958_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		max77958->irq_masks_cache[i] = max77958->irq_masks_cur[i];

		max77958_write_reg(i2c, max77958_mask_reg[i],
				max77958->irq_masks_cur[i]);
	}

	mutex_unlock(&max77958->irqlock);
}

	static const inline struct max77958_irq_data *
irq_to_max77958_irq(struct max77958_dev *max77958, int irq)
{
	return &max77958_irqs[irq - max77958->irq_base];
}

static void max77958_irq_mask(struct irq_data *data)
{
	struct max77958_dev *max77958 = irq_get_chip_data(data->irq);
	const struct max77958_irq_data *irq_data =
		irq_to_max77958_irq(max77958, data->irq);

	if (irq_data->group >= MAX77958_IRQ_GROUP_NR)
		return;

	max77958->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void max77958_irq_unmask(struct irq_data *data)
{
	struct max77958_dev *max77958 = irq_get_chip_data(data->irq);
	const struct max77958_irq_data *irq_data =
		irq_to_max77958_irq(max77958, data->irq);

	if (irq_data->group >= MAX77958_IRQ_GROUP_NR)
		return;

	max77958->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static void max77958_irq_disable(struct irq_data *data)
{
	max77958_irq_mask(data);
}

static struct irq_chip max77958_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= max77958_irq_lock,
	.irq_bus_sync_unlock	= max77958_irq_sync_unlock,
	.irq_mask		= max77958_irq_mask,
	.irq_unmask		= max77958_irq_unmask,
	.irq_disable            = max77958_irq_disable,
};


static irqreturn_t max77958_irq_thread(int irq, void *data)
{
	struct max77958_dev *max77958 = data;
	u8 irq_reg[MAX77958_IRQ_GROUP_NR] = {0};
	u8 dump_reg[10] = {0, };
	int i, rc;

	pr_info("%s: irq gpio pre-state(0x%02x)\n", __func__,
			gpio_get_value(max77958->irq_gpio));
	if (!max77958->boot_complete)
		return IRQ_HANDLED;

	pr_info("irq[%d] %d/%d/%d",
			irq, max77958->irq,
			max77958->irq_base,
			max77958->irq_gpio);

	rc = max77958_bulk_read(max77958->i2c,
			REG_UIC_INT, 4, &irq_reg[USBC_INT]);


	rc = max77958_bulk_read(max77958->i2c,
			REG_USBC_STATUS1,
			8, dump_reg);

	pr_info("[max77958] info [%x] [%x] [%x_%x]",
			max77958->HW_Revision, max77958->Device_Revision,
			max77958->FW_Revision, max77958->FW_Minor_Revision);

	pr_info("[max77958] irq_reg[USBC_INT] : [%x], irq_reg[CC_INT] : %x,\n",
			irq_reg[USBC_INT], irq_reg[CC_INT]);
	pr_info("[max77958] irq_reg[PD_INT]: %x, irq_reg[VDM_INT] : %x\n",
			irq_reg[PD_INT], irq_reg[ACTION_INT]);
	pr_info("[max77958] dump_reg, %x, %x, %x, %x, %x, %x, %x, %x\n",
			dump_reg[0], dump_reg[1], dump_reg[2], dump_reg[3],
			dump_reg[4], dump_reg[5], dump_reg[6], dump_reg[7]);

	/* Apply masking */
	for (i = 0; i < MAX77958_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~max77958->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < MAX77958_IRQ_NR ; i++) {
		if (irq_reg[max77958_irqs[i].group] & max77958_irqs[i].mask)
			handle_nested_irq(max77958->irq_base + i);
	}
	return IRQ_HANDLED;
}

int max77958_irq_init(struct max77958_dev *max77958)
{
	int i;
	int ret;

	if (!max77958->irq_gpio) {
		dev_warn(max77958->dev, "No interrupt specified.\n");
		max77958->irq_base = 0;
		return 0;
	}

	if (!max77958->irq_base) {
		dev_err(max77958->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&max77958->irqlock);

	max77958->irq = gpio_to_irq(max77958->irq_gpio);

	ret = gpio_request(max77958->irq_gpio, "max77958_int");
	if (ret) {
		dev_err(max77958->dev, "%s - failed to request irq\n",
			__func__);
		return ret;
	}

	ret = gpio_direction_input(max77958->irq_gpio);
	if (ret)
		dev_err(max77958->dev, "%s - failed to set irq as input\n",
			__func__);

	gpio_free(max77958->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < MAX77958_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;
		/* IRQ 1:MASK 0:NOT MASK */
		max77958->irq_masks_cur[i] = 0xff;
		max77958->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(max77958, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (max77958_mask_reg[i] == MAX77958_REG_INVALID)
			continue;
		max77958_write_reg(i2c, max77958_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < MAX77958_IRQ_NR; i++) {
		int cur_irq;

		cur_irq = i + max77958->irq_base;

		pr_info("cur_irq=%d", cur_irq);
		irq_set_chip_data(cur_irq, max77958);
		irq_set_chip_and_handler(cur_irq, &max77958_irq_chip,
				handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(max77958->irq, NULL, max77958_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"max77958-irq", max77958);
	if (ret) {
		dev_err(max77958->dev, "Failed to request IRQ %d: %d\n",
				max77958->irq, ret);
		return ret;
	}

	return 0;
}

void max77958_irq_exit(struct max77958_dev *max77958)
{
	if (max77958->irq)
		free_irq(max77958->irq, max77958);
}
