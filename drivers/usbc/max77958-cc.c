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
#include <linux/usb/typec.h>
#include <linux/usbc/max77958-usbc.h>

static enum usb_role max77958_get_attached_state(struct max77958_usbc_platform_data *usbc)
{
	return usbc->pd_data->current_dr;
}

static void max77958_close_DN1_DP2(struct max77958_usbc_platform_data *usbc)
{
	struct max77958_apcmd_node *node = max77958_alloc_apcmd_data();
	unsigned char *cmd;

	/*
	 * enable switches DP/DP2, DN/DN1
	 * i2c mw 25 21.1 06;i2c mw 25 22.1 09;i2c mw 25 41.1 00;
	 */
	if (node) {
		cmd = node->cmd_data.cmd;
		cmd[0] = OPCODE_CONTROL1_W;
		cmd[1] = 0x09;
		node->cmd_data.cmd_length = 0x2;
		max77958_queue_apcmd(usbc, node);
	}
}

static int max77958_dr_set(struct typec_port *port, enum typec_data_role typec_role)
{
	struct max77958_usbc_platform_data *usbc = typec_get_drvdata(port);
	enum usb_role role, cur_role;
	int ret = 0;
	u8 val;

	ret = max77958_read_reg(usbc->i2c, REG_PD_STATUS1, &val);
	if (ret < 0)
		return ret;
	cur_role = max77958_get_attached_state(usbc);
	if (cur_role == USB_ROLE_NONE)
		return 0;
	role = (typec_role == TYPEC_HOST) ? USB_ROLE_HOST : USB_ROLE_DEVICE;

	if (role != cur_role) {
		struct max77958_apcmd_node *node = max77958_alloc_apcmd_data();
		unsigned char *cmd;

		if (node) {
			cmd = node->cmd_data.cmd;
			cmd[0] = OPCODE_SWAP_REQ;
			cmd[1] = 0x01;
			node->cmd_data.cmd_length = 0x2;
			max77958_queue_apcmd(usbc, node);
		}
	}

	usb_role_switch_set_role(usbc->role_sw, role);
	typec_set_data_role(usbc->port, typec_role);
	return ret;
}

static const struct typec_operations max77958_ops = {
	.dr_set = max77958_dr_set
};

static void max77958_set_role(struct max77958_usbc_platform_data *usbc)
{
	enum usb_role role =  max77958_get_attached_state(usbc);

	usb_role_switch_set_role(usbc->role_sw, role);

	switch (role) {
	case USB_ROLE_HOST:
		pr_info("%s: HOST\n", __func__);
		typec_set_data_role(usbc->port, TYPEC_HOST);
		max77958_close_DN1_DP2(usbc);
		break;
	case USB_ROLE_DEVICE:
		pr_info("%s: DEVICE\n", __func__);
		typec_set_data_role(usbc->port, TYPEC_DEVICE);
		break;
	default:
		break;
	}
}

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
			(pd_data->current_dr == USB_ROLE_HOST) ? "HOST" :
			(pd_data->current_dr == USB_ROLE_DEVICE) ? "DEVICE" :
			"NONE");

	if (attach && (usbpd_data->current_wtrstat == WTR_WATER)) {
		pr_info("Blocking by WATER STATE");
		return;
	}
	max77958_set_role(usbpd_data);
}

static irqreturn_t max77958_vconncop_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS1, &cc_data->cc_status1);
	cc_data->vconnocp = (cc_data->cc_status1 & BIT_VCONNOCPI)
		>> FFS(BIT_VCONNOCPI);

	pr_debug("%s: exit %d\n", __func__, cc_data->vconnocp);
	return IRQ_HANDLED;
}

static irqreturn_t max77958_vsafe0v_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_BC_STATUS, &cc_data->bc_status);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS1, &cc_data->cc_status1);
	cc_data->vsafe0v = (cc_data->cc_status1 & BIT_VSAFE0V)
		>> FFS(BIT_VSAFE0V);

	pr_debug("%s: exit %d, bc_status=%02x cc_status0=%02x cc_status1=%02x\n",
		__func__, cc_data->vsafe0v,
		cc_data->bc_status, cc_data->cc_status0, cc_data->cc_status1);
	return IRQ_HANDLED;
}

static irqreturn_t max77958_water_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 waterstat = 0;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS1, &cc_data->cc_status1);
	waterstat = (cc_data->cc_status1 & BIT_WtrStat)
		>> FFS(BIT_WtrStat);

	if (usbc_data->current_wtrstat != waterstat) {
		if ((waterstat == WTR_DRY) || (waterstat == WTR_WATER)) {
			usbc_data->prev_wtrstat = usbc_data->current_wtrstat;
			usbc_data->current_wtrstat = waterstat;
		}
	}
	pr_debug("%s: exit %d %s\n", __func__, waterstat,
		(waterstat == WTR_DRY) ? "dry" :
		(waterstat == WTR_WATER) ? "water" : "unknown");
	return IRQ_HANDLED;
}

static irqreturn_t max77958_ccpinstat_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 ccpinstat = 0;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);

	ccpinstat = (cc_data->cc_status0 & BIT_CCPinStat)
		>> FFS(BIT_CCPinStat);
#ifdef DEBUG
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
#endif
	cc_data->ccpinstat = ccpinstat;
	pr_debug("%s: exit %d, cc_status0=%02x\n", __func__,
		cc_data->ccpinstat, cc_data->cc_status0);
	return IRQ_HANDLED;
}

static irqreturn_t max77958_ccistat_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 ccistat = 0;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);
	ccistat = (cc_data->cc_status0 & BIT_CCIStat) >> FFS(BIT_CCIStat);
#ifdef DEBUG
	switch (ccistat) {
	case CCISTAT_NONE:
		pr_info("Not in USB_ROLE_DEVICE");
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
#endif
	cc_data->ccistat = ccistat;
	max77958_notify_cci_vbus_current(usbc_data);

	pr_debug("%s: exit %d, cc_status0=%02x\n", __func__,
		cc_data->ccistat, cc_data->cc_status0);
	return IRQ_HANDLED;
}


static irqreturn_t max77958_ccvnstat_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	u8 ccvcnstat = 0;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_CC_STATUS0, &cc_data->cc_status0);

	ccvcnstat = (cc_data->cc_status0 & BIT_CCVcnStat) >> FFS(BIT_CCVcnStat);

	if (cc_data->current_vcon != ccvcnstat) {
		if ((ccvcnstat == VCR_OFF) || (ccvcnstat == VCR_ON)) {
			cc_data->previous_vcon = cc_data->current_vcon;
			cc_data->current_vcon = ccvcnstat;
		}
	}
	cc_data->ccvcnstat = ccvcnstat;

	pr_debug("%s: exit %d %s\n", __func__, ccvcnstat,
		(ccvcnstat == VCR_OFF) ? "off" :
		(ccvcnstat == VCR_ON) ? "on" : "unknown");
	return IRQ_HANDLED;
}

static irqreturn_t max77958_ccstat_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_cc_data *cc_data = usbc_data->cc_data;
	struct power_supply *psy_charger = usbc_data->psy_charger;
	union power_supply_propval val;
	u8 ccstat = 0;
	int prev_power_role = usbc_data->power_role;

	pr_debug("%s: enter irq%d\n", __func__, irq);
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
		usbc_data->pd_data->current_dr = USB_ROLE_NONE;
		usbc_data->cc_data->current_vcon = VCR_OFF;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		max77958_vbus_turn_on_ctrl(usbc_data, 0);
		max77958_notify_dr_status(usbc_data, 0);
		usbc_data->connected_device = 0;
		usbc_data->pd_data->previous_dr = USB_ROLE_NONE;
		if (psy_charger) {
			val.intval = 0;
			psy_charger->desc->set_property(psy_charger,
					POWER_SUPPLY_PROP_CHARGING_ENABLED,
					&val);
		} else {
			pr_err("%s: Fail to get psy charger\n",
				__func__);
		}
		break;
	case CC_SNK:
		pr_info("CCSTAT : CC_SINK");
		usbc_data->pd_data->cc_status = CC_SNK;
		usbc_data->power_role = CC_SNK;
		if (cc_data->current_pr != CC_SNK) {
			cc_data->previous_pr = cc_data->current_pr;
			cc_data->current_pr = CC_SNK;
			if (prev_power_role == CC_SRC)
				max77958_vbus_turn_on_ctrl(usbc_data, 0);
		}
		if (psy_charger) {
			val.intval = 1;
			psy_charger->desc->set_property(psy_charger,
					POWER_SUPPLY_PROP_CHARGING_ENABLED,
					&val);
		} else {
			pr_err("%s: Fail to get psy charger\n",
				__func__);
		}
		usbc_data->connected_device = 1;
		break;
	case CC_SRC:
		pr_info("CCSTAT : CC_SOURCE");
		usbc_data->pd_data->cc_status = CC_SRC;
		usbc_data->power_role = CC_SRC;
		if (cc_data->current_pr != CC_SRC) {
			cc_data->previous_pr = cc_data->current_pr;
			cc_data->current_pr = CC_SRC;
			max77958_vbus_turn_on_ctrl(usbc_data, 1);
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
	pr_debug("%s: exit cc_status0=%02x\n", __func__, cc_data->cc_status0);
	return IRQ_HANDLED;
}

int max77958_cc_init(struct max77958_usbc_platform_data *usbc_data)
{
	struct typec_capability typec_cap = { };
	struct fwnode_handle *connector, *ep;
	struct max77958_cc_data *cc_data = NULL;
	struct device *i2c_dev = &usbc_data->i2c->dev;
	int ret;

	pr_debug("%s: enter\n", __func__);

	cc_data = usbc_data->cc_data;

	wake_lock_init(&cc_data->max77958_cc_wake_lock, WAKE_LOCK_SUSPEND,
			"max77958cc->wake_lock");

	ep = fwnode_graph_get_next_endpoint(dev_fwnode(i2c_dev), NULL);
	if (!ep) {
		pr_err("%s: no graph\n", __func__);
		return -ENODEV;
	}
	connector = fwnode_graph_get_remote_port_parent(ep);
	fwnode_handle_put(ep);
	if (!connector) {
		pr_err("%s: no connector\n", __func__);
		return -ENODEV;
	}
	usbc_data->role_sw = usb_role_switch_get(i2c_dev);
	if (IS_ERR(usbc_data->role_sw)) {
		ret = PTR_ERR(usbc_data->role_sw);
		pr_err("%s: no role_sw %d\n", __func__, ret);
		goto err_put_fwnode;
	}

	typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	typec_cap.driver_data = usbc_data;
	typec_cap.type = TYPEC_PORT_DRP;
	typec_cap.data = TYPEC_PORT_DRD;
	typec_cap.ops = &max77958_ops;
	typec_cap.fwnode = connector;

	usbc_data->port = typec_register_port(usbc_data->dev, &typec_cap);
	if (IS_ERR(usbc_data->port)) {
		ret = PTR_ERR(usbc_data->port);
		pr_err("%s: no role_sw %d\n", __func__, ret);
		goto err_put_role;
	}

	 max77958_set_role(usbc_data);

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
		(usbc_data->current_wtrstat == WTR_WATER) ? "WATER" : "DRY");

	pr_info("OUT");

	return 0;

err_irq:
	typec_unregister_port(usbc_data->port);
err_put_role:
	usb_role_switch_put(usbc_data->role_sw);
err_put_fwnode:
	fwnode_handle_put(connector);
	return ret;

}

