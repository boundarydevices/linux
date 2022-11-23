// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/prio.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/nvmem-consumer.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_wakeup.h>
#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif
#include <linux/pm_runtime.h>

#include "mtk_hdmirx.h"
#include "mtk_hdmi_rpt.h"
#include "hdmi_rx2_hw.h"
#include "vga_table.h"
#include "hdmi_rx2_hal.h"
#include "hdmi_rx21_phy.h"
#include "hdmi_rx2_hdcp.h"
#include "hdmi_rx2_ctrl.h"
#include "hdmi_rx2_dvi.h"
#include "hdmi_rx2_aud_task.h"
#include "hdmirx_drv.h"

static ssize_t notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct notify_device *sdev = (struct notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	return sprintf(buf, "%d\n", sdev->state);
}
static DEVICE_ATTR_RO(notify);

static ssize_t name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct notify_device *sdev = (struct notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	if (strlen(sdev->name) > 64)
		return 0;

	return snprintf(buf, 64, "%s\n", sdev->name);
}
static DEVICE_ATTR_RO(name);

#define HDMIRX_ATTR_SPRINTF(fmt, arg...)  \
do { \
	if (buf_offset < (sizeof(pbuf) - 1)) { \
		temp_len = sprintf(pbuf + buf_offset, fmt, ##arg); \
		if (temp_len > 0) \
			buf_offset += temp_len; \
		pbuf[buf_offset] = 0; \
	} \
} while (0)

static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct MTK_HDMI *myhdmi;
	u32 temp_len = 0;
	u32 buf_offset = 0;
	char pbuf[512];
	int ret;

	struct notify_device *sdev = (struct notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	memset(pbuf, 0, 512);

	myhdmi = (struct MTK_HDMI *)sdev->myhdmi;
	if ((myhdmi->p5v_status & 0x1) == 0x1)
		HDMIRX_ATTR_SPRINTF("[RX] hdmirx5v: on\n");
	else
		HDMIRX_ATTR_SPRINTF("[RX] hdmirx5v: off\n");
	HDMIRX_ATTR_SPRINTF("[RX] hpd: %d\n",
		hdmi2com_get_hpd_level(myhdmi, HDMI_SWITCH_1));
	HDMIRX_ATTR_SPRINTF("[RX] power_on: %d\n",
		myhdmi->power_on);
	HDMIRX_ATTR_SPRINTF("[RX] state: %d\n",
		myhdmi->HDMIState);
	HDMIRX_ATTR_SPRINTF("[RX] vid_locked: %d\n",
		myhdmi->vid_locked);
	HDMIRX_ATTR_SPRINTF("[RX] aud_locked: %d\n",
		myhdmi->aud_locked);
	HDMIRX_ATTR_SPRINTF("[RX] hdcp_version: %d\n",
		myhdmi->hdcp_version);

	if (hdmi2com_input_color_space_type(myhdmi) == 0x0) {
		if (hdmi2com_is_420_mode(myhdmi) == TRUE)
			HDMIRX_ATTR_SPRINTF("[RX] CS: YCbCr420\n");
		else if (hdmi2com_is_422_input(myhdmi) == 1)
			HDMIRX_ATTR_SPRINTF("[RX] CS: YCbCr422\n");
		else
			HDMIRX_ATTR_SPRINTF("[RX] CS: YCbCr444\n");
	} else
		HDMIRX_ATTR_SPRINTF("[RX] CS: RGB\n");

	if (myhdmi->dvi_s.DviTiming < MODE_MAX)
		HDMIRX_ATTR_SPRINTF("[RX] DviTiming: %d, %s\n",
			myhdmi->dvi_s.DviTiming,
			szRxResStr[myhdmi->dvi_s.DviTiming]);
	else
		HDMIRX_ATTR_SPRINTF("[RX] unknown DviTiming: %d\n",
			myhdmi->dvi_s.DviTiming);

	HDMIRX_ATTR_SPRINTF("[RX] width: %d\n",
		dvi2_get_input_width(myhdmi));
	HDMIRX_ATTR_SPRINTF("[RX] height: %d\n",
		dvi2_get_input_height(myhdmi));
	HDMIRX_ATTR_SPRINTF("[RX] interlace/progressive: %d\n",
		dvi2_get_interlaced(myhdmi));
	HDMIRX_ATTR_SPRINTF("[RX] pixel clock: %d\n",
		hdmi2com_pixel_clk(myhdmi));
	HDMIRX_ATTR_SPRINTF("[RX] DviVclk: %d\n",
		myhdmi->dvi_s.DviVclk);
	HDMIRX_ATTR_SPRINTF("[RX] DviHclk: %d\n",
		myhdmi->dvi_s.DviHclk);

	if (hdmi2com_upstream_is_hdcp2x_device(myhdmi)) {
		HDMIRX_ATTR_SPRINTF("[RX] upstream is hdcp2x device\n");
		HDMIRX_ATTR_SPRINTF("[RX] hdcp2 state: 0x%x\n",
			hdmi2com_hdcp2x_state(myhdmi));
	} else {
		RX_DEF_LOG("[RX]upstream is hdcp1x device\n");
		if (hdmi2com_check_hdcp1x_decrypt_on(myhdmi))
			HDMIRX_ATTR_SPRINTF("[RX] upstream is hdcp1x decrpt on\n");
		else
			HDMIRX_ATTR_SPRINTF("[RX] upstream is hdcp1x decrpt off\n");
	}

	myhdmi->crc0 = 0;
	hdmi2com_crc(myhdmi, 1);
	HDMIRX_ATTR_SPRINTF("[RX] crc: 0x%08x\n", myhdmi->crc0);

	if ((buf_offset + 1) >= 512) {
		RX_DEF_LOG("[rx]%s: too much %d\n", __func__, buf_offset);
		buf_offset = 510;
	}

	return snprintf(buf, buf_offset + 1, "%s\n", pbuf);
}
static DEVICE_ATTR_RO(status);

static int create_switch_class(struct MTK_HDMI *myhdmi)
{
	if (!myhdmi->switch_class) {
		myhdmi->switch_class = class_create(THIS_MODULE, "hdmirxswitch");
		if (IS_ERR(myhdmi->switch_class))
			return PTR_ERR(myhdmi->switch_class);
		atomic_set(&myhdmi->dev_cnt, 0);
	}
	return 0;
}

static int uevent_dev_register(struct MTK_HDMI *myhdmi,
	struct notify_device *sdev)
{
	int ret;

	RX_DEF_LOG("[RX]%s, 0x%lx\n", __func__, (unsigned long)sdev);

	if (!myhdmi->switch_class) {
		ret = create_switch_class(myhdmi);
		if (ret == 0)
			RX_DEF_LOG("[RX]create_switch_class susesess\n");
		else {
			RX_DEF_LOG("[RX]create_switch_class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&myhdmi->dev_cnt);
	sdev->dev = device_create(myhdmi->switch_class, NULL,
			MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev) {
		RX_DEF_LOG("[RX]device create ok,index:0x%x\n", sdev->index);
		ret = 0;
	} else {
		RX_DEF_LOG("[RX]device create fail,index:0x%x\n", sdev->index);
		ret = -1;
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_notify);
	if (ret < 0) {
		device_destroy(myhdmi->switch_class, MKDEV(0, sdev->index));
		RX_DEF_LOG("[RX]switch: Failed to register driver %s\n", sdev->name);
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_notify);
		RX_DEF_LOG("[RX]switch: Failed to register driver %s\n", sdev->name);
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_status);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_name);
		device_remove_file(sdev->dev, &dev_attr_notify);
		RX_DEF_LOG("[RX]status switch: Failed to %s\n", sdev->name);
		return ret;
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;
	sdev->myhdmi = (void *)myhdmi;

	return ret;
}

int notify_uevent(struct notify_device *sdev, int state)
{
	char *envp[3];
	char name_buf[120];
	char state_buf[120];
	int ret;

	if (sdev == NULL)
		return -1;

	if (sdev->state != state)
		sdev->state = state;

	ret = snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	if (ret < 0)
		RX_DEF_LOG("[RX]%s err\n", __func__);
	envp[0] = name_buf;
	ret = snprintf(state_buf, sizeof(state_buf), "SWITCH_NOTIFY=%d", sdev->state);
	if (ret < 0)
		RX_DEF_LOG("[RX]%s err\n", __func__);
	envp[1] = state_buf;
	envp[2] = NULL;

	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	//RX_DEF_LOG("[RX]SWITCH_NOTIFY:%d\n", state);

	return 0;
}

static ssize_t aud_notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct aud_notify_device *sdev = (struct aud_notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	return sprintf(buf, "%d\n", sdev->state);
}
static DEVICE_ATTR_RO(aud_notify);

static ssize_t aud_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct aud_notify_device *sdev = (struct aud_notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	if (strlen(sdev->name) > 64)
		return 0;

	return snprintf(buf, 64, "%s\n", sdev->name);
}
static DEVICE_ATTR_RO(aud_name);

static ssize_t aud_fs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct aud_notify_device *sdev = (struct aud_notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	return sprintf(buf, "%d\n", sdev->aud_fs);
}
static DEVICE_ATTR_RO(aud_fs);

static ssize_t aud_ch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct aud_notify_device *sdev = (struct aud_notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	return sprintf(buf, "%d\n", sdev->aud_ch);
}
static DEVICE_ATTR_RO(aud_ch);

static ssize_t aud_bit_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct aud_notify_device *sdev = (struct aud_notify_device *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	return sprintf(buf, "%d\n", sdev->aud_bit);
}
static DEVICE_ATTR_RO(aud_bit);

static int create_aud_switch_class(struct MTK_HDMI *myhdmi)
{
	if (!myhdmi->switch_aud_class) {
		myhdmi->switch_aud_class = class_create(THIS_MODULE, "hdmirxaud");
		if (IS_ERR(myhdmi->switch_aud_class))
			return PTR_ERR(myhdmi->switch_aud_class);
		atomic_set(&myhdmi->aud_dev_cnt, 0);
	}
	return 0;
}

static int aud_uevent_dev_register(struct MTK_HDMI *myhdmi,
	struct aud_notify_device *sdev)
{
	int ret;

	RX_DEF_LOG("[RX]%s, 0x%lx\n", __func__, (unsigned long)sdev);

	if (!myhdmi->switch_aud_class) {
		ret = create_aud_switch_class(myhdmi);
		if (ret == 0)
			RX_DEF_LOG("[RX]create_aud_switch_class susesess\n");
		else {
			RX_DEF_LOG("[RX]create_aud_switch_class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&myhdmi->aud_dev_cnt);
	sdev->dev = device_create(myhdmi->switch_aud_class, NULL,
			MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev) {
		RX_DEF_LOG("[RX]aud device create ok,index:0x%x\n", sdev->index);
		ret = 0;
	} else {
		RX_DEF_LOG("[RX]aud device create fail,index:0x%x\n", sdev->index);
		ret = -1;
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_aud_notify);
	if (ret < 0) {
		device_destroy(myhdmi->switch_aud_class, MKDEV(0, sdev->index));
		RX_DEF_LOG("[RX]aud state switch: Failed to %s\n", sdev->name);
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_aud_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_aud_notify);
		RX_DEF_LOG("[RX]aud name switch: Failed to %s\n", sdev->name);
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_aud_fs);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_aud_notify);
		device_remove_file(sdev->dev, &dev_attr_aud_name);
		RX_DEF_LOG("[RX]aud fs switch: Failed to %s\n", sdev->name);
		return ret;
	}
	ret = device_create_file(sdev->dev, &dev_attr_aud_ch);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_aud_notify);
		device_remove_file(sdev->dev, &dev_attr_aud_name);
		device_remove_file(sdev->dev, &dev_attr_aud_fs);
		RX_DEF_LOG("[RX]aud ch switch: Failed to %s\n", sdev->name);
		return ret;
	}
	ret = device_create_file(sdev->dev, &dev_attr_aud_bit);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_aud_notify);
		device_remove_file(sdev->dev, &dev_attr_aud_name);
		device_remove_file(sdev->dev, &dev_attr_aud_fs);
		device_remove_file(sdev->dev, &dev_attr_aud_ch);
		RX_DEF_LOG("[RX]aud bit switch: Failed to %s\n", sdev->name);
		return ret;
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;
	sdev->aud_fs = 0;
	sdev->aud_ch = 0;
	sdev->aud_bit = 0;

	return ret;
}

int aud_notify_uevent(struct aud_notify_device *sdev,
	int state,
	int aud_fs,
	int aud_ch,
	int aud_bit)
{
	char *envp[6];
	char name_buf[40];
	char state_buf[40];
	char fs_buf[40];
	char ch_buf[40];
	char bit_buf[40];
	int ret;

	if (sdev == NULL)
		return -1;

	if (sdev->state != state)
		sdev->state = state;
	sdev->aud_fs = aud_fs;
	sdev->aud_ch = aud_ch;
	sdev->aud_bit = aud_bit;

	ret = snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	if (ret < 0)
		RX_DEF_LOG("[RX]%s err\n", __func__);
	envp[0] = name_buf;

	ret = snprintf(state_buf, sizeof(state_buf), "SWITCH_NOTIFY=%d", sdev->state);
	if (ret < 0)
		RX_DEF_LOG("[RX]%s err\n", __func__);
	envp[1] = state_buf;

	ret = snprintf(fs_buf, sizeof(fs_buf), "SWITCH_FS=%d", sdev->aud_fs);
	if (ret < 0)
		RX_DEF_LOG("[RX]%s err\n", __func__);
	envp[2] = fs_buf;

	ret = snprintf(ch_buf, sizeof(ch_buf), "SWITCH_CH=%d", sdev->aud_ch);
	if (ret < 0)
		RX_DEF_LOG("[RX]%s err\n", __func__);
	envp[3] = ch_buf;

	ret = snprintf(bit_buf, sizeof(bit_buf), "SWITCH_BIT=%d", sdev->aud_bit);
	if (ret < 0)
		RX_DEF_LOG("[RX]%s err\n", __func__);
	envp[4] = bit_buf;

	envp[5] = NULL;

	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	return 0;
}

/** @ingroup type_group_hdmirx_drv_API
 * @par Description\n
 *	   notify VSW event.
 * @param[in]
 *	   myhdmi: struct MTK_HDMI *myhdmi.
 * @param[in]
 *	   notify: VSW event ID.
 * @return
 *	   none.
 * @par Boundary case and Limitation
 *	   none.
 * @par Error case and Error handling
 *	   none.
 * @par Call graph and Caller graph (refer to the graph below)
 * @par Refer to the source code
 */
void
hdmi_rx_video_notify(struct MTK_HDMI *myhdmi, enum VSW_NFY_COND_T notify)
{
	if (notify == VSW_NFY_ERROR) {
		RX_INFO_LOG("[RX]VSW_NFY_ERROR\n");
	} else if (notify == VSW_NFY_LOCK) {
		RX_INFO_LOG("[RX]VSW_NFY_LOCK,%lu\n", jiffies);
	} else if (notify == VSW_NFY_UNLOCK) {
		RX_INFO_LOG("[RX]VSW_NFY_UNLOCK,%lu\n", jiffies);
		hdmirx_state_callback(myhdmi, HDMI_RX_TIMING_UNLOCK);
	} else if (notify == VSW_NFY_RES_CHGING) {
		myhdmi->vid_locked = 0;
		RX_INFO_LOG("[RX]VSW_NFY_RES_CHGING,%lu\n", jiffies);
	} else if (notify == VSW_NFY_RES_CHG_DONE) {
		RX_INFO_LOG("[RX]VSW_NFY_RES_CHG_DONE,%lu\n", jiffies);
		myhdmi->vid_locked = 1;
		if ((myhdmi->hdmi_mode_setting == HDMI_RPT_TMDS_MODE) ||
			(myhdmi->hdmi_mode_setting == HDMI_RPT_PIXEL_MODE)) {
			/* get video parameter */
			hdmi2_get_vid_para(myhdmi);
			/* set hdmitx repeater mode */
			txapi_rpt_mode(myhdmi, myhdmi->hdmi_mode_setting);
			/* config output path */
			hdmi2com_cfg_out(myhdmi);
			/* set hdmitx */
			txapi_cfg_video(myhdmi);
		}
		hdmirx_state_callback(myhdmi, HDMI_RX_TIMING_LOCK);
	} else if (notify == VSW_NFY_ASPECT_CHG) {
		RX_INFO_LOG("[RX]VSW_NFY_ASPECT_CHG\n");
	} else if (notify == VSW_NFY_CS_CHG) {
		RX_INFO_LOG("[RX]VSW_NFY_CS_CHG\n");
	} else if (notify == VSW_NFY_JPEG_CHG) {
		RX_INFO_LOG("[RX]VSW_NFY_JPEG_CHG\n");
	} else if (notify == VSW_NFY_CINEMA_CHG) {
		RX_INFO_LOG("[RX]VSW_NFY_CINEMA_CHG\n");
	}
}

void
hdmi_rx_audio_notify(struct MTK_HDMI *myhdmi, enum AUDIO_EVENT_T notify)
{
	if (notify == AUDIO_LOCK) {
		RX_AUDIO_LOG("[RX]AUDIO_LOCK,%lu\n", jiffies);
		myhdmi->aud_locked = 1;
		hdmirx_state_callback(myhdmi, HDMI_RX_AUD_LOCK);
	} else if (notify == AUDIO_UNLOCK) {
		RX_AUDIO_LOG("[RX]AUDIO_UNLOCK,%lu\n", jiffies);
		myhdmi->aud_locked = 0;
		hdmirx_state_callback(myhdmi, HDMI_RX_AUD_UNLOCK);
	}
}

void
hdmi_rx_notify(struct MTK_HDMI *myhdmi, enum HDMIRX_NOTIFY_T notify)
{
	RX_INFO_LOG("[RX]%s: %d\n", __func__, notify);
	if (notify == HDMI_RX_ACP_PKT_CHANGE)
		hdmirx_state_callback(myhdmi, HDMI_RX_ACP_PKT_CHANGE);
	else if (notify == HDMI_RX_AVI_INFO_CHANGE)
		hdmirx_state_callback(myhdmi, HDMI_RX_AVI_INFO_CHANGE);
	else if (notify == HDMI_RX_AUD_INFO_CHANGE)
		hdmirx_state_callback(myhdmi, HDMI_RX_AUD_INFO_CHANGE);
	else if (notify == HDMI_RX_HDR_INFO_CHANGE)
		hdmirx_state_callback(myhdmi, HDMI_RX_HDR_INFO_CHANGE);
	else if (notify == HDMI_RX_PWR_5V_CHANGE)
		hdmirx_state_callback(myhdmi, HDMI_RX_PWR_5V_CHANGE);
	else if (notify == HDMI_RX_EDID_CHANGE)
		hdmirx_state_callback(myhdmi, HDMI_RX_EDID_CHANGE);
	else if (notify == HDMI_RX_HDCP_VERSION)
		hdmirx_state_callback(myhdmi, HDMI_RX_HDCP_VERSION);
}

void
hdmi_rx_send_cmd(struct MTK_HDMI *myhdmi, u8 u1icmd)
{
	myhdmi->main_cmd = u1icmd;

	atomic_set(&myhdmi->main_event, 1);
	wake_up_interruptible(&myhdmi->main_wq);
}

void hdmi_rx_set_main_state(struct MTK_HDMI *myhdmi, u32 state)
{
	u32 temp;

	temp = myhdmi->main_state;
	myhdmi->main_state = state;
	RX_DEF_LOG("[RX]main state(%d->%d)\n", temp, state);
}

void
hdmi_rx_suspend(void)
{
}

void
hdmi_rx_resume(void)
{
}

void
hdmi_rx_timer_isr(struct timer_list *t)
{
	struct MTK_HDMI *myhdmi = from_timer(myhdmi, t, hdmi_timer);

	if (myhdmi->main_state == HDMIRX_IDLE) {
		RX_DEF_LOG("[RX]timer unused\n");
		return;
	}

	if ((myhdmi->is_rx_task_disable == FALSE) || (myhdmi->hdcpinit == 0))
		hdmi_rx_send_cmd(myhdmi, HDMI_RX_SERVICE_CMD);

	if (myhdmi->is_rx_task_disable == FALSE)
		hdmi2_timer_handle(myhdmi);
	if ((hdmi2_get_state(myhdmi) == rx_reset_digital) ||
		(hdmi2_get_state(myhdmi) == rx_wait_timing_stable) ||
		(hdmi2_get_state(myhdmi) == rx_is_stable)) {
		if (myhdmi->scdt_clr_cnt > 10)
			myhdmi->scdt = FALSE;
		else
			myhdmi->scdt_clr_cnt++;
		#ifdef HDMIRX_RPT_EN
		if (myhdmi->vrr_emp.cnt > 10) {
			myhdmi->vrr_emp.fgValid = FALSE;
			hdmi2_emp_vrr_irq(myhdmi);
		} else
			myhdmi->vrr_emp.cnt++;
		#endif
		if (myhdmi->hdcp_flag == TRUE) {
			myhdmi->hdcp_cnt++;
			if (myhdmi->hdcp_cnt > 200) {
				myhdmi->hdcp_flag = FALSE;
				myhdmi->hdcp_version = 0;
				hdmi_rx_notify(myhdmi, HDMI_RX_HDCP_VERSION);
			}
		}
	}

	myhdmi->timer_count++;
	mod_timer(&myhdmi->hdmi_timer,
		jiffies + HDMI_RX_TIMER_TICKET / (1000 / HZ));
}

static int
hdmi_rx_main_kthread(void *data)
{
	struct MTK_HDMI *myhdmi = data;
	int ret = 0;

	RX_DEF_LOG("[RX]myhdmi->is_rx_init=%d\n",
		myhdmi->is_rx_init);
	RX_DEF_LOG("[RX]myhdmi->main_cmd=%d\n",
		myhdmi->main_cmd);
	RX_DEF_LOG("[RX]myhdmi->is_rx_task_disable=%d\n",
		myhdmi->is_rx_task_disable);

	while (myhdmi->is_rx_init == TRUE) {
		ret = wait_event_interruptible(myhdmi->main_wq,
			atomic_read(&myhdmi->main_event));
		if (ret == -ERESTARTSYS) {
			RX_DEF_LOG("[RX]event err: %d\n", ret);
			ret = -EINTR;
			break;
		}
		atomic_set(&myhdmi->main_event, 0);
		if (myhdmi->is_rx_init == FALSE) {
			RX_DEF_LOG("[RX]task break\n");
			break;
		}

		myhdmi->task_count++;

		if (myhdmi->main_state == HDMIRX_INIT) {
			hdmi_rx_set_main_state(myhdmi, HDMIRX_RUN);
			hdmirx_toprgu_rst(myhdmi);
			hdmi2_rx_var_init(myhdmi);
			dvi2_var_init(myhdmi);
			hdmi2_hw_init(myhdmi);
			hdmi2_set_state(myhdmi, rx_no_signal);
			hdmi2_update_tx_port(myhdmi);
		} else if (myhdmi->main_state == HDMIRX_RUN) {
			if (myhdmi->main_cmd == HDMI_RX_SERVICE_CMD) {
				hdmi2_state(myhdmi);
				if (myhdmi->hdmi_mode_setting != HDMI_RPT_TMDS_MODE) {
					if (!myhdmi->is_detected)
						dvi2_mode_detected(myhdmi);
					else
						dvi2_check_mode_change(myhdmi);
				}
				hdmi2_hdcp_service(myhdmi);
			}
		} else if (myhdmi->main_state == HDMIRX_EXIT) {
			hdmi_rx_set_main_state(myhdmi, HDMIRX_IDLE);
		}

		if (kthread_should_stop())
			break;
	}

	myhdmi->is_wait_finish = TRUE;
	RX_DEF_LOG("[RX]task exit\n");

	return ret;
}

static irqreturn_t hdmi_rx_irq(int irq, void *dev_id)
{
	struct MTK_HDMI *myhdmi = (struct MTK_HDMI *)dev_id;
	u32 isr_status, isr130_status;

	isr_status = hdmi2com_check_isr(myhdmi);
	isr130_status = hdmi2com_check_isr130(myhdmi);
	hdmi2com_clr_int(myhdmi);

	if (myhdmi->main_state != HDMIRX_RUN) {
		RX_DEF_LOG("[RX]irq unused\n");
		hdmi2com_set_isr_enable(myhdmi, FALSE, HDMI_RX_IRQ_ALL);
		return IRQ_HANDLED;
	}

	if (isr130_status & HDMI_RX_VRR_INT) {
		myhdmi->vrr_emp.fgValid = TRUE;
		myhdmi->vrr_emp.cnt = 0;
		hdmi2_emp_vrr_irq(myhdmi);
		if (myhdmi->my_debug == 12)
			RX_DEF_LOG("[RX]EMP IRQ\n");
	}

	if (isr_status & HDMI_RX_CKDT_INT) {
		hdmi2com_set_isr_enable(myhdmi, FALSE,
			HDMI_RX_CKDT_INT);
		RX_DEF_LOG("[RX]HDMI_RX_CKDT_INT\n");
	}

	if (isr_status & HDMI_RX_SCDT_INT) {
		hdmi2com_set_isr_enable(myhdmi, FALSE,
			HDMI_RX_SCDT_INT);
		RX_DEF_LOG("[RX]HDMI_RX_SCDT_INT\n");
	}

	if (isr_status & HDMI_RX_VSYNC_INT) {
		myhdmi->scdt = (bool)r_fld(TOP_MISC, SCDT);
		myhdmi->scdt_clr_cnt  = 0;
		//myhdmi->temp++;
		//if ((myhdmi->temp % 360) == 0)
			//RX_DEF_LOG("[RX]HDMI_RX_VSYNC_INT\n");
	}

	if (isr_status & HDMI_RX_CDRADIO_INT)
		RX_DEF_LOG("[RX]HDMI_RX_CDRADIO_INT\n");

	if (isr_status & HDMI_RX_SCRAMBLING_INT)
		RX_DEF_LOG("[RX]HDMI_RX_SCRAMBLING_INT\n");

	if (isr_status & HDMI_RX_FSCHG_INT)
		RX_DEF_LOG("[RX]HDMI_RX_FSCHG_INT\n");

	if (isr_status & HDMI_RX_DSDPKT_INT)
		RX_DEF_LOG("[RX]HDMI_RX_DSDPKT_INT\n");

	if (isr_status & HDMI_RX_HBRPKT_INT)
		RX_DEF_LOG("[RX]HDMI_RX_HBRPKT_INT\n");

	if (isr_status & HDMI_RX_CHCHG_INT)
		RX_DEF_LOG("[RX]HDMI_RX_CHCHG_INT\n");

	if (isr_status & HDMI_RX_AVI_INT)
		RX_DEF_LOG("[RX]HDMI_RX_AVI_INT\n");

	if (isr_status & HDMI_RX_HDMI_MODE_CHG)
		RX_DEF_LOG("[RX]HDMI_RX_HDMI_MODE_CHG\n");

	if (isr_status & HDMI_RX_HDMI_NEW_GCP)
		RX_DEF_LOG("[RX]HDMI_RX_HDMI_NEW_GCP:0x%08x\n",
			r_reg(RX_GCP_DBYTE3));

	if (isr_status & HDMI_RX_PDTCLK_CHG)
		RX_DEF_LOG("[RX]HDMI_RX_PDTCLK_CHG:0x2ec60=%08x\n",
			r_reg(RG_RX_DEBUG_CLK_XPCNT));

	if (isr_status & HDMI_RX_H_UNSTABLE)
		RX_DEF_LOG("[RX]HDMI_RX_H_UNSTABLE:0x2e890=%08x\n",
			r_reg(RG_FDET_HSYNC_HIGH_COUNT));

	if (isr_status & HDMI_RX_V_UNSTABLE)
		RX_DEF_LOG("[RX]HDMI_RX_V_UNSTABLE:0x2e898=%08x\n",
			r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN));

	if ((isr_status & HDMI_RX_SET_MUTE) ||
		(isr130_status & HDMI_RX_CLEAR_MUTE)) {
		if (r_fld(CPRX_BYTE1, REG_CP_SET_MUTE))
			RX_DEF_LOG("[RX]HDMI_RX_SET_MUTE\n");
		if (r_fld(CPRX_BYTE1, REG_CP_CLR_MUTE))
			RX_DEF_LOG("[RX]HDMI_RX_CLEAR_MUTE\n");
	}

#ifdef CC_SUPPORT_HDR10Plus
	if (isr130_status & HDMI_RX_VSI_INT)
		_vHDMI2ParseHdr10PlusVsiHandle(myhdmi);
#endif

	// for hdr10+ and DV EM Packet
	if (isr130_status & HDMI_RX_EMP_INT)
		_vHDMI20HDREMPHandler(myhdmi);

	return IRQ_HANDLED;
}

static irqreturn_t hdmi_rx_phy_irq(int irq, void *dev_id)
{
	struct MTK_HDMI *myhdmi = (struct MTK_HDMI *)dev_id;
	u32 isr_status;

	isr_status = vHDMI21PhyIRQClear(myhdmi);

	if (isr_status) {
		vHDMI21PhyIRQEnable(myhdmi, 1, FALSE);
		RX_DEF_LOG("[RX]HDMI_RX_CKDT_INT, 0x%08x\n", isr_status);
	}

	return IRQ_HANDLED;
}

static u32
hdmi_rx_task_init(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]%s\n", __func__);

	hdmi_rx_set_main_state(myhdmi, HDMIRX_INIT);
	mod_timer(&myhdmi->hdmi_timer, jiffies + 1000 / (1000 / HZ));

	return SRV_OK;
}

static u32
hdmi_rx_task_uninit(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]%s", __func__);

	hdmi_rx_set_main_state(myhdmi, HDMIRX_EXIT);
	hdmi_rx_send_cmd(myhdmi, HDMI_RX_SERVICE_CMD);

	return SRV_OK;
}

void
hdmi_rx_task_start(struct MTK_HDMI *myhdmi, bool u1Enable)
{
	if (u1Enable) {
		if (myhdmi->is_rx_task_disable == TRUE)
			myhdmi->is_rx_task_disable = FALSE;
	} else {
		if (myhdmi->is_rx_task_disable == FALSE)
			myhdmi->is_rx_task_disable = TRUE;
	}

	if (u1Enable == FALSE)
		RX_DEF_LOG(
			"[RX]stop task:%d\n", myhdmi->is_rx_task_disable);
	else
		RX_DEF_LOG(
			"[RX]satrt task:%d\n", myhdmi->is_rx_task_disable);
}

bool
hdmi_rx_domain_on(struct MTK_HDMI *myhdmi)
{
	int ret;

	RX_DEF_LOG("[RX]enable power domain and CG\n");

	/* HDMIRX PM domain */
	ret = pm_runtime_get_sync(myhdmi->dev);
	if (ret < 0) {
		RX_DEF_LOG("[RX]failed to enable hdmirx power: %d\n", ret);
		return FALSE;
	}

	/* enable clk */
	ret = clk_set_parent(myhdmi->hdmiclksel, myhdmi->hdmiclk208m);
	if (ret) {
		RX_DEF_LOG("[RX]Failed to set hdmiclksel\n");
		return FALSE;
	}
	ret = clk_prepare_enable(myhdmi->hdmiclksel);
	if (ret) {
		RX_DEF_LOG("[RX]Failed to enable hdmiclksel\n");
		return FALSE;
	}
	ret = clk_set_parent(myhdmi->hdcpclksel, myhdmi->hdcpclk104m);
	if (ret) {
		RX_DEF_LOG("[RX]Failed to set hdcpclksel\n");
		return FALSE;
	}
	ret = clk_prepare_enable(myhdmi->hdcpclksel);
	if (ret) {
		RX_DEF_LOG("[RX]Failed to enable hdcpclksel\n");
		return FALSE;
	}
	ret = clk_prepare_enable(myhdmi->hdmisel);
	if (ret) {
		RX_DEF_LOG("[RX]Failed to enable hdmisel\n");
		return FALSE;
	}

	return TRUE;
}


void
hdmi_rx_domain_off(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]disable power domain and CG\n");
	/* disable HDMIRX PM domain */
	pm_runtime_put_sync(myhdmi->dev);
	/* disable clk */
	clk_disable_unprepare(myhdmi->hdmiclksel);
	clk_disable_unprepare(myhdmi->hdcpclksel);
	clk_disable_unprepare(myhdmi->hdmisel);
}

void
hdmi_rx_power_on(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->power_on == 1) {
		RX_DEF_LOG("[RX]don't power on again\n");
		return;
	}

	RX_DEF_LOG("[RX]%s\n", __func__);
	hdmi_rx_domain_on(myhdmi);
	hdmi_rx_task_init(myhdmi);

	myhdmi->power_on = 1;
}

void
hdmi_rx_power_off(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	if (myhdmi->power_on == 0) {
		RX_DEF_LOG("[RX]don't power off again\n");
		return;
	}

	RX_DEF_LOG("[RX]%s\n", __func__);
	hdmi_rx_task_uninit(myhdmi);

	for (temp = 0; temp < 10; temp++) {
		usleep_range(10000, 10050);
		if (myhdmi->main_state == HDMIRX_IDLE) {
			hdmi2_set_state(myhdmi, rx_no_signal);
			vHDMI21PhyPowerDown(myhdmi);
			hdmi_rx_domain_off(myhdmi);
			RX_DEF_LOG("[RX]power off, %d\n", temp);
			break;
		}
	}

	myhdmi->power_on = 0;
}

void
hdmi_rx_get_aud_info(struct MTK_HDMI *myhdmi, struct HDMIRX_AUD_INFO_T *p)
{
	p->is_aud_lock = aud2_is_lock(myhdmi);
	hdmi2_get_audio_info(myhdmi, &(p->aud));
}

void
hdmi_rx_get_info(struct MTK_HDMI *myhdmi, struct HDMIRX_INFO_T *p)
{
	p->is_5v_on = hdmi2_get_5v_level(myhdmi);
	p->is_stable = (myhdmi->HDMIState == rx_is_stable);
	p->is_timing_lock = (myhdmi->dvi_s.DviChkState == DVI_CHK_MODECHG);
	p->is_hdmi_mode = hdmi2_is_hdmi_mode(myhdmi);
	p->is_rgb = TRUE;
	if (hdmi2com_input_color_space_type(myhdmi) == 0)
		p->is_rgb = FALSE;
	p->deep_color = hdmi2_get_deep_color_status(myhdmi);
	dvi2_get_timing_detail(myhdmi, &(p->timing));
}

void
hdmi_rx_set_edid(struct MTK_HDMI *myhdmi,
	struct HDMI_RX_E_T *pedid)
{
	hdmi2_write_edid(myhdmi, &(pedid->u1HdmiRxEDID[0]));
}

void hdmirx_set_vcore(struct MTK_HDMI *myhdmi, u32 vcore)
{
#if HDMIRX_YOCTO
	int ret;

	if ((vcore > 750000) || (vcore < 550000))
		return;

	RX_DEF_LOG("[RX] set vcore %d\n", vcore);
	ret = regulator_set_voltage(myhdmi->reg_vcore, vcore, INT_MAX);
	if (ret)
		RX_DEF_LOG("[RX] set vcore %d err\n", vcore);
#endif
}

void
hdmirx_state_callback(struct MTK_HDMI *myhdmi,
	enum HDMIRX_NOTIFY_T notify)
{
	u32 temp;
	int ret;

	if (myhdmi->video_notify == notify) {
		RX_INFO_LOG("[RX]ignore video notify, %d.\n", notify);
		return;
	} else if (myhdmi->audio_notify == notify) {
		RX_INFO_LOG("[RX]ignore audio notify, %d.\n", notify);
		return;
	}

	switch (notify) {
	case HDMI_RX_PWR_5V_CHANGE:
		if ((myhdmi->portswitch == HDMI_SWITCH_1) &&
			((myhdmi->p5v_status & 0x1) == 0x1)) {
			temp = 1;
		} else {
			temp = 0;
			if (myhdmi->video_notify != HDMI_RX_TIMING_UNLOCK) {
				notify_uevent(&myhdmi->switch_data,
					HDMI_RX_TIMING_UNLOCK);
				myhdmi->video_notify = HDMI_RX_TIMING_UNLOCK;
			}
			if (myhdmi->audio_notify != HDMI_RX_AUD_UNLOCK) {
				notify_uevent(&myhdmi->switch_data,
					HDMI_RX_AUD_UNLOCK);
				aud_notify_uevent(&myhdmi->switch_aud_data,
					0xFF, 0, 0, 0);
				aud_notify_uevent(&myhdmi->switch_aud_data,
					HDMI_RX_AUD_UNLOCK, 0, 0, 0);
				myhdmi->audio_notify = HDMI_RX_AUD_UNLOCK;
			}
		}
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_PWR_5V_CHANGE);
		if (temp == 1) {
			notify_uevent(&myhdmi->switch_data,
				HDMI_RX_PLUG_IN);
			#if HDMIRX_YOCTO
			ret = regulator_set_voltage(myhdmi->reg_vcore, 750000, INT_MAX);
			if (ret)
				RX_DEF_LOG("[RX] set vcore 750000 err\n");
			#endif
		} else {
			notify_uevent(&myhdmi->switch_data,
				HDMI_RX_PLUG_OUT);
			#if HDMIRX_YOCTO
			ret = regulator_set_voltage(myhdmi->reg_vcore, 550000, INT_MAX);
			if (ret)
				RX_DEF_LOG("[RX] set vcore 550000 err\n");
			#endif
		}
		break;
	case HDMI_RX_TIMING_LOCK:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_TIMING_LOCK);
		myhdmi->video_notify = HDMI_RX_TIMING_LOCK;
		break;
	case HDMI_RX_TIMING_UNLOCK:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_TIMING_UNLOCK);
		myhdmi->video_notify = HDMI_RX_TIMING_UNLOCK;
		break;
	case HDMI_RX_AUD_LOCK:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_AUD_LOCK);
		aud_notify_uevent(&myhdmi->switch_aud_data,
			0xFF, 0, 0, 0);
		aud_notify_uevent(&myhdmi->switch_aud_data,
			HDMI_RX_AUD_LOCK,
			myhdmi->aud_caps.SampleFreq,
			myhdmi->aud_caps.AudInf.info.SpeakerPlacement,
			myhdmi->aud_caps.AudChStat.WordLen);
		myhdmi->audio_notify = HDMI_RX_AUD_LOCK;
		break;
	case HDMI_RX_AUD_UNLOCK:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_AUD_UNLOCK);
		aud_notify_uevent(&myhdmi->switch_aud_data,
			0xFF, 0, 0, 0);
		aud_notify_uevent(&myhdmi->switch_aud_data,
			HDMI_RX_AUD_UNLOCK, 0, 0, 0);
		myhdmi->audio_notify = HDMI_RX_AUD_UNLOCK;
		break;
	case HDMI_RX_ACP_PKT_CHANGE:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_ACP_PKT_CHANGE);
		break;
	case HDMI_RX_AVI_INFO_CHANGE:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_AVI_INFO_CHANGE);
		break;
	case HDMI_RX_AUD_INFO_CHANGE:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_AUD_INFO_CHANGE);
		break;
	case HDMI_RX_HDR_INFO_CHANGE:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_HDR_INFO_CHANGE);
		break;
	case HDMI_RX_HDCP_VERSION:
		notify_uevent(&myhdmi->switch_data,
			HDMI_RX_HDCP_VERSION);
		break;
	default:
		break;
	}
}

bool mtk_is_rpt_mode(struct device *dev)
{
	struct MTK_HDMI *myhdmi = dev_get_drvdata(dev);

	if (myhdmi->main_state == HDMIRX_IDLE)
		return FALSE;

	if ((myhdmi->hdmi_mode_setting == HDMI_RPT_DGI_MODE) ||
		(myhdmi->hdmi_mode_setting == HDMI_RPT_DRAM_MODE) ||
		(myhdmi->hdmi_mode_setting == HDMI_RPT_PIXEL_MODE) ||
		(myhdmi->hdmi_mode_setting == HDMI_RPT_TMDS_MODE))
		return TRUE;
	else
		return FALSE;
}

void mtk_issue_edid(struct device *dev, u8 u1EdidReady)
{
	struct MTK_HDMI *myhdmi = dev_get_drvdata(dev);

	if (myhdmi->main_state == HDMIRX_IDLE)
		return;

	RX_DEF_LOG("[RX]tx edid chg: old=%d,current=%d,new=%d,%lums\n",
		myhdmi->u1TxEdidReadyOld,
		myhdmi->u1TxEdidReady,
		u1EdidReady, jiffies);

	if ((u1EdidReady != myhdmi->u1TxEdidReadyOld) ||
		(myhdmi->u1TxEdidReadyOld != myhdmi->u1TxEdidReady)) {
		hdmi2_set_hpd_low_timer(myhdmi, 0);
		hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
		if (u1EdidReady == HDMI_PLUG_OUT)
			hdmi2_hdcp_set_receiver(myhdmi);
		else
			if (txapi_is_plugin(myhdmi))
				hdmi2_hdcp_set_repeater(myhdmi);
			else
				hdmi2_hdcp_set_receiver(myhdmi);
	}

	myhdmi->u1TxEdidReady = u1EdidReady;
}

bool mtk_is_upstream_need_auth(struct device *dev)
{
	struct MTK_HDMI *myhdmi = dev_get_drvdata(dev);

	if (myhdmi->main_state == HDMIRX_IDLE)
		return FALSE;

	if (myhdmi->hdcp_state == UnAuthenticated)
		return FALSE;
	else
		return TRUE;
}

void mtk_rx_set_tx_ksv(struct device *dev,
	bool fgIsHdcp2,
	u16 u2TxBStatus,
	u8 *prbTxBksv,
	u8 *prbTxKsvlist,
	bool fgTxVMatch)
{
	struct MTK_HDMI *myhdmi = dev_get_drvdata(dev);
	u8 i;

	if (myhdmi->main_state == HDMIRX_IDLE)
		return;

	if ((prbTxBksv == NULL) || (prbTxKsvlist == NULL)) {
		RX_DEF_LOG("[RX]ksv list err\n");
		hdmi2_set_hdcp_auth_done(myhdmi, TRUE);
		return;
	}

	if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP)) {
		RX_DEF_LOG("[RX]rpt data,h2=%d,b=%x,m=%d,%lums\n",
			fgIsHdcp2,
			u2TxBStatus,
			fgTxVMatch,
			jiffies);
		RX_DEF_LOG("[RX]rpt data,bksv= %x %x %x %x %x\n",
			prbTxBksv[0],
			prbTxBksv[1],
			prbTxBksv[2],
			prbTxBksv[3],
			prbTxBksv[4]);
		hdmi2_rx_info(myhdmi);
	}

	memset(&myhdmi->rpt_data, 0, sizeof(struct RxHDCPRptData));

	myhdmi->rpt_data.is_hdcp2 = fgIsHdcp2;
	myhdmi->rpt_data.is_match = fgTxVMatch;

	if (myhdmi->rpt_data.is_hdcp2) {
		if (u2TxBStatus & HDCP2_MAX_CASCADE_EXCEEDED)
			myhdmi->rpt_data.is_casc_exc = TRUE;

		if (u2TxBStatus & HDCP2_MAX_DEVS_EXCEEDED)
			myhdmi->rpt_data.is_devs_exc = TRUE;

		if (u2TxBStatus & HDCP2_INCLUDE_HDCP1_DEV)
			myhdmi->rpt_data.is_hdcp1_present = TRUE;

		if (u2TxBStatus & HDCP2_INCLUDE_HDCP2_DEV)
			myhdmi->rpt_data.is_hdcp2_present = TRUE;

		myhdmi->rpt_data.count =
			((u2TxBStatus & HDCP2_DEVICE_COUNT) >> 4);
		if (myhdmi->rpt_data.count > HDCP2_RX_MAX_KSV_COUNT) {
			myhdmi->rpt_data.count = HDCP2_RX_MAX_KSV_COUNT;
			myhdmi->rpt_data.is_devs_exc = TRUE;
		}
		for (i = 0; i < (myhdmi->rpt_data.count*5); i++)
			myhdmi->rpt_data.list[i] = prbTxKsvlist[i];

		for (i = 0; i < 5; i++)
			myhdmi->rpt_data.list[(myhdmi->rpt_data.count)*5 + i] =
				prbTxBksv[i];

		myhdmi->rpt_data.count = myhdmi->rpt_data.count + 1;
		if (myhdmi->rpt_data.count > HDCP2_RX_MAX_KSV_COUNT) {
			myhdmi->rpt_data.count = HDCP2_RX_MAX_KSV_COUNT;
			myhdmi->rpt_data.is_devs_exc = TRUE;
		}

		myhdmi->rpt_data.depth =
			((u2TxBStatus & HDCP2_DEVICE_DEPTH) >> 9) + 1;
		if (myhdmi->rpt_data.depth > HDCP2_RX_MAX_DEV_COUNT) {
			myhdmi->rpt_data.depth = HDCP2_RX_MAX_DEV_COUNT;
			myhdmi->rpt_data.is_casc_exc = TRUE;
		}

	} else {
		if (u2TxBStatus & MAX_CASCADE_EXCEEDED)
			myhdmi->rpt_data.is_casc_exc = TRUE;

		if (u2TxBStatus & MAX_DEVS_EXCEEDED)
			myhdmi->rpt_data.is_devs_exc = TRUE;

		myhdmi->rpt_data.is_hdcp1_present = TRUE;

		myhdmi->rpt_data.count = u2TxBStatus & DEVICE_COUNT;
		if (myhdmi->rpt_data.count > RX_MAX_KSV_COUNT) {
			myhdmi->rpt_data.count = RX_MAX_KSV_COUNT;
			myhdmi->rpt_data.is_devs_exc = TRUE;
		}
		for (i = 0; i < (myhdmi->rpt_data.count*5); i++)
			myhdmi->rpt_data.list[i] = prbTxKsvlist[i];

		for (i = 0; i < 5; i++)
			myhdmi->rpt_data.list[(myhdmi->rpt_data.count)*5 + i] =
				prbTxBksv[i];

		myhdmi->rpt_data.count = myhdmi->rpt_data.count + 1;
		if (myhdmi->rpt_data.count > RX_MAX_KSV_COUNT) {
			myhdmi->rpt_data.count = RX_MAX_KSV_COUNT;
			myhdmi->rpt_data.is_devs_exc = TRUE;
		}

		myhdmi->rpt_data.depth =
			((u2TxBStatus & DEVICE_DEPTH) >> 8) + 1;
		if (myhdmi->rpt_data.depth > RX_MAX_DEV_COUNT) {
			myhdmi->rpt_data.depth = RX_MAX_DEV_COUNT;
			myhdmi->rpt_data.is_casc_exc = TRUE;
		}
	}

	hdmi2_set_hdcp_auth_done(myhdmi, TRUE);
}

bool
hdmirx_dts_mapping(struct MTK_HDMI *myhdmi, struct device_node *np)
{
	RX_ADR = of_iomap(np, 0);
	if (IS_ERR(RX_ADR)) {
		RX_DEF_LOG("[RX]RX_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]RX_ADR=0x%p\n", RX_ADR);

	TX_ADR = of_iomap(np, 1);
	if (IS_ERR(TX_ADR)) {
		RX_DEF_LOG("[RX]TX_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]TX_ADR=0x%p\n", TX_ADR);

	V_ADR = of_iomap(np, 2);
	if (IS_ERR(V_ADR)) {
		RX_DEF_LOG("[RX]V_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]V_ADR=0x%p\n", V_ADR);

	A_ADR = of_iomap(np, 3);
	if (IS_ERR(A_ADR)) {
		RX_DEF_LOG("[RX]A_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]A_ADR=0x%p\n", A_ADR);

	S_ADR = of_iomap(np, 4);
	if (IS_ERR(S_ADR)) {
		RX_DEF_LOG("[RX]S_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]S_ADR=0x%p\n", S_ADR);

	E_ADR = of_iomap(np, 5);
	if (IS_ERR(E_ADR)) {
		RX_DEF_LOG("[RX]E_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]E_ADR=0x%p\n", E_ADR);

	AP_ADR = of_iomap(np, 6);
	if (IS_ERR(AP_ADR)) {
		RX_DEF_LOG("[RX]AP_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]AP_ADR=0x%p\n", AP_ADR);

	CK_ADR = of_iomap(np, 7);
	if (IS_ERR(CK_ADR)) {
		RX_DEF_LOG("[RX]CK_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]CK_ADR=0x%lx\n", (unsigned long)CK_ADR);

	R_ADR = of_iomap(np, 8);
	if (IS_ERR(R_ADR)) {
		RX_DEF_LOG("[RX]R_ADR fail\n");
		return FALSE;
	}
	//RX_DEF_LOG("[RX]R_ADR=0x%lx\n", (unsigned long)R_ADR);

	return TRUE;
}

void hdmirx_toprgu_rst(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]%s\n", __func__);

	w_reg(R_ADR + 0x90, 0x85010000);
	w_reg(R_ADR + 0x90, 0x85000000);
}

static int hdmirx_release(struct inode *inode,
	struct file *file)
{
	return 0;
}

static int hdmirx_open(struct inode *inode, struct file *filp)
{
	struct MTK_HDMI *myhdmi;

	myhdmi = container_of(inode->i_cdev, struct MTK_HDMI, rx_cdev);
	if (!myhdmi) {
		pr_info("[RX] Cannot find MTK_HDMI\n");
		return -ENOMEM;
	}
	filp->private_data = myhdmi;
	//RX_DEF_LOG("[RX] %s: 0x%lx\n", __func__, (unsigned long)filp->private_data);

	return 0;
}

static long hdmirx_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct MTK_HDMI *myhdmi = (struct MTK_HDMI *)filp->private_data;
	struct HDMIRX_DEV_INFO dev_info;
	struct HDMIRX_VID_PARA vid_para;
	struct HDMIRX_AUD_INFO aud_info;
	struct hdr10InfoPkt hdr10_info;
	unsigned int temp;
	int r = 0;

	//RX_DEF_LOG("[RX] ioctl: cmd=0x%x, 0x%lx\n", cmd, (unsigned long)filp->private_data);

	switch (cmd) {
	case MTK_HDMIRX_DEV_INFO:
		io_get_dev_info(myhdmi, &dev_info);
		if (copy_to_user((void __user *)arg, &dev_info,
			sizeof(struct HDMIRX_DEV_INFO))) {
			RX_DEF_LOG("[RX] failed: %d\n", __LINE__);
			r = -EFAULT;
		}
		break;

	case MTK_HDMIRX_VID_INFO:
		io_get_vid_info(myhdmi, &vid_para);
		if (copy_to_user((void __user *)arg, &vid_para,
			sizeof(struct HDMIRX_VID_PARA))) {
			RX_DEF_LOG("[RX] failed: %d\n", __LINE__);
			r = -EFAULT;
		}
		break;

	case MTK_HDMIRX_AUD_INFO:
		io_get_aud_info(myhdmi, &aud_info);
		if (copy_to_user((void __user *)arg, &aud_info,
			sizeof(struct HDMIRX_AUD_INFO))) {
			RX_DEF_LOG("[RX] failed: %d\n", __LINE__);
			r = -EFAULT;
		}
		break;

	case MTK_HDMIRX_ENABLE:
		if (arg == 1) {
			RX_DEF_LOG("[RX] enable\n");
			hdmi_rx_power_on(myhdmi);
			aud2_enable(myhdmi, TRUE);
		} else {
			RX_DEF_LOG("[RX] disable\n");
			hdmi_rx_power_off(myhdmi);
			aud2_enable(myhdmi, FALSE);
		}
		break;

	case MTK_HDMIRX_SWITCH:
		RX_DEF_LOG("[RX] port from %d to %d\n",
			myhdmi->portswitch, (u8)arg);
		if (myhdmi->portswitch != (u8)arg) {
			myhdmi->portswitch = (u8)arg;
			if (myhdmi->portswitch != 0) {
				//vHDMI21PhyAnaInit(myhdmi);
				aud2_enable(myhdmi, TRUE);
			} else {
				RX_DEF_LOG("[rx]power down\n");
				hdmi2_tmds_term_ctrl(myhdmi, FALSE);
				//vApiHDMI21PhyAnaPowerDownMode(myhdmi);
			}
			hdmi2_set_hpd_low_timer(myhdmi, HDMI2_HPD_Period);
		}
		break;

	case MTK_HDMIRX_PKT:
		io_get_hdr10_info(myhdmi, &hdr10_info);
		if (copy_to_user((void __user *)arg, &hdr10_info,
			sizeof(struct hdr10InfoPkt))) {
			RX_DEF_LOG("[RX] failed: %d\n", __LINE__);
			r = -EFAULT;
		}
		break;

	case MTK_HDMIRX_DRV_VER:
		temp = HDMIRX_DRV_VER;
		if (copy_to_user((void __user *)arg, &temp,
			sizeof(temp))) {
			RX_DEF_LOG("[RX] failed: %d\n", __LINE__);
			r = -EFAULT;
		}
		break;

	default:
		r = -EFAULT;
		RX_DEF_LOG("[RX] Unknown ioctl: 0x%x\n", cmd);
		break;
	}

	return r;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long hdmirx_ioctl_compat(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct HDMIRX_DEV_INFO __user *arg_dev;
	struct HDMIRX_VID_PARA __user *arg_vid;
	struct HDMIRX_AUD_INFO __user *arg_aud;
	struct hdr10InfoPkt __user *arg_hdr10;
	unsigned int __user *arg_k;
	int ret = 0;

	//RX_DEF_LOG("[RX] ioctl: cmd=0x%x, 0x%lx\n",
	//	__func__, cmd, (unsigned long)filp->private_data);

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case CP_MTK_HDMIRX_DEV_INFO:
		arg_dev = compat_ptr(arg);
		filp->f_op->unlocked_ioctl(filp,
			MTK_HDMIRX_DEV_INFO,
			(unsigned long)arg_dev);
		break;

	case CP_MTK_HDMIRX_VID_INFO:
		arg_vid =	compat_ptr(arg);
		filp->f_op->unlocked_ioctl(filp,
			MTK_HDMIRX_VID_INFO,
			(unsigned long)arg_vid);
		break;

	case CP_MTK_HDMIRX_AUD_INFO:
		arg_aud = compat_ptr(arg);
		filp->f_op->unlocked_ioctl(filp,
			MTK_HDMIRX_AUD_INFO,
			(unsigned long)arg_aud);
		break;

	case CP_MTK_HDMIRX_ENABLE:
		arg_k = compat_ptr(arg);
		ret = filp->f_op->unlocked_ioctl(filp,
			MTK_HDMIRX_ENABLE,
			(unsigned long)arg_k);
		break;

	case CP_MTK_HDMIRX_SWITCH:
		arg_k = compat_ptr(arg);
		ret = filp->f_op->unlocked_ioctl(filp,
			MTK_HDMIRX_SWITCH,
			(unsigned long)arg_k);
		break;

	case CP_MTK_HDMIRX_PKT:
		arg_hdr10 = compat_ptr(arg);
		filp->f_op->unlocked_ioctl(filp,
			MTK_HDMIRX_PKT,
			(unsigned long)arg_hdr10);
		break;

	case CP_MTK_HDMIRX_DRV_VER:
		arg_k = compat_ptr(arg);
		ret = filp->f_op->unlocked_ioctl(filp,
			MTK_HDMIRX_DRV_VER,
			(unsigned long)arg_k);
		break;

	default:
		RX_DEF_LOG(">> calling default cmd=0x%x\n", cmd);
		hdmirx_ioctl(filp, cmd, arg);
		break;

	}
	return ret;
}
#endif

static const struct file_operations hdmirx_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hdmirx_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = hdmirx_ioctl_compat,
#endif
	.open = hdmirx_open,
	.release = hdmirx_release,
};

static int
hdmirx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct class_device *class_dev = NULL;
	struct MTK_HDMI *myhdmi = NULL;
	struct MTK_HDMIRX *hdmirx = NULL;
	struct nvmem_cell *cell;
	u32 *pbuf;
	size_t len;
	int ret = 0;

	RX_DEF_LOG("[RX]%s\n", __func__);

	myhdmi = devm_kzalloc(dev, sizeof(struct MTK_HDMI), GFP_KERNEL);
	if (!myhdmi) {
		RX_DEF_LOG("[RX]devm_kzalloc MTK_HDMI err\n");
		return -ENOMEM;
	}
	memset(myhdmi, 0, sizeof(struct MTK_HDMI));
	myhdmi->dev = dev;
	platform_set_drvdata(pdev, myhdmi);

	if (of_device_is_available(np) != 1) {
		RX_DEF_LOG("[RX]find device fail\n");
		return -1;
	}

#if HDMIRX_YOCTO
	/* efuse */
	cell = nvmem_cell_get(myhdmi->dev, "phy-hdmirx");
	if (IS_ERR(cell)) {
		RX_DEF_LOG("[RX]failed to read efuse\n");
		return -1;
	}
	if (cell == NULL) {
		RX_DEF_LOG("[RX]failed to read efuse rterm(cell is NULL)\n");
		return -1;
	}
	pbuf = (unsigned int *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	myhdmi->efuse = pbuf[0];
	kfree(pbuf);
	RX_DEF_LOG("[RX]efuse=0x%x\n", myhdmi->efuse);
	if ((myhdmi->efuse & 0x1) == 1) {
		RX_DEF_LOG("[RX] disable hdmirx, do not init\n");
		return -1;
	}

	/* rterm */
	cell = nvmem_cell_get(myhdmi->dev, "phy-rterm");
	if (IS_ERR(cell)) {
		RX_DEF_LOG("[RX]failed to read efuse rterm\n");
		return -1;
	}
	if (cell == NULL) {
		RX_DEF_LOG("[RX]failed to read efuse rterm(cell is NULL)\n");
		return -1;
	}
	pbuf = (unsigned int *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	myhdmi->rterm = pbuf[0];
	kfree(pbuf);
	RX_DEF_LOG("[RX]rterm=0x%08x\n", myhdmi->rterm);
#endif

	myhdmi->switch_data.name = "hdmi";
	myhdmi->switch_data.index = 0;
	myhdmi->switch_data.state = 0;
	ret = uevent_dev_register(myhdmi, &myhdmi->switch_data);

	myhdmi->switch_aud_data.name = "rxaudio";
	myhdmi->switch_aud_data.index = 0;
	myhdmi->switch_aud_data.state = 0;
	myhdmi->switch_aud_data.aud_fs = 0;
	myhdmi->switch_aud_data.aud_ch = 0;
	myhdmi->switch_aud_data.aud_bit = 0;
	ret = aud_uevent_dev_register(myhdmi, &myhdmi->switch_aud_data);

	/* register cdev */
	ret = alloc_chrdev_region(&myhdmi->rx_devno, 0, 1, HDMIRX_DEVNAME);
	if (ret) {
		RX_DEF_LOG("[RX]alloc_chrdev_region fail\n");
		return -1;
	}
	cdev_init(&myhdmi->rx_cdev, &hdmirx_fops);
	myhdmi->rx_cdev.owner = THIS_MODULE;
	myhdmi->rx_cdev.ops = &hdmirx_fops;
	ret = cdev_add(&myhdmi->rx_cdev, myhdmi->rx_devno, 1);
	myhdmi->rx_class = class_create(THIS_MODULE, HDMIRX_DEVNAME);
	class_dev = (struct class_device *)device_create(myhdmi->rx_class,
		NULL, myhdmi->rx_devno,
		NULL, HDMIRX_DEVNAME);
	//RX_DEF_LOG("[RX]class_dev=0x%lx, class_dev=0x%lx\n",
	//	(unsigned long)myhdmi->rx_class,
	//	(unsigned long)class_dev);

	/************************************************************/

	hdmirx = devm_kzalloc(dev, sizeof(struct MTK_HDMIRX), GFP_KERNEL);
	if (!hdmirx) {
		RX_DEF_LOG("[RX]devm_kzalloc MTK_HDMIRX err\n");
		return -ENOMEM;
	}
	memset(hdmirx, 0, sizeof(struct MTK_HDMIRX));
	hdmirx->dev = myhdmi->dev;
	hdmirx->issue_edid = mtk_issue_edid;
	hdmirx->hdcp_is_doing_auth = mtk_is_upstream_need_auth;
	hdmirx->hdcp_is_rpt = mtk_is_rpt_mode;
	hdmirx->set_ksv = mtk_rx_set_tx_ksv;
	myhdmi->rx = hdmirx;

	/*   reg io map  */
	if (hdmirx_dts_mapping(myhdmi, np) == FALSE)
		return -1;

	/* 3.3v */
	myhdmi->hdmi_v33 = devm_regulator_get(&pdev->dev, "hdmi33v");
	if (IS_ERR(myhdmi->hdmi_v33)) {
		RX_DEF_LOG("[RX]find 3.3v fail\n");
		return -1;
	}
	ret = regulator_set_voltage(myhdmi->hdmi_v33, 3300000, 3300000);
	if (ret) {
		RX_DEF_LOG("[RX]3.3v fail %d\n", ret);
		return ret;
	}
	ret = regulator_enable(myhdmi->hdmi_v33);
	if (ret) {
		RX_DEF_LOG("[RX]hdmi33v regulator enable failed\n");
		return ret;
	}
	/* 0.85v */
	myhdmi->hdmi_v08 = devm_regulator_get(&pdev->dev, "hdmi08v");
	if (IS_ERR(myhdmi->hdmi_v08)) {
		RX_DEF_LOG("[RX]find 0.8v fail\n");
		return -1;
	}
	ret = regulator_set_voltage(myhdmi->hdmi_v08, 800000, 800000);
	if (ret) {
		RX_DEF_LOG("[RX]0.85v fail %d\n", ret);
		return ret;
	}
	ret = regulator_enable(myhdmi->hdmi_v08);
	if (ret) {
		RX_DEF_LOG("[RX]hdmi_v08 regulator enable failed\n");
		return ret;
	}
	/* vcore */
	/* Create opp table from dts */
#if HDMIRX_YOCTO
	dev_pm_opp_of_add_table(myhdmi->dev);
	myhdmi->reg_vcore = devm_regulator_get(myhdmi->dev, "dvfsrc-vcore");
#endif

	/* irq */
	myhdmi->rx_irq = platform_get_irq(pdev, 0);
	RX_DEF_LOG("[RX]hdmi rx irq: %d\n", myhdmi->rx_irq);
	if (myhdmi->rx_irq < 0) {
		RX_DEF_LOG("[RX]failed to request hdmi rx irq\n");
		return -1;
	}
	myhdmi->rx_phy_irq = platform_get_irq(pdev, 1);
	RX_DEF_LOG("[RX]hdmi rx phy irq: %d\n", myhdmi->rx_phy_irq);
	if (myhdmi->rx_phy_irq < 0) {
		RX_DEF_LOG("[RX]failed to request hdmi rx phy irq\n");
		return -1;
	}

	/* debugfs init */
	ret = hdmi2cmd_debug_init(myhdmi);
	if (ret)
		RX_DEF_LOG("[RX]Failed debugfs.\n");

	/* get clk */
	myhdmi->hdmiclksel = devm_clk_get(dev, "hdmiclksel");
	if (IS_ERR(myhdmi->hdmiclksel)) {
		RX_DEF_LOG("[RX]Failed to get hdmiclksel clk\n");
		return PTR_ERR(myhdmi->hdmiclksel);
	}
	myhdmi->hdmiclk208m = devm_clk_get(dev, "hdmiclk208m");
	if (IS_ERR(myhdmi->hdmiclk208m)) {
		RX_DEF_LOG("[RX]Failed to get hdmiclk208m clk\n");
		return PTR_ERR(myhdmi->hdmiclk208m);
	}
	myhdmi->hdcpclksel = devm_clk_get(dev, "hdcpclksel");
	if (IS_ERR(myhdmi->hdcpclksel)) {
		RX_DEF_LOG("[RX]Failed to get hdcpclksel clk\n");
		return PTR_ERR(myhdmi->hdcpclksel);
	}
	myhdmi->hdcpclk104m = devm_clk_get(dev, "hdcpclk104m");
	if (IS_ERR(myhdmi->hdcpclk104m)) {
		RX_DEF_LOG("[RX]Failed to get hdcpclk104m clk\n");
		return PTR_ERR(myhdmi->hdcpclk104m);
	}
	myhdmi->hdmisel = devm_clk_get(dev, "hdmisel");
	if (IS_ERR(myhdmi->hdmisel)) {
		RX_DEF_LOG("[RX]Failed to get hdmisel clk\n");
		return PTR_ERR(myhdmi->hdmisel);
	}
	/* mac and phy PM */
	pm_runtime_enable(myhdmi->dev);

	RX_DEF_LOG("[RX]pdev=0x%lx,dev=0x%lx,phydev=0x%lx,myhdmi=0x%lx\n",
		(unsigned long)pdev,
		(unsigned long)myhdmi->dev,
		(unsigned long)myhdmi->phydev,
		(unsigned long)myhdmi);

	hdmirx_toprgu_rst(myhdmi);

#ifdef HDMIRX_OPTEE_EN
	optee_hdcp_open(myhdmi);
#endif

	myhdmi->portswitch = HDMI_SWITCH_1;
	myhdmi->aud_enable = 0;
	myhdmi->is_rx_init = TRUE;
	myhdmi->is_rx_task_disable = FALSE;
	myhdmi->is_wait_finish = FALSE;
	myhdmi->hdmi_mode_setting = HDMI_RPT_PIXEL_MODE;

	myhdmi->debugtype = 0;
	myhdmi->my_debug = 0;
	myhdmi->my_debug1 = 0;
	myhdmi->my_debug2 = 0;

	hdmi_rx_set_main_state(myhdmi, HDMIRX_IDLE);

	init_waitqueue_head(&myhdmi->main_wq);
	myhdmi->main_task = kthread_create(hdmi_rx_main_kthread,
		myhdmi, "hdmirx_timer_kthread");
	if (IS_ERR(myhdmi->main_task)) {
		myhdmi->main_task = NULL;
		RX_DEF_LOG("[RX]creat thread fail\n");
		return PTR_ERR(myhdmi->main_task);
	}
	wake_up_process(myhdmi->main_task);

	memset((void *)&myhdmi->hdmi_timer, 0, sizeof(myhdmi->hdmi_timer));
	timer_setup(&myhdmi->hdmi_timer, &hdmi_rx_timer_isr, 0);

	hdmi_rx_power_on(myhdmi);

	ret = devm_request_irq(myhdmi->dev,
		myhdmi->rx_irq,
		hdmi_rx_irq,
		IRQF_TRIGGER_HIGH,
		dev_name(myhdmi->dev),
		myhdmi);
	if (ret < 0) {
		RX_DEF_LOG("[RX]Fail hdmi rx irq %d: %d\n",
			myhdmi->rx_irq, ret);
		return ret;
	}
	ret = devm_request_irq(myhdmi->dev,
		myhdmi->rx_phy_irq,
		hdmi_rx_phy_irq,
		IRQF_TRIGGER_HIGH,
		dev_name(myhdmi->dev),
		myhdmi);
	if (ret < 0) {
		RX_DEF_LOG("[RX]Fail hdmi rx phy irq %d: %d\n",
			myhdmi->rx_phy_irq, ret);
		return ret;
	}

	RX_DEF_LOG("[rx]%s done\n", __func__);

	return 0;
}

static int
hdmirx_remove(struct platform_device *pdev)
{
	return 0;
}

static int hdmirx_suspend(struct device *dev)
{
	struct MTK_HDMI *myhdmi = dev_get_drvdata(dev);

	RX_DEF_LOG("[RX] %s\n", __func__);
	if ((myhdmi->efuse & 0x1) == 1) {
		RX_DEF_LOG("[RX] disable hdmirx, do not suspend\n");
		return 0;
	}

	hdmi_rx_power_off(myhdmi);
	return 0;
}

static int hdmirx_resume(struct device *dev)
{
	struct MTK_HDMI *myhdmi = dev_get_drvdata(dev);

	RX_DEF_LOG("[RX] %s\n", __func__);

	if ((myhdmi->efuse & 0x1) == 1) {
		RX_DEF_LOG("[RX] disable hdmirx, do not resume\n");
		return 0;
	}

	hdmi_rx_power_on(myhdmi);
	return 0;
}

static const struct dev_pm_ops hdmirx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hdmirx_suspend, hdmirx_resume)
};

static const struct of_device_id mtk_hdmirx_of_ids[] = {
	{ .compatible = "mediatek,mt8195-hdmirx", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_hdmirx_of_ids);

static struct platform_driver hdmirx_driver = {
	.probe = hdmirx_probe,
	.remove = hdmirx_remove,
	.driver = {
		.name = HDMIRX_DEVNAME,
		.pm = &hdmirx_pm_ops,
		.of_match_table = mtk_hdmirx_of_ids,
	}
};

module_platform_driver(hdmirx_driver);

MODULE_AUTHOR("Bo.Xu <bo.xu@mediatek.com>");
MODULE_DESCRIPTION("HDMI RX Driver");
MODULE_LICENSE("GPL");
