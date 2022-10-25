// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2021 BayLibre, SAS
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/semaphore.h>

#include "mtk_mt8195_hdmi_ddc.h"
#include "mtk_mt8195_hdmi_regs.h"
#include "mtk_mt8195_hdmi.h"

#define EDID_ID 0x50
#define DDC2_CLOK 572 /* BIM=208M/(v*4) = 90Khz */
#define DDC2_CLOK_EDID 832 /* BIM=208M/(v*4) = 62.5Khz */

#define SCDC_I2C_SLAVE_ADDRESS 0x54

enum sif_bit_t_hdmi {
	SIF_8_BIT_HDMI, /* /< [8 bits data address.] */
	SIF_16_BIT_HDMI, /* /< [16 bits data address.] */
};

static inline bool mtk_ddc_readbit(struct mtk_hdmi_ddc *ddc, unsigned short reg,
				   unsigned int offset)
{
	return (readl(ddc->regs + reg) & offset) ? true : false;
}

static inline unsigned int mtk_ddc_read(struct mtk_hdmi_ddc *ddc,
					unsigned short reg)
{
	return readl(ddc->regs + reg);
}

static inline void mtk_ddc_write(struct mtk_hdmi_ddc *ddc, unsigned short reg,
				 unsigned int val)
{
	writel(val, ddc->regs + reg);
}

static inline void mtk_ddc_mask(struct mtk_hdmi_ddc *ddc, unsigned int reg,
				unsigned int val, unsigned int mask)
{
	unsigned int tmp;

	tmp = readl(ddc->regs + reg) & ~mask;
	tmp |= (val & mask);
	writel(tmp, ddc->regs + reg);
}

static void hdmi_ddc_request(struct mtk_hdmi_ddc *ddc)
{
	mtk_ddc_mask(ddc, HDCP2X_POL_CTRL, HDCP2X_DIS_POLL_EN,
		     HDCP2X_DIS_POLL_EN);
}

static void DDC_WR_ONE(struct mtk_hdmi_ddc *ddc, unsigned int addr_id,
		       unsigned int offset_id, unsigned char wr_data)
{
	if (mtk_ddc_read(ddc, HDCP2X_DDCM_STATUS) & DDC_I2C_BUS_LOW) {
		mtk_ddc_mask(ddc, DDC_CTRL, (CLOCK_SCL << DDC_CMD_SHIFT),
			     DDC_CMD);
		usleep_range(250, 300);
	}
	mtk_ddc_mask(ddc, HPD_DDC_CTRL, DDC2_CLOK << DDC_DELAY_CNT_SHIFT,
		     DDC_DELAY_CNT);
	mtk_ddc_write(ddc, SI2C_CTRL, SI2C_ADDR_READ << SI2C_ADDR_SHIFT);
	mtk_ddc_mask(ddc, SI2C_CTRL, wr_data << SI2C_WDATA_SHIFT, SI2C_WDATA);
	mtk_ddc_mask(ddc, SI2C_CTRL, SI2C_WR, SI2C_WR);

	mtk_ddc_write(ddc, DDC_CTRL,
		      (SEQ_WRITE_REQ_ACK << DDC_CMD_SHIFT) +
			      (1 << DDC_DIN_CNT_SHIFT) +
			      (offset_id << DDC_OFFSET_SHIFT) + (addr_id << 1));

	usleep_range(1000, 1250);

	if ((mtk_ddc_read(ddc, HDCP2X_DDCM_STATUS) &
	     (DDC_I2C_NO_ACK | DDC_I2C_BUS_LOW))) {
		if (mtk_ddc_read(ddc, HDCP2X_DDCM_STATUS) & DDC_I2C_BUS_LOW) {
			mtk_ddc_mask(ddc, DDC_CTRL,
				     (CLOCK_SCL << DDC_CMD_SHIFT), DDC_CMD);
			usleep_range(250, 300);
		}
	}
}

static unsigned char
ddcm_read_hdmi(struct mtk_hdmi_ddc *ddc,
	       unsigned int u4_clk_div, unsigned char uc_dev, unsigned int u4_addr,
	       enum sif_bit_t_hdmi uc_addr_type, unsigned char *puc_value,
	       unsigned int u4_count)
{
	unsigned int i, temp_length, loop_counter, temp_ksvlist, device_n;
	unsigned int uc_read_count, uc_idx;
	unsigned long ddc_start_time, ddc_end_time, ddc_timeout;

	if (!puc_value || !u4_count || !u4_clk_div)
		return 0;

	uc_idx = 0;
	if (mtk_ddc_read(ddc, HDCP2X_DDCM_STATUS) & DDC_I2C_BUS_LOW) {
		mtk_ddc_mask(ddc, DDC_CTRL, (CLOCK_SCL << DDC_CMD_SHIFT),
			     DDC_CMD);
		usleep_range(250, 300);
	}

	mtk_ddc_mask(ddc, DDC_CTRL, (CLEAR_FIFO << DDC_CMD_SHIFT), DDC_CMD);

	if (u4_addr == 0x43) {
		mtk_ddc_write(ddc, DDC_CTRL,
			      (SEQ_READ_NO_ACK << DDC_CMD_SHIFT) +
				      (u4_count << DDC_DIN_CNT_SHIFT) +
				      (u4_addr << DDC_OFFSET_SHIFT) +
				      (uc_dev << 1));
		usleep_range(700, 1000);

		if (u4_count > 10)
			temp_ksvlist = 10;
		else
			temp_ksvlist = u4_count;

		for (uc_idx = 0; uc_idx < temp_ksvlist; uc_idx++) {
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_RD);
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_CONFIRM_READ);

			puc_value[uc_idx] = (mtk_ddc_read(ddc, HPD_DDC_STATUS) &
					   DDC_DATA_OUT) >>
					  DDC_DATA_OUT_SHIFT;
			usleep_range(100, 150);
		}

		if (u4_count == temp_ksvlist)
			return (uc_idx + 1);

		usleep_range(500, 600);

		if (u4_count / 5 == 3)
			device_n = 5;
		else
			device_n = 10;

		for (uc_idx = 10; uc_idx < (10 + device_n); uc_idx++) {
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_RD);
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_CONFIRM_READ);

			puc_value[uc_idx] = (mtk_ddc_read(ddc, HPD_DDC_STATUS) &
					   DDC_DATA_OUT) >>
					  DDC_DATA_OUT_SHIFT;
			usleep_range(100, 150);
		}

		if (u4_count == (10 + device_n))
			return (uc_idx + 1);

		usleep_range(500, 700);

		if (u4_count / 5 == 5)
			device_n = 5;
		else
			device_n = 10;

		for (uc_idx = 20; uc_idx < (20 + device_n); uc_idx++) {
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_RD);
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_CONFIRM_READ);

			puc_value[uc_idx] = (mtk_ddc_read(ddc, HPD_DDC_STATUS) &
					   DDC_DATA_OUT) >>
					  DDC_DATA_OUT_SHIFT;
			usleep_range(100, 150);
		}

		if (u4_count == (20 + device_n))
			return (uc_idx + 1);

		usleep_range(500, 700);

		if (u4_count / 5 == 7)
			device_n = 5;
		else
			device_n = 10;

		for (uc_idx = 30; uc_idx < (30 + device_n); uc_idx++) {
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_RD);
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_CONFIRM_READ);

			puc_value[uc_idx] = (mtk_ddc_read(ddc, HPD_DDC_STATUS) &
					   DDC_DATA_OUT) >>
					  DDC_DATA_OUT_SHIFT;
			usleep_range(100, 150);
		}

		if (u4_count == (30 + device_n))
			return (uc_idx + 1);

		usleep_range(500, 700);

		for (uc_idx = 40; uc_idx < (40 + 5); uc_idx++) {
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_RD);
			mtk_ddc_write(ddc, SI2C_CTRL,
				      (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
					      SI2C_CONFIRM_READ);

			puc_value[uc_idx] = (mtk_ddc_read(ddc, HPD_DDC_STATUS) &
					   DDC_DATA_OUT) >>
					  DDC_DATA_OUT_SHIFT;
			usleep_range(100, 150);
		}

		if (u4_count == 45)
			return (uc_idx + 1);
	} else {
		if (u4_count >= 16) {
			temp_length = 16;
			loop_counter =
				u4_count / 16 + ((u4_count % 16 == 0) ? 0 : 1);
		} else {
			temp_length = u4_count;
			loop_counter = 1;
		}
		if (uc_dev >= EDID_ID) {
			if (u4_clk_div < DDC2_CLOK_EDID)
				u4_clk_div = DDC2_CLOK_EDID;
		}
		mtk_ddc_mask(ddc, HPD_DDC_CTRL, u4_clk_div << DDC_DELAY_CNT_SHIFT,
			     DDC_DELAY_CNT);
		for (i = 0; i < loop_counter; i++) {
			if (i == (loop_counter - 1) && i != 0 &&
			    u4_count % 16)
				temp_length = u4_count % 16;

			/* EDID_ID(0x50) - 0x53 are special flow-control values */
			if ((uc_dev > EDID_ID) && (uc_dev <= 0x53)) {
				mtk_ddc_mask(ddc, SCDC_CTRL,
					     (uc_dev - EDID_ID)
						     << DDC_SEGMENT_SHIFT,
					     DDC_SEGMENT);
				mtk_ddc_write(
					ddc, DDC_CTRL,
					(ENH_READ_NO_ACK << DDC_CMD_SHIFT) +
						(temp_length
						 << DDC_DIN_CNT_SHIFT) +
						((u4_addr + i * temp_length)
						 << DDC_OFFSET_SHIFT) +
						(EDID_ID << 1));
			} else {
				mtk_ddc_write(
					ddc, DDC_CTRL,
					(SEQ_READ_NO_ACK << DDC_CMD_SHIFT) +
						(temp_length
						 << DDC_DIN_CNT_SHIFT) +
						((u4_addr + ((u4_addr == 0x43) ?
									  0 :
									  (i * 16)))
						 << DDC_OFFSET_SHIFT) +
						(uc_dev << 1));
			}
			usleep_range(5000, 5500);
			ddc_start_time = jiffies;
			ddc_timeout = temp_length + 5;
			ddc_end_time = ddc_start_time + ddc_timeout * HZ / 1000;
			while (1) {
				if ((mtk_ddc_read(ddc, HPD_DDC_STATUS) &
				     DDC_I2C_IN_PROG) == 0)
					break;

				if (time_after(jiffies, ddc_end_time)) {
					pr_info("[HDMI][DDC] error: time out\n");
					return 0;
				}
				usleep_range(1000, 1500);
			}
			if ((mtk_ddc_read(ddc, HDCP2X_DDCM_STATUS) &
			     (DDC_I2C_NO_ACK | DDC_I2C_BUS_LOW))) {
				if (mtk_ddc_read(ddc, HDCP2X_DDCM_STATUS) &
				    DDC_I2C_BUS_LOW) {
					mtk_ddc_mask(ddc, DDC_CTRL,
						     (CLOCK_SCL
						      << DDC_CMD_SHIFT),
						     DDC_CMD);
					usleep_range(250, 300);
				}
				return 0;
			}

			/* get the DDC data from FIFO */
			for (uc_idx = 0; uc_idx < temp_length; uc_idx++) {
				/* latch the FIFO output */
				mtk_ddc_write(ddc, SI2C_CTRL,
					      (SI2C_ADDR_READ
					       << SI2C_ADDR_SHIFT) +
						      SI2C_RD);

				/* read FIFO output value from DDC_STATUS */
				puc_value[i * 16 + uc_idx] =
					(mtk_ddc_read(ddc, HPD_DDC_STATUS) &
					 DDC_DATA_OUT) >>
					DDC_DATA_OUT_SHIFT;

				/* increment read address of FIFO and un-latch the FIFO */
				mtk_ddc_write(ddc, SI2C_CTRL,
					      (SI2C_ADDR_READ
					       << SI2C_ADDR_SHIFT) +
						      SI2C_CONFIRM_READ);
				/*
				 * when reading edid, if hdmi module been reset,
				 * ddc will fail and it's
				 *speed will be set to 400.
				 */
				if (((mtk_ddc_read(ddc, HPD_DDC_CTRL) >> 16) &
				     0xFFFF) < DDC2_CLOK)
					return 0;

				uc_read_count = i * 16 + uc_idx + 1;
			}
		}
		return uc_read_count;
	}
	return 0;
}

static unsigned char vddc_read(struct mtk_hdmi_ddc *ddc, unsigned int u4_clk_div,
			      unsigned char uc_dev, unsigned int u4_addr,
			      enum sif_bit_t_hdmi uc_addr_type,
			      unsigned char *puc_value, unsigned int u4_count)
{
	unsigned int u4_read_count = 0;
	unsigned char uc_return_value = 0;

	if (!puc_value || !u4_count || !u4_clk_div ||
	    uc_addr_type > SIF_16_BIT_HDMI ||
	    (uc_addr_type == SIF_8_BIT_HDMI && u4_addr > 255) ||
	    (uc_addr_type == SIF_16_BIT_HDMI && u4_addr > 65535)) {
		return 0;
	}

	if (uc_addr_type == SIF_8_BIT_HDMI)
		u4_read_count = 255 - u4_addr + 1;
	else if (uc_addr_type == SIF_16_BIT_HDMI)
		u4_read_count = 65535 - u4_addr + 1;

	u4_read_count = (u4_read_count > u4_count) ? u4_count : u4_read_count;
	uc_return_value = ddcm_read_hdmi(ddc, u4_clk_div, uc_dev, u4_addr,
				       uc_addr_type, puc_value, u4_read_count);
	return uc_return_value;
}

static unsigned char fg_ddc_data_read(struct mtk_hdmi_ddc *ddc,
				   unsigned char b_dev,
				   unsigned char b_data_addr,
				   unsigned char b_data_count,
				   unsigned char *pr_data)
{
	bool flag;

	mutex_lock(&ddc->mtx);

	hdmi_ddc_request(ddc);
	if (vddc_read(ddc, DDC2_CLOK, (unsigned char)b_dev,
		     (unsigned int)b_data_addr, SIF_8_BIT_HDMI,
		     (unsigned char *)pr_data,
		     (unsigned int)b_data_count) == b_data_count) {
		flag = true;
	} else {
		flag = false;
	}

	mutex_unlock(&ddc->mtx);
	return flag;
}

static unsigned char fg_ddc_data_write(struct mtk_hdmi_ddc *ddc,
				    unsigned char b_dev,
				    unsigned char b_data_addr,
				    unsigned char b_data_count,
				    unsigned char *pr_data)
{
	unsigned int i;

	mutex_lock(&ddc->mtx);

	hdmi_ddc_request(ddc);
	for (i = 0; i < b_data_count; i++)
		DDC_WR_ONE(ddc, b_dev, b_data_addr + i, *(pr_data + i));

	mutex_unlock(&ddc->mtx);
	return 1;
}

static int mtk_hdmi_ddc_xfer(struct i2c_adapter *adapter, struct i2c_msg *msgs,
			     int num)
{
	struct mtk_hdmi_ddc *ddc = adapter->algo_data;
	struct device *dev = adapter->dev.parent;
	int ret;
	int i;
	unsigned char offset = 0;

	if (!ddc)
		return -EINVAL;

	for (i = 0; i < num; i++) {
		struct i2c_msg *msg = &msgs[i];

		if (msg->flags & I2C_M_RD) {
			/* The underlying DDC hardware always issue a write request
			 * that assigns the read offset as part of the read operation.
			 * Therefore we need to use the offset value assigned
			 * in the previous write request from the drm_edid.c
			 */
			ret = fg_ddc_data_read(ddc, msg->addr,
						offset, /* determined by previous write requests */
					    (msg->len), &msg->buf[0]);
		} else {
			ret = fg_ddc_data_write(ddc, msg->addr, msg->buf[0],
					     (msg->len - 1), &msg->buf[1]);

			/* we store the offset value requested by drm_edid and scdc framework
			 * to use in subsequent read requests.
			 */
			if (((msg->addr == DDC_ADDR) || (msg->addr == SCDC_I2C_SLAVE_ADDRESS))
				&& 1 == msg->len)
				offset = msg->buf[0];
		}

		if (ret <= 0)
			goto xfer_end;
	}

	return i;

xfer_end:
	dev_err(dev, "ddc failed! : %d\n", ret);
	return ret;
}

static u32 mtk_hdmi_ddc_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm mtk_hdmi_ddc_algorithm = {
	.master_xfer = mtk_hdmi_ddc_xfer,
	.functionality = mtk_hdmi_ddc_func,
};

static int mtk_hdmi_ddc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_hdmi_ddc *ddc;
	int ret;

	ddc = devm_kzalloc(dev, sizeof(struct mtk_hdmi_ddc), GFP_KERNEL);
	if (!ddc)
		return -ENOMEM;

	ddc->clk = devm_clk_get(dev, "ddc-i2c");
	if (IS_ERR(ddc->clk)) {
		dev_err(dev, "get ddc_clk failed: %p ,\n", ddc->clk);
		return PTR_ERR(ddc->clk);
	}
	ret = clk_prepare_enable(ddc->clk);
	if (ret) {
		dev_err(dev, "enable ddc clk failed!\n");
		return ret;
	}

	mutex_init(&ddc->mtx);

	strscpy(ddc->adap.name, "mediatek-hdmi-ddc", sizeof(ddc->adap.name));
	ddc->adap.owner = THIS_MODULE;
	ddc->adap.class = I2C_CLASS_DDC;
	ddc->adap.algo = &mtk_hdmi_ddc_algorithm;
	ddc->adap.retries = 3;
	ddc->adap.dev.of_node = dev->of_node;
	ddc->adap.algo_data = ddc;
	ddc->adap.dev.parent = &pdev->dev;

	ret = i2c_add_adapter(&ddc->adap);
	if (ret < 0) {
		dev_err(dev, "failed to add bus to i2c core\n");
		goto err_clk_disable;
	}

	platform_set_drvdata(pdev, ddc);
	return 0;

err_clk_disable:
	clk_disable_unprepare(ddc->clk);
	return ret;
}

static int mtk_hdmi_ddc_remove(struct platform_device *pdev)
{
	struct mtk_hdmi_ddc *ddc = platform_get_drvdata(pdev);

	mutex_destroy(&ddc->mtx);
	i2c_del_adapter(&ddc->adap);
	clk_disable_unprepare(ddc->clk);

	return 0;
}

static const struct of_device_id mtk_hdmi_ddc_match[] = {
	{
		.compatible = "mediatek,mt8195-hdmi-ddc",
	},
	{
		.compatible = "mediatek,mt8188-hdmi-ddc",
	},
	{},
};

struct platform_driver mtk_hdmi_mt8195_ddc_driver = {
	.probe = mtk_hdmi_ddc_probe,
	.remove = mtk_hdmi_ddc_remove,
	.driver = {
		.name = "mediatek-hdmi-mt8195-ddc",
		.of_match_table = mtk_hdmi_ddc_match,
	},
};

MODULE_AUTHOR("Can Zeng <can.zeng@mediatek.com>");
MODULE_DESCRIPTION("MediaTek HDMI DDC Driver");
MODULE_LICENSE("GPL v2");
