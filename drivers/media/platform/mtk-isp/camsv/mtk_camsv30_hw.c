// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 BayLibre
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
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
	.dma_special_fun = 0x61000000U, /* enable RDMA insterlace */
	.imgo_con = 0x80000080U, /* DMA FIFO depth and burst */
	.imgo_con2 = 0x00020002U, /* DMA priority */
	.imgo_con3 = 0x00020002U, /* DMA pre-priority */
	.enableFH = false, /* Frame Header disabled */
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

static u32 mtk_camsv30_read(struct mtk_cam_dev *priv, u32 reg)
{
	return readl(priv->regs + reg);
}

static void mtk_camsv30_write(struct mtk_cam_dev *priv, u32 reg, u32 value)
{
	writel(value, priv->regs + reg);
}

static void mtk_camsv30_img0_write(struct mtk_cam_dev *priv, u32 reg, u32 value)
{
	writel(value, priv->regs_img0 + reg);
}

static u32 mtk_camsv30_tg_read(struct mtk_cam_dev *priv, u32 reg)
{
	return readl(priv->regs_tg + reg);
}

static void mtk_camsv30_tg_write(struct mtk_cam_dev *priv, u32 reg, u32 value)
{
	writel(value, priv->regs_tg + reg);
}

static void mtk_camsv30_update_buffers_add(struct mtk_cam_dev *cam_dev,
					   struct mtk_cam_dev_buffer *buf)
{
	mtk_camsv30_img0_write(cam_dev, CAMSV_IMGO_SV_BASE_ADDR, buf->daddr);

	mtk_camsv30_write(cam_dev, CAMSV_IMGO_FBC, 0x1U);
}

static void mtk_camsv30_cmos_vf_hw_enable(struct mtk_cam_dev *cam_dev,
					  bool pak_en)
{
	u32 clk_en;

	clk_en = CAMSV_TG_DP_CLK_EN | CAMSV_DMA_DP_CLK_EN;
	if (pak_en)
		clk_en |= CAMSV_PAK_DP_CLK_EN;
	mtk_camsv30_write(cam_dev, CAMSV_CLK_EN, clk_en);
	mtk_camsv30_tg_write(cam_dev, CAMSV_TG_VF_CON,
			     mtk_camsv30_tg_read(cam_dev, CAMSV_TG_VF_CON) |
			     CAMSV_TG_VF_CON_VFDATA_EN);
}

static void mtk_camsv30_cmos_vf_hw_disable(struct mtk_cam_dev *cam_dev,
					   bool pak_en)
{
	mtk_camsv30_tg_write(cam_dev, CAMSV_TG_SEN_MODE,
			     mtk_camsv30_tg_read(cam_dev, CAMSV_TG_SEN_MODE) &
			     ~CAMSV_TG_SEN_MODE_CMOS_EN);
	mtk_camsv30_tg_write(cam_dev, CAMSV_TG_VF_CON,
			     mtk_camsv30_tg_read(cam_dev, CAMSV_TG_VF_CON) &
			     ~CAMSV_TG_VF_CON_VFDATA_EN);
}

static void mtk_camsv30_setup(struct mtk_cam_dev *cam_dev, u32 w, u32 h,
			      u32 bpl, u32 mbus_fmt)
{
	const struct mtk_cam_conf *conf = cam_dev->conf;
	int poll_num = 1000;
	u32 int_en = INT_ST_MASK_CAMSV;
	struct mtk_cam_sparams sparams = {0};

	fmt_to_sparams(mbus_fmt, &sparams);

	spin_lock(&cam_dev->irqlock);

	if (pm_runtime_get_sync(cam_dev->dev) < 0) {
		dev_err(cam_dev->dev, "failed to get pm_runtime\n");
		spin_unlock(&cam_dev->irqlock);
		return;
	}

	mtk_camsv30_tg_write(cam_dev, CAMSV_TG_SEN_MODE, conf->tg_sen_mode);

	mtk_camsv30_tg_write(cam_dev,
			     CAMSV_TG_SEN_GRAB_PXL, (w * sparams.w_factor) << 16U);

	mtk_camsv30_tg_write(cam_dev, CAMSV_TG_SEN_GRAB_LIN, h << 16U);

	/* YUV_U2S_DIS: disable YUV sensor unsigned to signed */
	mtk_camsv30_tg_write(cam_dev, CAMSV_TG_PATH_CFG, 0x1000U);

	/* Reset cam */
	mtk_camsv30_write(cam_dev, CAMSV_SW_CTL, CAMSV_SW_RST);
	mtk_camsv30_write(cam_dev, CAMSV_SW_CTL, 0x0U);
	mtk_camsv30_write(cam_dev, CAMSV_SW_CTL, CAMSV_IMGO_RST_TRIG);

	while (mtk_camsv30_read(cam_dev, CAMSV_SW_CTL) !=
		       (CAMSV_IMGO_RST_TRIG | CAMSV_IMGO_RST_ST) &&
	       poll_num++ < 1000)
		;

	mtk_camsv30_write(cam_dev, CAMSV_SW_CTL, 0x0U);

	mtk_camsv30_write(cam_dev, CAMSV_INT_EN, int_en);

	mtk_camsv30_write(cam_dev, CAMSV_MODULE_EN,
			  conf->module_en | sparams.module_en_pak);
	mtk_camsv30_write(cam_dev, CAMSV_FMT_SEL, sparams.fmt_sel);
	mtk_camsv30_write(cam_dev, CAMSV_PAK, sparams.pak);

	mtk_camsv30_img0_write(cam_dev, CAMSV_IMGO_SV_XSIZE, bpl - 1U);
	mtk_camsv30_img0_write(cam_dev, CAMSV_IMGO_SV_YSIZE, h - 1U);

	mtk_camsv30_img0_write(cam_dev, CAMSV_IMGO_SV_STRIDE, sparams.imgo_stride | bpl);

	mtk_camsv30_img0_write(cam_dev, CAMSV_IMGO_SV_CON, conf->imgo_con);
	mtk_camsv30_img0_write(cam_dev, CAMSV_IMGO_SV_CON2, conf->imgo_con2);

	/* CMOS_EN first */
	mtk_camsv30_tg_write(cam_dev, CAMSV_TG_SEN_MODE,
			     mtk_camsv30_tg_read(cam_dev, CAMSV_TG_SEN_MODE) | 0x1U);

	/* finally, CAMSV_MODULE_EN : IMGO_EN */
	mtk_camsv30_write(cam_dev, CAMSV_MODULE_EN,
			  mtk_camsv30_read(cam_dev, CAMSV_MODULE_EN) | 0x00000010U);

	pm_runtime_put_autosuspend(cam_dev->dev);
	spin_unlock(&cam_dev->irqlock);
}

static irqreturn_t isp_irq_camsv30(int irq, void *data)
{
	struct mtk_cam_dev *cam_dev = (struct mtk_cam_dev *)data;
	struct mtk_cam_dev_buffer *buf;
	unsigned long flags = 0;
	unsigned int irq_status;

	spin_lock_irqsave(&cam_dev->irqlock, flags);

	irq_status = mtk_camsv30_read(cam_dev, CAMSV_INT_STATUS);

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
			(*cam_dev->hw_functions->mtk_cam_update_buffers_add)
						(cam_dev, &cam_dev->dummy);
			cam_dev->is_dummy_used = true;
		} else {
			buf = list_first_entry_or_null(&cam_dev->buf_list,
						       struct mtk_cam_dev_buffer,
						       list);
			(*cam_dev->hw_functions->mtk_cam_update_buffers_add)
						(cam_dev, buf);
			cam_dev->is_dummy_used = false;
		}
	}

	spin_unlock_irqrestore(&cam_dev->irqlock, flags);

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

	ret = clk_bulk_prepare_enable(cam_dev->num_clks, cam_dev->clks);
	if (ret) {
		dev_err(dev, "failed to enable clock:%d\n", ret);
		return ret;
	}

	if (vb2_is_streaming(vbq)) {
		mutex_lock(&cam_dev->protect_mutex);

		mtk_camsv30_setup(cam_dev, fmt->width, fmt->height,
				  fmt->plane_fmt[0].bytesperline, vdev->fmtinfo->code);

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

		mutex_unlock(&cam_dev->protect_mutex);

		/* Stream on the sub-device */
		mutex_lock(&cam_dev->op_lock);
		ret = v4l2_subdev_call(&cam_dev->subdev, video, s_stream, 1);

		if (ret) {
			cam_dev->stream_count--;
			if (cam_dev->stream_count == 0)
				media_pipeline_stop(vdev->vdev.entity.pads);
		}
		mutex_unlock(&cam_dev->op_lock);

		if (ret)
			goto fail_no_stream;
	}

	return 0;

fail_no_stream:
	mutex_lock(&cam_dev->protect_mutex);
	list_for_each_entry_safe(buf, buf_prev, &cam_dev->buf_list, list) {
		buf->daddr = 0ULL;
		list_del(&buf->list);
		vb2_buffer_done(&buf->v4l2_buf.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	mutex_unlock(&cam_dev->protect_mutex);
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
	static const char * const clk_names[] = {
		"camsys_cam_cgpdn",
		"camsys_camtg_cgpdn",
		"camsys_camsv"
	};

	struct mtk_cam_dev *cam_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cam_dev->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(cam_dev->regs)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(cam_dev->regs);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	cam_dev->regs_img0 = devm_ioremap_resource(dev, res);
	if (IS_ERR(cam_dev->regs_img0)) {
		dev_err(dev, "failed to map img0 register base\n");
		return PTR_ERR(cam_dev->regs_img0);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	cam_dev->regs_tg = devm_ioremap_resource(dev, res);
	if (IS_ERR(cam_dev->regs_tg)) {
		dev_err(dev, "failed to map TG register base\n");
		return PTR_ERR(cam_dev->regs_tg);
	}

	cam_dev->num_clks = ARRAY_SIZE(clk_names);
	cam_dev->clks = devm_kcalloc(dev, cam_dev->num_clks,
				     sizeof(*cam_dev->clks), GFP_KERNEL);
	if (!cam_dev->clks)
		return -ENOMEM;

	for (i = 0; i < cam_dev->num_clks; ++i)
		cam_dev->clks[i].id = clk_names[i];

	ret = devm_clk_bulk_get(dev, cam_dev->num_clks, cam_dev->clks);
	if (ret) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	cam_dev->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, cam_dev->irq,
			       isp_irq_camsv30, 0,
			       dev_name(dev), cam_dev);
	if (ret != 0) {
		dev_err(dev, "failed to request irq=%d\n", cam_dev->irq);
		return -ENODEV;
	}

	cam_dev->hw_functions = &mtk_camsv30_hw_functions;

	/* initialise protection mutex */
	spin_lock_init(&cam_dev->irqlock);

	/* initialise runtime power management */
	pm_runtime_set_autosuspend_delay(dev, MTK_CAMSV30_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

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
	{
		.compatible = "mediatek,mt8365-camsv",
		.data = &camsv30_conf,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_camsv30_of_ids);

static struct platform_driver mtk_camsv30_driver = {
	.probe = mtk_camsv30_probe,
	.remove = mtk_camsv30_remove,
	.driver = {
		.name = "mtk-camsv-isp30",
		.of_match_table = of_match_ptr(mtk_camsv30_of_ids),
		.pm = &mtk_camsv30_pm_ops,
	}
};

module_platform_driver(mtk_camsv30_driver);

MODULE_DESCRIPTION("MediaTek CAMSV ISP3.0 driver");
MODULE_AUTHOR("Florian Sylvestre <fsylvestre@baylibre.com>");
MODULE_LICENSE("GPL v2");
