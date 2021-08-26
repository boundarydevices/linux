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
#include <linux/mfd/max77958-private.h>
#include <linux/platform_device.h>
#include <linux/usbc/max77958-usbc.h>


void max77958_bc12_get_vadc(u8 vbadc)
{
	switch (vbadc) {
	case 0:
		pr_info(" VBUS < 3.5V");
		break;
	case 1:
		pr_info(" 3.5V <=  VBUS < 4.5V");
		break;
	case 2:
		pr_info(" 4.5V <=  VBUS < 5.5V ");
		break;
	case 3:
		pr_info(" 5.5V <=  VBUS < 6.5V");
		break;
	case 4:
		pr_info(" 6.5V <=  VBUS < 7.5V");
		break;
	case 5:
		pr_info(" 7.5V <= VBUS < 8.5V");
		break;
	case 6:
		pr_info(" 8.5V <= VBUS < 9.5V");
		break;
	case 7:
		pr_info(" 9.5V <= VBUS < 10.5V");
		break;
	case 8:
		pr_info(" 10.5V <= VBUS < 11.5V");
		break;
	case 9:
		pr_info(" 11.5V <= VBUS < 12.5V");
		break;
	case 10:
		pr_info(" 12.5V <= VBUS < 13.5V");
		break;
	case 11:
		pr_info(" 13.5V <= VBUS < 14.5V");
		break;
	case 12:
		pr_info(" 14.5V <= VBUS < 15.5V");
		break;
	case 13:
		pr_info(" 15.5V <= VBUS < 16.5V");
		break;
	case 14:
		pr_info(" 16.5V <= VBUS < 17.5V");
		break;
	case 15:
		pr_info(" 17.5V <= VBUS < 18.5V");
		break;
	case 16:
		pr_info(" 18.5V <= VBUS < 19.5V");
		break;
	case 17:
		pr_info(" 19.5V <= VBUS < 20.5V");
		break;
	case 18:
		pr_info(" 20.5V <= VBUS < 21.5V");
		break;
	case 19:
		pr_info(" 21.5V <= VBUS < 22.5V");
		break;
	case 20:
		pr_info(" 22.5V <= VBUS < 23.5V");
		break;
	case 21:
		pr_info(" 23.5V <= VBUS < 24.5V");
		break;
	case 22:
		pr_info(" 24.5V <= VBUS < 25.5V");
		break;
	case 23:
		pr_info(" 25.5V <= VBUS < 26.5V");
		break;
	case 24:
		pr_info(" 26.5V <= VBUS < 27.5V");
		break;
	case 25:
		pr_info(" 27.5V <= VBUS");
		break;
	default:
		pr_info(" Reserved ");
		break;

	};
}


int max77958_bc12_set_charger(struct max77958_usbc_platform_data *usbc_data)
{
	struct max77958_bc12_data *bc12_data = usbc_data->bc12_data;
	struct power_supply *psy_charger = usbc_data->psy_charger;
	int rc = 0;

	pr_info(" BIT_ChgTyp = %02Xh, BIT_PrChgTyp = %02Xh",
			bc12_data->chg_type, bc12_data->pr_chg_type);

	if (psy_charger != NULL) {
		union power_supply_propval value;
		switch (bc12_data->chg_type) {
		case CHGTYP_NOTHING:
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case CHGTYP_USB_SDP:
		case CHGTYP_CDP:
			value.intval = POWER_SUPPLY_TYPE_USB;
			break;
		case CHGTYP_DCP:
			value.intval = POWER_SUPPLY_TYPE_MAINS;
			break;
		default:
			value.intval = -EINVAL;
			break;
		}
		psy_charger->desc->set_property(psy_charger,
				POWER_SUPPLY_PROP_ONLINE, &value);
	}

	return rc;
}

static irqreturn_t max77958_vbadc_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_bc12_data *bc12_data = usbc_data->bc12_data;
	u8 vbadc = 0;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_USBC_STATUS1,
		&bc12_data->usbc_status1);
	vbadc = (bc12_data->usbc_status1 & BIT_VBADC) >> FFS(BIT_VBADC);
	max77958_bc12_get_vadc(vbadc);
	bc12_data->vbadc = vbadc;
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}



static irqreturn_t max77958_chgtype_irq(int irq, void *data)
{

	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_bc12_data *bc12_data = usbc_data->bc12_data;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);

	max77958_read_reg(usbc_data->i2c, REG_BC_STATUS, &bc12_data->bc_status);

	bc12_data->chg_type = (bc12_data->bc_status & BIT_ChgTyp)
		>> FFS(BIT_ChgTyp);

	bc12_data->pr_chg_type = (bc12_data->bc_status & BIT_PrChgTyp)
		>> FFS(BIT_PrChgTyp);

	max77958_bc12_set_charger(usbc_data);

	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t max77958_dcdtmo_irq(int irq, void *data)
{

	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_bc12_data *bc12_data = usbc_data->bc12_data;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_BC_STATUS, &bc12_data->bc_status);

	pr_info(" BIT_DCDTmoI occured");
	bc12_data->dcdtmo = (bc12_data->bc_status & BIT_DCDTmo)
		>> FFS(BIT_DCDTmo);

	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}


static irqreturn_t max77958_vbusdet_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_bc12_data *bc12_data = usbc_data->bc12_data;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_BC_STATUS, &bc12_data->bc_status);

	if ((bc12_data->bc_status & BIT_VBUSDet) == BIT_VBUSDet) {
		pr_info(" VBUS > VVBDET");
		bc12_data->vbusdet = 1;
	} else {
		pr_info(" VBUS < VVBDET");
		bc12_data->vbusdet = 0;
	}
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}

int max77958_bc12_init(struct max77958_usbc_platform_data *usbc_data)
{
	struct max77958_bc12_data *bc12_data = NULL;
	int ret;

	pr_info(" IN");

	bc12_data = usbc_data->bc12_data;

	wake_lock_init(&bc12_data->max77958_bc12_wake_lock, WAKE_LOCK_SUSPEND,
			"max77958pd->wake_lock");

	bc12_data->irq_vbadc = usbc_data->irq_base
		+ MAX77958_USBC_IRQ_VBADC_INT;
	if (bc12_data->irq_vbadc) {
		ret = request_threaded_irq(bc12_data->irq_vbadc,
				NULL, max77958_vbadc_irq,
				0,
				"usbc-vbadc-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	bc12_data->irq_vbusdet = usbc_data->irq_base
		+ MAX77958_USBC_IRQ_VBUS_INT;
	if (bc12_data->irq_vbusdet) {
		ret = request_threaded_irq(bc12_data->irq_vbusdet,
				NULL, max77958_vbusdet_irq,
				0,
				"bc-vbusdet-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	bc12_data->irq_dcdtmo = usbc_data->irq_base
		+ MAX77958_USBC_IRQ_DCD_INT;
	if (bc12_data->irq_dcdtmo) {
		ret = request_threaded_irq(bc12_data->irq_dcdtmo,
				NULL, max77958_dcdtmo_irq,
				0,
				"bc-dcdtmo-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	bc12_data->irq_chgtype = usbc_data->irq_base
		+ MAX77958_USBC_IRQ_CHGT_INT;
	if (bc12_data->irq_chgtype) {
		ret = request_threaded_irq(bc12_data->irq_chgtype,
				NULL, max77958_chgtype_irq,
				0,
				"bc-chgtype-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	pr_info(" OUT");
	return 0;

err_irq:
	kfree(bc12_data);
	return ret;
}
