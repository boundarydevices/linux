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

void max77958_notify_dr_status(struct max77958_usbc_platform_data
	*usbpd_data, uint8_t attach)
{
	struct max77958_pd_data *pd_data = usbpd_data->pd_data;

	pr_info("%s",	attach ? "ATTACHED":"DETACHED");
	if (usbpd_data->power_role == CC_SNK) {
		pr_info("Power Role: CC_SNK");
	} else if (usbpd_data->power_role == CC_SRC) {
		pr_info("Power Role: CC_SRC");
	} else {
		pr_info("Power Role: [%x] ",
				usbpd_data->power_role);
	}
	pr_info("Data Role: %s",
			pd_data->current_dr ? "DFP":"UFP");

	if (attach) {
		if (usbpd_data->current_wtrstat == WATER) {
			pr_info("Blocking by WATER STATE");
			return;
		}
		if (pd_data->current_dr == UFP) {
			pr_info("Turn off the USB HOST");
		} else if (pd_data->current_dr == DFP) {
			pr_info("Turn on the USB HOST");
		} else {
			pr_info("Data Role Unknown (%d) no action",
					pd_data->current_dr);
		}
	} else {
		pr_info("Turn off the USB HOST");
	}
}

static irqreturn_t max77958_vconncop_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;

	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS1, &cc_data->cc_status1);
	cc_data->vconnocp = (cc_data->cc_status1 & BIT_VCONNOCPI)
		>> FFS(BIT_VCONNOCPI);
	pr_info("VCONNOCPI : [%d]", cc_data->vconnocp);

	return IRQ_HANDLED;
}

static irqreturn_t max77958_vsafe0v_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 ccpinstat = 0;
	u8 connstat = 0;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_BC_STATUS, &cc_data->bc_status);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS1, &cc_data->cc_status1);
	ccpinstat = (cc_data->cc_status0 & BIT_CCPinStat)
		>> FFS(BIT_CCPinStat);
	cc_data->vsafe0v = (cc_data->cc_status1 & BIT_VSAFE0V)
		>> FFS(BIT_VSAFE0V);
	connstat = (cc_data->cc_status1 & BIT_WtrStat)
		>> FFS(BIT_WtrStat);

	pr_info("VSAFE0V : [%d], REG_BC_STATUS : %x,",
			cc_data->vsafe0v,
			cc_data->bc_status);
	pr_info("REG_CC_STATUS0 : %x, REG_CC_STATUS1 : %x",
			cc_data->cc_status0,
			cc_data->cc_status1);
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t max77958_water_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 waterstat = 0;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS1, &cc_data->cc_status1);
	waterstat = (cc_data->cc_status1 & BIT_WtrStat)
		>> FFS(BIT_WtrStat);

	switch (waterstat) {
	case DRY:
		pr_info("== WATER RUN-DRY DETECT ==");
		if (usbc_data->current_wtrstat != DRY) {
			usbc_data->prev_wtrstat = usbc_data->current_wtrstat;
			usbc_data->current_wtrstat = DRY;
		}
		break;

	case WATER:
		pr_info("== WATER DETECT ==");

		if (usbc_data->current_wtrstat != WATER) {
			usbc_data->prev_wtrstat = usbc_data->current_wtrstat;
			usbc_data->current_wtrstat = WATER;
		}
		break;
	default:
		break;

	}

	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t max77958_ccpinstat_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 ccpinstat = 0;

	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	ccpinstat = (cc_data->cc_status0 & BIT_CCPinStat)
		>> FFS(BIT_CCPinStat);

	switch (ccpinstat) {
	case NO_DETERMINATION:
		pr_info("CCPINSTAT : [NO_DETERMINATION]");
		break;
	case CC1_ACTIVE:
		pr_info("CCPINSTAT : [CC1_ACTIVE]");
		break;

	case CC2_ACTVIE:
		pr_info("CCPINSTAT : [CC2_ACTIVE]");
		break;
	default:
		pr_info("CCPINSTAT [%d]", ccpinstat);
		break;

	}
	cc_data->ccpinstat = ccpinstat;

	return IRQ_HANDLED;
}

static irqreturn_t max77958_ccistat_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 ccistat = 0;

	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	ccistat = (cc_data->cc_status0 & BIT_CCIStat) >> FFS(BIT_CCIStat);
	switch (ccistat) {
	case CCISTAT_NONE:
		pr_info("Not in UFP");
		break;

	case CCISTAT_500MA:
		pr_info("VBUS CURRENT : 500mA!");
		break;

	case CCISTAT_1500MA:
		pr_info("VBUS CURRENT : 1.5A!");
		break;

	case CCISTAT_3000MA:
		pr_info("VBUS CURRENT : 3.0A!");
		break;

	default:
		pr_info("CCINSTAT(Never Call this routine) !");
		break;

	}
	cc_data->ccistat = ccistat;
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	max77958_notify_cci_vbus_current(usbc_data);

	return IRQ_HANDLED;
}


static irqreturn_t max77958_ccvnstat_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 ccvcnstat = 0;

	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	ccvcnstat = (cc_data->cc_status0 & BIT_CCVcnStat) >> FFS(BIT_CCVcnStat);

	switch (ccvcnstat) {
	case 0:
		pr_info("Vconn Disabled");
		if (cc_data->current_vcon != OFF) {
			cc_data->previous_vcon = cc_data->current_vcon;
			cc_data->current_vcon = OFF;
		}
		break;

	case 1:
		pr_info("Vconn Enabled");
		if (cc_data->current_vcon != ON) {
			cc_data->previous_vcon = cc_data->current_vcon;
			cc_data->current_vcon = ON;
		}
		break;

	default:
		pr_info("ccvnstat(Never Call this routine) !");
		break;

	}
	cc_data->ccvcnstat = ccvcnstat;
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);


	return IRQ_HANDLED;
}

static void max77958_ccstat_irq_handler(void *data, int irq)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
#ifdef CONFIG_MAX77960_CHARGER
	struct power_supply *psy_charger;
	union power_supply_propval val;
#endif
	u8 ccstat = 0;
	int prev_power_role = usbc_data->power_role;

	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);
	ccstat =  (cc_data->cc_status0 & BIT_CCStat) >> FFS(BIT_CCStat);

	switch (ccstat) {
	case CC_NO_CON:
		pr_info("CCSTAT : CC_NO_CONNECTION");
		max77958_clear_apcmd_queue(usbc_data);
		usbc_data->pd_data->cc_status = CC_NO_CON;
		usbc_data->power_role = CC_NO_CON;
		usbc_data->cc_data->current_pr = CC_NO_CON;
		usbc_data->pd_pr_swap = CC_NO_CON;
		usbc_data->pd_data->current_dr = UFP;
		usbc_data->cc_data->current_vcon = OFF;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		max77958_vbus_turn_on_ctrl(usbc_data, OFF);
		max77958_notify_dr_status(usbc_data, 0);
		usbc_data->connected_device = 0;
		usbc_data->pd_data->previous_dr = UNKNOWN_STATE;
#ifdef CONFIG_MAX77960_CHARGER
		psy_charger =
			power_supply_get_by_name("max77960-charger");
		if (psy_charger) {
			val.intval = 0;
			psy_charger->desc->set_property(psy_charger,
					POWER_SUPPLY_PROP_CHARGING_ENABLED,
					&val);
		} else {
			pr_err("%s: Fail to get psy charger\n",
				__func__);
		}
#endif
		break;
	case CC_SNK:
		pr_info("CCSTAT : CC_SINK");
		usbc_data->pd_data->cc_status = CC_SNK;
		usbc_data->power_role = CC_SNK;
		if (cc_data->current_pr != SNK) {
			cc_data->previous_pr = cc_data->current_pr;
			cc_data->current_pr = SNK;
			if (prev_power_role == CC_SRC)
				max77958_vbus_turn_on_ctrl(usbc_data,
					OFF);
		}
#ifdef CONFIG_MAX77960_CHARGER
		psy_charger =
			power_supply_get_by_name("max77960-charger");
		if (psy_charger) {
			val.intval = 1;
			psy_charger->desc->set_property(psy_charger,
					POWER_SUPPLY_PROP_CHARGING_ENABLED,
					&val);
		} else {
			pr_err("%s: Fail to get psy charger\n",
				__func__);
		}
#endif
		usbc_data->connected_device = 1;
		break;
	case CC_SRC:
		pr_info("CCSTAT : CC_SOURCE");
		usbc_data->pd_data->cc_status = CC_SRC;
		usbc_data->power_role = CC_SRC;
		if (cc_data->current_pr != SRC) {
			cc_data->previous_pr = cc_data->current_pr;
			cc_data->current_pr = SRC;
			max77958_vbus_turn_on_ctrl(usbc_data, ON);
		}
		usbc_data->connected_device = 1;
		break;
	case CC_AUD_ACC:
		pr_info("CCSTAT : CC_AUDIO_ACCESSORY");
		usbc_data->pd_data->cc_status = CC_AUD_ACC;
		usbc_data->power_role = CC_AUD_ACC;
		usbc_data->connected_device = 1;
		break;
	case CC_DBG_SRC:
		pr_info("CCSTAT : CC_DEBUG_ACCESSORY");
		usbc_data->pd_data->cc_status = CC_DBG_SRC;
		usbc_data->power_role = CC_DBG_SRC;
		usbc_data->connected_device = 1;
		break;
	case CC_ERR:
		pr_info("CCSTAT : CC_ERROR_RECOVERY");
		usbc_data->pd_data->cc_status = CC_ERR;
		usbc_data->power_role = CC_ERR;
		break;
	case CC_DISABLED:
		pr_info("CCSTAT : CC_DISABLED");
		usbc_data->pd_data->cc_status = CC_DISABLED;
		usbc_data->power_role = CC_DISABLED;
		break;
	case CC_DBG_SNK:
		pr_info("CCSTAT : CC_DEBUG_SINK");
		usbc_data->pd_data->cc_status = CC_DBG_SNK;
		usbc_data->power_role = CC_DBG_SNK;
		usbc_data->connected_device = 1;
		break;
	default:
		pr_info("CCSTAT : CC_RFU");
		break;
	}
}

static irqreturn_t max77958_ccstat_irq(int irq, void *data)
{
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77958_ccstat_irq_handler(data, irq);
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

int max77958_cc_init(struct max77958_usbc_platform_data *usbc_data)
{
	struct max77958_cc_data *cc_data = NULL;
	int ret;

	pr_info("IN");

	cc_data = usbc_data->cc_data;

	wake_lock_init(&cc_data->max77958_cc_wake_lock, WAKE_LOCK_SUSPEND,
			"max77958cc->wake_lock");

	cc_data->irq_vconncop = usbc_data->irq_base
		+ MAX77958_CC_IRQ_VCONNCOP_INT;
	if (cc_data->irq_vconncop) {
		ret = request_threaded_irq(cc_data->irq_vconncop,
				NULL, max77958_vconncop_irq,
				0,
				"cc-vconncop-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	cc_data->irq_vsafe0v = usbc_data->irq_base
		+ MAX77958_CC_IRQ_VSAFE0V_INT;
	if (cc_data->irq_vsafe0v) {
		ret = request_threaded_irq(cc_data->irq_vsafe0v,
				NULL, max77958_vsafe0v_irq,
				0,
				"cc-vsafe0v-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	cc_data->irq_wtr = usbc_data->irq_base + MAX77958_CC_IRQ_WTR_INT;
	if (cc_data->irq_wtr) {
		ret = request_threaded_irq(cc_data->irq_wtr,
				NULL, max77958_water_irq,
				0,
				"cc-wtr-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}
	cc_data->irq_ccpinstat = usbc_data->irq_base
		+ MAX77958_CC_IRQ_CCPINSTAT_INT;
	if (cc_data->irq_ccpinstat) {
		ret = request_threaded_irq(cc_data->irq_ccpinstat,
				NULL, max77958_ccpinstat_irq,
				0,
				"cc-ccpinstat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}
	cc_data->irq_ccistat = usbc_data->irq_base
		+ MAX77958_CC_IRQ_CCISTAT_INT;
	if (cc_data->irq_ccistat) {
		ret = request_threaded_irq(cc_data->irq_ccistat,
				NULL, max77958_ccistat_irq,
				0,
				"cc-ccistat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}
	cc_data->irq_ccvcnstat = usbc_data->irq_base
		+ MAX77958_CC_IRQ_CCVCNSTAT_INT;
	if (cc_data->irq_ccvcnstat) {
		ret = request_threaded_irq(cc_data->irq_ccvcnstat,
				NULL, max77958_ccvnstat_irq,
				0,
				"cc-ccvcnstat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}
	cc_data->irq_ccstat = usbc_data->irq_base + MAX77958_CC_IRQ_CCSTAT_INT;
	if (cc_data->irq_ccstat) {
		ret = request_threaded_irq(cc_data->irq_ccstat,
				NULL, max77958_ccstat_irq,
				0,
				"cc-ccstat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS1, &cc_data->cc_status1);
	usbc_data->current_wtrstat = (cc_data->cc_status1 & BIT_WtrStat)
		>> FFS(BIT_WtrStat);
	pr_info("WATER STATE : [%s]\n",
		usbc_data->current_wtrstat ? "WATER" : "DRY");

	pr_info("OUT");

	return 0;

err_irq:
	kfree(cc_data);
	return ret;

}

