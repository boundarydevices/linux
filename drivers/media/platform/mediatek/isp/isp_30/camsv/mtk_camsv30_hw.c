// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 BayLibre
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "mtk_camsv.h"
#include "mtk_camsv30_regs.h"

#define MTK_CAMSV30_AUTOSUSPEND_DELAY_MS 100

static const struct mtk_cam_conf camsv30_conf = {
	.tg_sen_mode = 0x00010002U, /* TIME_STP_EN = 1. DBL_DATA_BUS = 1 */
	.module_en = 0x40000001U, /* enable double buffer and TG */
	.imgo_con = 0x80000080U, /* DMA FIFO depth and burst */
	.imgo_con2 = 0x00020002U, /* DMA priority */
};

static void fmt_to_sparams(u32 mbus_fmt, struct mtk_cam_sparams *sparams)
{
	switch (mbus_fmt) {
	/* SBGGR values coming from isp5.0 configuration.
	 * not tested on isp2.0
	 */
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		sparams->w_factor = 1;
		sparams->module_en_pak = 0x4;
		sparams->fmt_sel = 0x2;
		sparams->pak = 0x5;
		sparams->imgo_stride = 0x000B0000;
		break;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		sparams->w_factor = 1;
		sparams->module_en_pak = 0x4;
		sparams->fmt_sel = 0x1;
		sparams->pak = 0x6;
		sparams->imgo_stride = 0x000B0000;
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		sparams->w_factor = 1;
		sparams->module_en_pak = 0x4;
		sparams->fmt_sel = 0x0;
		sparams->pak = 0x7;
		sparams->imgo_stride = 0x000B0000;
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		sparams->w_factor = 2;
		sparams->module_en_pak = 0x8;
		sparams->fmt_sel = 0x1000003;
		sparams->pak = 0x0;
		sparams->imgo_stride = 0x00090000;
		break;
	default:
		break;
	}
}

static void mtk_camsv30_update_buffers_add(struct mtk_cam_dev *cam_dev,
					   struct mtk_cam_dev_buffer *buf)
{
	writel(buf->daddr, cam_dev->regs_img0 + CAMSV_IMGO_SV_BASE_ADDR);

	writel(0x1U, cam_dev->regs + CAMSV_IMGO_FBC);
}

static void mtk_camsv30_cmos_vf_hw_enable(struct mtk_cam_dev *cam_dev,
					  bool pak_en)
{
	u32 clk_en;

	clk_en = CAMSV_TG_DP_CLK_EN | CAMSV_DMA_DP_CLK_EN;
	if (pak_en)
		clk_en |= CAMSV_PAK_DP_CLK_EN;
	writel(clk_en, cam_dev->regs + CAMSV_CLK_EN);
	writel(readl(cam_dev->regs_tg + CAMSV_TG_VF_CON) | CAMSV_TG_VF_CON_VFDATA_EN,
			cam_dev->regs_tg + CAMSV_TG_VF_CON);
}

static void mtk_camsv30_cmos_vf_hw_disable(struct mtk_cam_dev *cam_dev,
					   bool pak_en)
{
	writel(readl(cam_dev->regs_tg + CAMSV_TG_SEN_MODE) & ~CAMSV_TG_SEN_MODE_CMOS_EN,
			cam_dev->regs_tg + CAMSV_TG_SEN_MODE);
	writel(readl(cam_dev->regs_tg + CAMSV_TG_VF_CON) & ~CAMSV_TG_VF_CON_VFDATA_EN,
			cam_dev->regs_tg + CAMSV_TG_VF_CON);
}

static void mtk_camsv30_setup(struct mtk_cam_dev *cam_dev, u32 w, u32 h,
			      u32 bpl, u32 mbus_fmt)
{
	const struct mtk_cam_conf *conf = cam_dev->conf;
	u32 int_en = INT_ST_MASK_CAMSV;
	u32 tmp;
	struct mtk_cam_sparams sparams;

	fmt_to_sparams(mbus_fmt, &sparams);

	if (pm_runtime_resume_and_get(cam_dev->dev) < 0) {
		dev_err(cam_dev->dev, "failed to get pm_runtime\n");
		return;
	}

	spin_lock_irq(&cam_dev->irqlock);

	writel(conf->tg_sen_mode, cam_dev->regs_tg + CAMSV_TG_SEN_MODE);

	writel((w * sparams.w_factor) << 16U, cam_dev->regs_tg + CAMSV_TG_SEN_GRAB_PXL);

	writel(h << 16U, cam_dev->regs_tg + CAMSV_TG_SEN_GRAB_LIN);

	/* YUV_U2S_DIS: disable YUV sensor unsigned to signed */
	writel(0x1000U, cam_dev->regs_tg + CAMSV_TG_PATH_CFG);

	/* Reset cam */
	writel(CAMSV_SW_RST, cam_dev->regs + CAMSV_SW_CTL);
	writel(0x0U, cam_dev->regs + CAMSV_SW_CTL);
	writel(CAMSV_IMGO_RST_TRIG, cam_dev->regs + CAMSV_SW_CTL);

	readl_poll_timeout_atomic(cam_dev->regs + CAMSV_SW_CTL, tmp,
			(tmp == (CAMSV_IMGO_RST_TRIG | CAMSV_IMGO_RST_ST)), 10, 200);

	writel(0x0U, cam_dev->regs + CAMSV_SW_CTL);

	writel(int_en, cam_dev->regs + CAMSV_INT_EN);

	writel(conf->module_en | sparams.module_en_pak,
	      cam_dev->regs + CAMSV_MODULE_EN);
	writel(sparams.fmt_sel, cam_dev->regs + CAMSV_FMT_SEL);
	writel(sparams.pak, cam_dev->regs + CAMSV_PAK);

	writel(bpl - 1U, cam_dev->regs_img0 + CAMSV_IMGO_SV_XSIZE);
	writel(h - 1U, cam_dev->regs_img0 + CAMSV_IMGO_SV_YSIZE);

	writel(sparams.imgo_stride | bpl, cam_dev->regs_img0 + CAMSV_IMGO_SV_STRIDE);

	writel(conf->imgo_con, cam_dev->regs_img0 + CAMSV_IMGO_SV_CON);
	writel(conf->imgo_con2, cam_dev->regs_img0 + CAMSV_IMGO_SV_CON2);

	/* CMOS_EN first */
	writel(readl(cam_dev->regs_tg + CAMSV_TG_SEN_MODE) | CAMSV_TG_SEN_MODE_CMOS_EN,
			cam_dev->regs_tg + CAMSV_TG_SEN_MODE);

	/* finally, CAMSV_MODULE_EN : IMGO_EN */
	writel(readl(cam_dev->regs + CAMSV_MODULE_EN) | CAMSV_MODULE_EN_IMGO_EN,
		    cam_dev->regs + CAMSV_MODULE_EN);

	spin_unlock_irq(&cam_dev->irqlock);
	pm_runtime_put_autosuspend(cam_dev->dev);
}

static irqreturn_t isp_irq_camsv30(int irq, void *data)
{
	struct mtk_cam_dev *cam_dev = (struct mtk_cam_dev *)data;
	struct mtk_cam_dev_buffer *buf;
	unsigned int irq_status;

	spin_lock(&cam_dev->irqlock);

	irq_status = readl(cam_dev->regs + CAMSV_INT_STATUS);

	if (irq_status & INT_ST_MASK_CAMSV_ERR) {
		dev_err(cam_dev->dev, "irq error 0x%x\n",
			(unsigned int)(irq_status & INT_ST_MASK_CAMSV_ERR));
	}

	/* De-queue frame */
	if (irq_status & CAMSV_IRQ_PASS1_DON) {
		cam_dev->sequence++;

		if (!cam_dev->is_dummy_used) {
			buf = list_first_entry_or_null(&cam_dev->buf_list,
						       struct mtk_cam_dev_buffer,
						       list);
			if (buf) {
				buf->v4l2_buf.sequence = cam_dev->sequence;
				buf->v4l2_buf.vb2_buf.timestamp = ktime_get_ns();
				vb2_buffer_done(&buf->v4l2_buf.vb2_buf,
						VB2_BUF_STATE_DONE);
				list_del(&buf->list);
			}
		}

		if (list_empty(&cam_dev->buf_list)) {
			mtk_camsv30_update_buffers_add(cam_dev, &cam_dev->dummy);
			cam_dev->is_dummy_used = true;
		} else {
			buf = list_first_entry_or_null(&cam_dev->buf_list,
						       struct mtk_cam_dev_buffer,
						       list);
			mtk_camsv30_update_buffers_add(cam_dev, buf);
			cam_dev->is_dummy_used = false;
		}
	}

	spin_unlock(&cam_dev->irqlock);

	return IRQ_HANDLED;
}

static int mtk_camsv30_runtime_suspend(struct device *dev)
{
	struct mtk_cam_dev *cam_dev = dev_get_drvdata(dev);
	struct vb2_queue *vbq = &cam_dev->vdev.vbq;

	if (vb2_is_streaming(vbq)) {
		mutex_lock(&cam_dev->op_lock);
		v4l2_subdev_call(&cam_dev->subdev, video, s_stream, 0);
		mutex_unlock(&cam_dev->op_lock);
	}

	clk_bulk_disable_unprepare(cam_dev->num_clks, cam_dev->clks);

	return 0;
}

static int mtk_camsv30_runtime_resume(struct device *dev)
{
	struct mtk_cam_dev *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_video_device *vdev = &cam_dev->vdev;
	const struct v4l2_pix_format_mplane *fmt = &vdev->format;
	struct vb2_queue *vbq = &vdev->vbq;
	struct mtk_cam_dev_buffer *buf, *buf_prev;
	int ret;
	unsigned long flags = 0;

	ret = clk_bulk_prepare_enable(cam_dev->num_clks, cam_dev->clks);
	if (ret) {
		dev_err(dev, "failed to enable clock:%d\n", ret);
		return ret;
	}

	if (vb2_is_streaming(vbq)) {

		mtk_camsv30_setup(cam_dev, fmt->width, fmt->height,
				  fmt->plane_fmt[0].bytesperline, vdev->fmtinfo->code);

		spin_lock_irqsave(&cam_dev->irqlock, flags);
		buf = list_first_entry_or_null(&cam_dev->buf_list,
					       struct mtk_cam_dev_buffer,
					       list);
		if (buf) {
			mtk_camsv30_update_buffers_add(cam_dev, buf);
			cam_dev->is_dummy_used = false;
		} else {
			mtk_camsv30_update_buffers_add(cam_dev, &cam_dev->dummy);
			cam_dev->is_dummy_used = true;
		}

		mtk_camsv30_cmos_vf_hw_enable(cam_dev, vdev->fmtinfo->packed);

		spin_unlock_irqrestore(&cam_dev->irqlock, flags);

		/* Stream on the sub-device */
		mutex_lock(&cam_dev->op_lock);
		ret = v4l2_subdev_call(&cam_dev->subdev, video, s_stream, 1);

		if (ret) {
			cam_dev->stream_count--;
			if (cam_dev->stream_count == 0)
				video_device_pipeline_stop(&vdev->vdev);
		}
		mutex_unlock(&cam_dev->op_lock);

		if (ret)
			goto fail_no_stream;
	}

	return 0;

fail_no_stream:
	spin_lock_irqsave(&cam_dev->irqlock, flags);
	list_for_each_entry_safe(buf, buf_prev, &cam_dev->buf_list, list) {
		buf->daddr = 0ULL;
		list_del(&buf->list);
		vb2_buffer_done(&buf->v4l2_buf.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&cam_dev->irqlock, flags);
	return ret;
}

static struct mtk_cam_hw_functions mtk_camsv30_hw_functions = {
	.mtk_cam_setup = mtk_camsv30_setup,
	.mtk_cam_update_buffers_add = mtk_camsv30_update_buffers_add,
	.mtk_cam_cmos_vf_hw_enable = mtk_camsv30_cmos_vf_hw_enable,
	.mtk_cam_cmos_vf_hw_disable = mtk_camsv30_cmos_vf_hw_disable,
};

static int mtk_camsv30_probe(struct platform_device *pdev)
{
	static const char * const clk_names[] = { "cam", "camtg", "camsv"};

	struct mtk_cam_dev *cam_dev;
	struct device *dev = &pdev->dev;
	int ret;
	int i;

	if (!iommu_present(&platform_bus_type))
		return -EPROBE_DEFER;

	cam_dev = devm_kzalloc(dev, sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;

	cam_dev->conf = of_device_get_match_data(dev);
	if (!cam_dev->conf)
		return -ENODEV;

	cam_dev->dev = dev;
	dev_set_drvdata(dev, cam_dev);

	cam_dev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(cam_dev->regs))
		return dev_err_probe(dev, PTR_ERR(cam_dev->regs),
				"failed to map register base\n");


	cam_dev->regs_img0 = devm_platform_ioremap_resource(pdev, 1);

	if (IS_ERR(cam_dev->regs_img0))
		return dev_err_probe(dev, PTR_ERR(cam_dev->regs_img0),
				"failed to map img0 register base\n");


	cam_dev->regs_tg = devm_platform_ioremap_resource(pdev, 2);
	if (IS_ERR(cam_dev->regs_tg))
		return dev_err_probe(dev, PTR_ERR(cam_dev->regs_tg),
				"failed to map TG register base\n");


	cam_dev->num_clks = ARRAY_SIZE(clk_names);
	cam_dev->clks = devm_kcalloc(dev, cam_dev->num_clks,
				     sizeof(*cam_dev->clks), GFP_KERNEL);
	if (!cam_dev->clks)
		return -ENOMEM;

	for (i = 0; i < cam_dev->num_clks; ++i)
		cam_dev->clks[i].id = clk_names[i];

	ret = devm_clk_bulk_get(dev, cam_dev->num_clks, cam_dev->clks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get clocks: %i\n", ret);


	cam_dev->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, cam_dev->irq,
			       isp_irq_camsv30, 0,
			       dev_name(dev), cam_dev);
	if (ret != 0)
		return dev_err_probe(dev, -ENODEV, "failed to request irq=%d\n",
				cam_dev->irq);

	cam_dev->hw_functions = &mtk_camsv30_hw_functions;

	spin_lock_init(&cam_dev->irqlock);

	/* initialise runtime power management */
	pm_runtime_set_autosuspend_delay(dev, MTK_CAMSV30_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);

	/* Initialize the v4l2 common part */
	return mtk_cam_dev_init(cam_dev);
}

static int mtk_camsv30_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_dev *cam_dev = dev_get_drvdata(dev);

	mtk_cam_dev_cleanup(cam_dev);
	pm_runtime_put_autosuspend(dev);
	pm_runtime_disable(dev);

	return 0;
}

static const struct dev_pm_ops mtk_camsv30_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(mtk_camsv30_runtime_suspend,
			   mtk_camsv30_runtime_resume, NULL)
};

static const struct of_device_id mtk_camsv30_of_ids[] = {
	{ .compatible = "mediatek,mt8365-camsv", .data = &camsv30_conf },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk_camsv30_of_ids);

static struct platform_driver mtk_camsv30_driver = {
	.probe = mtk_camsv30_probe,
	.remove = mtk_camsv30_remove,
	.driver = {
		.name = "mtk-camsv-isp30",
		.of_match_table = mtk_camsv30_of_ids,
		.pm = &mtk_camsv30_pm_ops,
	}
};

module_platform_driver(mtk_camsv30_driver);

MODULE_DESCRIPTION("MediaTek CAMSV ISP3.0 driver");
MODULE_AUTHOR("Florian Sylvestre <fsylvestre@baylibre.com>");
MODULE_LICENSE("GPL");
