/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#define USB_DEV_MAX 4

#define MX25_USB_PHY_CTRL_OFFSET	0x08
#define MX25_BM_EXTERNAL_VBUS_DIVIDER	BIT(23)

#define MX53_USB_OTG_PHY_CTRL_0_OFFSET	0x08
#define MX53_USB_UH2_CTRL_OFFSET	0x14
#define MX53_USB_UH3_CTRL_OFFSET	0x18
#define MX53_BM_OVER_CUR_DIS_H1		BIT(5)
#define MX53_BM_OVER_CUR_DIS_OTG	BIT(8)
#define MX53_BM_OVER_CUR_DIS_UHx	BIT(30)

#define MX6_BM_OVER_CUR_DIS		BIT(7)
#define MX6_BM_ID_WAKEUP		BIT(16)
#define MX6_BM_VBUS_WAKEUP		BIT(17)
#define MX6_BM_WAKEUP_ENABLE		BIT(10)
#define MX6_BM_WAKEUP_INTR		BIT(31)

static int usbmisc_imx25_post(struct ci13xxx_imx_data *data)
{
	void __iomem *reg;
	unsigned long flags;
	u32 val;

	if (!data->non_core_base_addr)
		return -EINVAL;

	reg = data->non_core_base_addr + MX25_USB_PHY_CTRL_OFFSET;

	if (data->evdo) {
		spin_lock_irqsave(&data->lock, flags);
		val = readl(reg);
		writel(val | MX25_BM_EXTERNAL_VBUS_DIVIDER, reg);
		spin_unlock_irqrestore(&data->lock, flags);
		usleep_range(5000, 10000); /* needed to stabilize voltage */
	}

	return 0;
}

static int usbmisc_imx53_init(struct ci13xxx_imx_data *data)
{
	void __iomem *reg = NULL;
	unsigned long flags;
	u32 val = 0;

	if (!data->non_core_base_addr || data->index < 0)
		return -EINVAL;

	if (data->disable_oc) {
		spin_lock_irqsave(&data->lock, flags);
		switch (data->index) {
		case 0:
			reg = data->non_core_base_addr +
				MX53_USB_OTG_PHY_CTRL_0_OFFSET;
			val = readl(reg) | MX53_BM_OVER_CUR_DIS_OTG;
			break;
		case 1:
			reg = data->non_core_base_addr +
				MX53_USB_OTG_PHY_CTRL_0_OFFSET;
			val = readl(reg) | MX53_BM_OVER_CUR_DIS_H1;
			break;
		case 2:
			reg = data->non_core_base_addr +
				MX53_USB_UH2_CTRL_OFFSET;
			val = readl(reg) | MX53_BM_OVER_CUR_DIS_UHx;
			break;
		case 3:
			reg = data->non_core_base_addr +
				MX53_USB_UH3_CTRL_OFFSET;
			val = readl(reg) | MX53_BM_OVER_CUR_DIS_UHx;
			break;
		}
		if (reg && val)
			writel(val, reg);
		spin_unlock_irqrestore(&data->lock, flags);
	}

	return 0;
}

static int usbmisc_imx6q_init(struct ci13xxx_imx_data *data)
{
	unsigned long flags;
	u32 reg;

	if (!data->non_core_base_addr || data->index < 0)
		return -EINVAL;

	spin_lock_irqsave(&data->lock, flags);

	if (data->disable_oc) {
		reg = readl(data->non_core_base_addr + data->index * 4);
		writel(reg | MX6_BM_OVER_CUR_DIS,
			data->non_core_base_addr + data->index * 4);
	}

	/* Disable wakeup at initialization */
	reg = readl(data->non_core_base_addr + data->index * 4);
	switch (data->index) {
	case 0:
		reg &= ~(MX6_BM_ID_WAKEUP | MX6_BM_VBUS_WAKEUP
				| MX6_BM_WAKEUP_ENABLE);
		break;
	case 1:
	case 2:
	case 3:
		reg &= ~MX6_BM_WAKEUP_ENABLE;
	}
	writel(reg, data->non_core_base_addr + data->index * 4);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int usbmisc_imx6q_wakeup(struct ci13xxx_imx_data *data,
		enum ci_usb_wakeup_events wakeup_event)
{
	unsigned long flags;
	u32 reg;

	if (!data->non_core_base_addr || data->index < 0)
		return -EINVAL;

	spin_lock_irqsave(&data->lock, flags);

	reg = readl(data->non_core_base_addr + data->index * 4);
	switch (wakeup_event) {
	case CI_USB_WAKEUP_EVENT_GADGET:
		reg |= MX6_BM_VBUS_WAKEUP | MX6_BM_WAKEUP_ENABLE;
		break;
	case CI_USB_WAKEUP_EVENT_HOST:
		reg |= MX6_BM_WAKEUP_ENABLE;
		break;
	case CI_USB_WAKEUP_EVENT_OTG:
		reg |= MX6_BM_ID_WAKEUP | MX6_BM_VBUS_WAKEUP
			| MX6_BM_WAKEUP_ENABLE;
		break;
	case CI_USB_WAKEUP_EVENT_NONE:
		switch (data->index) {
		case 0:
			reg &= ~(MX6_BM_ID_WAKEUP | MX6_BM_VBUS_WAKEUP
					| MX6_BM_WAKEUP_ENABLE);
			break;
		case 1:
		case 2:
		case 3:
			reg &= ~MX6_BM_WAKEUP_ENABLE;
		}
		break;
	default:
		printk(KERN_ERR "error wakeup event\n");
		return -EINVAL;
	}

	writel(reg, data->non_core_base_addr + data->index * 4);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int usbmisc_imx6q_is_wakeup_interrupt(struct ci13xxx_imx_data *data)
{
	u32 reg;

	if (!data->non_core_base_addr || data->index < 0)
		return -EINVAL;

	reg = readl(data->non_core_base_addr + data->index * 4);
	if (reg & MX6_BM_WAKEUP_INTR)
		return IMX_WAKEUP_INTR_PENDING;

	return 0;

}

static const struct usbmisc_ops imx25_usbmisc_ops = {
	.post = usbmisc_imx25_post,
};

static const struct usbmisc_ops imx53_usbmisc_ops = {
	.init = usbmisc_imx53_init,
};

static const struct usbmisc_ops imx6q_usbmisc_ops = {
	.init = usbmisc_imx6q_init,
	.set_wakeup = usbmisc_imx6q_wakeup,
	.is_wakeup_intr = usbmisc_imx6q_is_wakeup_interrupt,
};
