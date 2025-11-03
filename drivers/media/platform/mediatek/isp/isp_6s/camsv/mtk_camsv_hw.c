// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
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
#include "mtk_camsv_regs.h"

#define MTK_CAMSV_AUTOSUSPEND_DELAY_MS 100

static const struct mtk_cam_conf camsv_conf = {
	.tg_sen_mode = 0x00010002U, /* TIME_STP_EN = 1. DBL_DATA_BUS = 1 */
	.module_en = 0x50000001U, /* enable double buffer(DB_LOAD_SRC:SOF) and TG */
	.dma_special_fun = 0x61000000U, /* enable RDMA insterlace */
	.imgo_con = 0x80000080U, /* DMA FIFO depth and burst */
	.imgo_con2 = 0x00400030U, /* DMA priority */
	.imgo_con3 = 0x00200010U, /* DMA pre-priority */
	.enableFH = false, /* Frame Header disabled */
};

static void fmt_to_sparams(u32 mbus_fmt, struct mtk_cam_sparams *sparams)
{
	switch (mbus_fmt) {
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		sparams->w_factor = 1;
		sparams->module_en_pak = 0x8; /* Select the source from TGB or PAK to FBC */
		sparams->fmt_sel = 0x2; /* TG format select */
		sparams->pak = 0x22; /* pake_mode & pak_dbl_mode */
		sparams->imgo_stride = 0x0; /* stride setting */
		break;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		sparams->w_factor = 1;
		sparams->module_en_pak = 0x8;
		sparams->fmt_sel = 0x1;
		sparams->pak = 0x21;
		sparams->imgo_stride = 0x0;
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		sparams->w_factor = 1;
		sparams->module_en_pak = 0x8;
		sparams->fmt_sel = 0x0;
		sparams->pak = 0x20;
		sparams->imgo_stride = 0x0;
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		sparams->w_factor = 2;
		sparams->module_en_pak = 0x8;
		sparams->fmt_sel = 0x3;
		sparams->pak = 0x0;
		sparams->imgo_stride = 0x08010000;
		break;
	default:
		break;
	}
}

static u32 mtk_camsv_read(struct mtk_cam_dev *priv, u32 reg)
{
	return readl(priv->regs + reg);
}

static void mtk_camsv_write(struct mtk_cam_dev *priv, u32 reg, u32 value)
{
	writel(value, priv->regs + reg);
}

static void mtk_camsv_update_buffers_add(struct mtk_cam_dev *cam_dev,
				struct mtk_cam_dev_buffer *buf)
{
	mtk_camsv_write(cam_dev, CAMSV_IMGO_BASE_ADDR, buf->daddr);
	mtk_camsv_write(cam_dev, CAMSV_IMGO_FBC, 0x11U);

	dev_dbg(cam_dev->dev, "%s enque imgo base addr: 0x%llx imgo_ctl2: 0x%llx camsv_imgo_fbc: 0x%llx\n",
		 __func__,
		 mtk_camsv_read(cam_dev, CAMSV_IMGO_BASE_ADDR),
		 mtk_camsv_read(cam_dev, CAMSV_FBC_IMGO_CTL2),
		 mtk_camsv_read(cam_dev, CAMSV_IMGO_FBC));
}

static void mtk_camsv_cmos_vf_hw_enable(struct mtk_cam_dev *cam_dev, bool pak_en)
{
	u32 clk_en;

	clk_en = CAMSV_TG_DP_CLK_EN | CAMSV_DMA_DP_CLK_EN;
	if (pak_en)
		clk_en |= CAMSV_PAK_DP_CLK_EN;
	mtk_camsv_write(cam_dev, CAMSV_CLK_EN, clk_en);

	// hw issuse: db_en must be off then on before vf on
	mtk_camsv_write(cam_dev, CAMSV_MODULE_EN,
			mtk_camsv_read(cam_dev, CAMSV_MODULE_EN) &
			     ~CAMSV_MODULE_EN_DB_EN);
	dev_info(cam_dev->dev, "%s: DB_EN off CAMSV_MODULE_EN:0x%llx\n",
		 __func__, mtk_camsv_read(cam_dev,
		 CAMSV_MODULE_EN));
	mtk_camsv_write(cam_dev, CAMSV_MODULE_EN,
			mtk_camsv_read(cam_dev, CAMSV_MODULE_EN) |
			     CAMSV_MODULE_EN_DB_EN);
	dev_info(cam_dev->dev, "%s: DB_EN on CAMSV_MODULE_EN:0x%llx\n",
		 __func__, mtk_camsv_read(cam_dev,
		 CAMSV_MODULE_EN));

	clk_en = mtk_camsv_read(cam_dev, CAMSV_TG_VF_CON)
	      | CAMSV_TG_VF_CON_VFDATA_EN;
	mtk_camsv_write(cam_dev, CAMSV_TG_VF_CON, clk_en);
}

static void mtk_camsv_cmos_vf_hw_disable(struct mtk_cam_dev *cam_dev, bool pak_en)
{
	mtk_camsv_write(cam_dev, CAMSV_TG_SEN_MODE,
			     mtk_camsv_read(cam_dev, CAMSV_TG_SEN_MODE) &
			     ~CAMSV_TG_SEN_MODE_CMOS_EN);

	mtk_camsv_write(cam_dev, CAMSV_TG_VF_CON,
			     mtk_camsv_read(cam_dev, CAMSV_TG_VF_CON) &
			     ~CAMSV_TG_VF_CON_VFDATA_EN);
}

static void mtk_camsv_setup(struct mtk_cam_dev *cam_dev, u32 w, u32 h, u32 bpl,
		     u32 mbus_fmt)
{
	const struct mtk_cam_conf *conf = cam_dev->conf;
	u32 int_en = INT_ST_MASK_CAMSV;
	struct mtk_cam_sparams sparams = {0};
	const u32 mask = CAMSV_IMGO_RST_TRIG | CAMSV_IMGO_RST_ST;
	u32 val;
	int ret;
	unsigned long flags = 0;

	fmt_to_sparams(mbus_fmt, &sparams);

	dev_info(cam_dev->dev, "%s power on camsv\n", __func__);
	if (pm_runtime_get_sync(cam_dev->dev) < 0) {
		dev_err(cam_dev->dev, "failed to get pm_runtime\n");
		return;
	}

	spin_lock_irqsave(&cam_dev->irqlock, flags);

	mtk_camsv_write(cam_dev, CAMSV_TG_SEN_MODE, conf->tg_sen_mode);

	mtk_camsv_write(cam_dev, CAMSV_TG_TIME_STAMP_CTL, 0x1U);

	mtk_camsv_write(cam_dev, CAMSV_TG_SEN_GRAB_PXL, (w * sparams.w_factor) << 16U);

	mtk_camsv_write(cam_dev, CAMSV_TG_SEN_GRAB_LIN, h << 16U);

	/* Reset CAMSV */
	mtk_camsv_write(cam_dev, CAMSV_SW_CTL, CAMSV_SW_RST);
	mtk_camsv_write(cam_dev, CAMSV_SW_CTL, 0x0U);
	mtk_camsv_write(cam_dev, CAMSV_SW_CTL, CAMSV_IMGO_RST_TRIG);

	ret = readl_poll_timeout_atomic(cam_dev->regs + CAMSV_SW_CTL, val,
					(val & mask) == mask, 10, 100000);
	if (ret) {
		dev_err(cam_dev->dev, "CAMSV reset failed, CAMSV_SW_CTL: 0x%x\n", val);
		return;
	}

	mtk_camsv_write(cam_dev, CAMSV_SW_CTL, 0x0U);

	mtk_camsv_write(cam_dev, CAMSV_INT_EN, int_en);

	mtk_camsv_write(cam_dev, CAMSV_MODULE_EN, conf->module_en | sparams.module_en_pak);
	mtk_camsv_write(cam_dev, CAMSV_FMT_SEL, sparams.fmt_sel);
	mtk_camsv_write(cam_dev, CAMSV_PAK, sparams.pak);

	/* Disable frame header since the default value on different chips varies */
	mtk_camsv_write(cam_dev, CAMSV_DMA_FH_EN, 0x0U);

	mtk_camsv_write(cam_dev, CAMSV_DMA_RSV1,
			mtk_camsv_read(cam_dev, CAMSV_DMA_RSV1) & 0x7fffffff);
	mtk_camsv_write(cam_dev, CAMSV_DMA_RSV6, 0xffffffffU);

	/* DMA performance : CQ ultra LSCI and BPCI. Multiplane ID */
	mtk_camsv_write(cam_dev, CAMSV_SPECIAL_FUN_EN, conf->dma_special_fun);

	mtk_camsv_write(cam_dev, CAMSV_FBC_IMGO_CTL1, 0x0U);
	// mtk_camsv_write(cam_dev, CAMSV_FBC_IMGO_CTL1, 0x00010000U);

	mtk_camsv_write(cam_dev, CAMSV_IMGO_XSIZE, bpl - 1U);
	mtk_camsv_write(cam_dev, CAMSV_IMGO_YSIZE, h - 1U);

	mtk_camsv_write(cam_dev, CAMSV_IMGO_STRIDE, sparams.imgo_stride | bpl);

	mtk_camsv_write(cam_dev, CAMSV_IMGO_CON, conf->imgo_con);
	mtk_camsv_write(cam_dev, CAMSV_IMGO_CON2, conf->imgo_con2);
	mtk_camsv_write(cam_dev, CAMSV_IMGO_CON3, conf->imgo_con3);

	/* CMOS_EN first */
	mtk_camsv_write(cam_dev, CAMSV_TG_SEN_MODE,
			mtk_camsv_read(cam_dev, CAMSV_TG_SEN_MODE) | 0x1U);

	/* then CAMSV_FBC_IMGO_CTL1 : FBC_EN=1 , DMA_RING_EN=1, FBC_MODE=1 */
	mtk_camsv_write(cam_dev, CAMSV_FBC_IMGO_CTL1,
			mtk_camsv_read(cam_dev, CAMSV_FBC_IMGO_CTL1) | 0x00008c00U);

	/* finally, CAMSV_MODULE_EN : IMGO_EN */
	mtk_camsv_write(cam_dev, CAMSV_MODULE_EN,
			mtk_camsv_read(cam_dev, CAMSV_MODULE_EN) | 0x00000010U);

	/* CAMSV_DMA_FH_EN : FRAME_HEADER_EN_IMGO */
	if (conf->enableFH)
		mtk_camsv_write(cam_dev, CAMSV_DMA_FH_EN, 0x1U);

	/*
	 * disable db load mask for non-dcif case; otherwise,
	 * it would cause inner register not latched from outer
	 */
	mtk_camsv_write(cam_dev, CAMSV_DCIF_SET,
			mtk_camsv_read(cam_dev, CAMSV_DCIF_SET) &
			     ~CAMSV_DCIF_SET_MASK_DB_LOAD);

	spin_unlock_irqrestore(&cam_dev->irqlock, flags);
	pm_runtime_put_autosuspend(cam_dev->dev);
}

static void mtk_camsv_reset(struct mtk_cam_dev *cam_dev)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&cam_dev->irqlock, flags);

	/* Disable camsv interrupt*/
	mtk_camsv_write(cam_dev, CAMSV_INT_EN, 0x0U);

	/* Disable IMGO */
	mtk_camsv_write(cam_dev, CAMSV_MODULE_EN,
			mtk_camsv_read(cam_dev, CAMSV_MODULE_EN) &
			     ~CAMSV_MODULE_EN_IMGO_EN);

	spin_unlock_irqrestore(&cam_dev->irqlock, flags);
}

static irqreturn_t isp_irq_camsv(int irq, void *data)
{
	struct mtk_cam_dev *cam_dev = (struct mtk_cam_dev *)data;
	struct mtk_cam_dev_buffer *buf;
	unsigned long flags = 0;
	unsigned int irq_status;

	spin_lock_irqsave(&cam_dev->irqlock, flags);

	irq_status = cam_dev->irq_info.irq_status = mtk_camsv_read(cam_dev, CAMSV_INT_STATUS);
	cam_dev->irq_info.imgo_base_addr = mtk_camsv_read(cam_dev, CAMSV_IMGO_BASE_ADDR);
	cam_dev->irq_info.imgo_ctl1 = mtk_camsv_read(cam_dev, CAMSV_FBC_IMGO_CTL1);
	cam_dev->irq_info.imgo_ctl2 = mtk_camsv_read(cam_dev, CAMSV_FBC_IMGO_CTL2);
	cam_dev->irq_info.imgo_xsize = mtk_camsv_read(cam_dev, CAMSV_IMGO_XSIZE);
	cam_dev->irq_info.imgo_ysize = mtk_camsv_read(cam_dev, CAMSV_IMGO_YSIZE);
	cam_dev->irq_info.imgo_stride = mtk_camsv_read(cam_dev, CAMSV_IMGO_STRIDE);
	cam_dev->irq_info.imgo_error = mtk_camsv_read(cam_dev, CAMSV_IMGO_ERR_STAT);

	/* De-queue frame */
	if (irq_status & CAMSV_IRQ_SW_PASS1_DON) {
		cam_dev->sequence++;

		if (cam_dev->buf_curr) {
			cam_dev->buf_curr->v4l2_buf.sequence = cam_dev->sequence;
			cam_dev->buf_curr->v4l2_buf.vb2_buf.timestamp = ktime_get_ns();
			vb2_buffer_done(&cam_dev->buf_curr->v4l2_buf.vb2_buf,
					VB2_BUF_STATE_DONE);
			cam_dev->buf_curr = NULL;
		}
	}

	if (irq_status & CAMSV_IRQ_PASS1_DON) {
		buf = list_first_entry_or_null(&cam_dev->buf_list,
						struct mtk_cam_dev_buffer,
						list);
		if (!cam_dev->buf_curr && buf) {
			mtk_camsv_update_buffers_add(cam_dev, buf);
			list_del(&buf->list);
			cam_dev->buf_curr = buf;
		}
	}

	spin_unlock_irqrestore(&cam_dev->irqlock, flags);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t isp_irq_camsv_thread(int irq, void *data)
{
	struct mtk_cam_dev *cam_dev = (struct mtk_cam_dev *)data;
	struct mtk_cam_irq_info *irq_info = &cam_dev->irq_info;

	dev_dbg(cam_dev->dev, "seq %u irq_stat 0x%x imgo baddr 0x%x ctl 0x%x/0x%x xysize 0x%x/0x%x stride 0x%x error 0x%x\n",
		cam_dev->sequence,
		irq_info->irq_status,
		irq_info->imgo_base_addr,
		irq_info->imgo_ctl1,
		irq_info->imgo_ctl2,
		irq_info->imgo_xsize,
		irq_info->imgo_ysize,
		irq_info->imgo_stride,
		irq_info->imgo_error);

	return IRQ_HANDLED;
}

static int mtk_camsv_runtime_suspend(struct device *dev)
{
	struct mtk_cam_dev *cam_dev = dev_get_drvdata(dev);

	if (cam_dev->streaming) {
		mtk_camsv_reset(cam_dev);
		mutex_lock(&cam_dev->op_lock);
		v4l2_subdev_call(&cam_dev->subdev, video, s_stream, 0);
		mutex_unlock(&cam_dev->op_lock);
	}
	dev_info(dev, "%s clk bulk disable unprepare\n", __func__);
	clk_bulk_disable_unprepare(cam_dev->num_clks, cam_dev->clks);

	return 0;
}

static int mtk_camsv_runtime_resume(struct device *dev)
{
	struct mtk_cam_dev *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_video_device *vdev = &cam_dev->vdev;
	const struct v4l2_pix_format_mplane *fmt = &vdev->format;
	struct vb2_queue *vbq = &vdev->vbq;
	struct mtk_cam_dev_buffer *buf, *buf_prev;
	int ret;
	unsigned long flags = 0;

	dev_info(dev, "%s clk bulk prepare enable\n", __func__);
	ret = clk_bulk_prepare_enable(cam_dev->num_clks, cam_dev->clks);
	if (ret) {
		dev_err(dev, "failed to enable clock:%d\n", ret);
		return ret;
	}

	if (vb2_is_streaming(vbq)) {
		mtk_camsv_setup(cam_dev, fmt->width, fmt->height,
				fmt->plane_fmt[0].bytesperline, vdev->fmtinfo->code);

		spin_lock_irqsave(&cam_dev->irqlock, flags);
		if (cam_dev->buf_curr) {
			mtk_camsv_update_buffers_add(cam_dev, cam_dev->buf_curr);
		} else {
			buf = list_last_entry(&cam_dev->buf_list,
					      struct mtk_cam_dev_buffer,
					      list);
			if (buf) {
				mtk_camsv_update_buffers_add(cam_dev, buf);
				list_del(&buf->list);
				cam_dev->buf_curr = buf;
			} else {
				dev_err(cam_dev->dev, "No buffer to add\n");
			}
		}

		spin_unlock_irqrestore(&cam_dev->irqlock, flags);

		mtk_camsv_cmos_vf_hw_enable(cam_dev, vdev->fmtinfo->packed);

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
	spin_lock_irqsave(&cam_dev->irqlock, flags);
	list_for_each_entry_safe(buf, buf_prev, &cam_dev->buf_list, list) {
		buf->daddr = 0ULL;
		list_del(&buf->list);
		vb2_buffer_done(&buf->v4l2_buf.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&cam_dev->irqlock, flags);
	return ret;
}

static struct mtk_cam_hw_functions mtk_camsv_hw_functions = {
	.mtk_cam_setup = mtk_camsv_setup,
	.mtk_cam_reset = mtk_camsv_reset,
	.mtk_cam_update_buffers_add = mtk_camsv_update_buffers_add,
	.mtk_cam_cmos_vf_hw_enable = mtk_camsv_cmos_vf_hw_enable,
	.mtk_cam_cmos_vf_hw_disable = mtk_camsv_cmos_vf_hw_disable,
};

static int mtk_camsv_probe(struct platform_device *pdev)
{
	static const char * const clk_names[] = {
		"CAMSYS_CAM_CGPDN",
		"CAMSYS_CAMTG_CGPDN",
		"CAMSYS_CAMSV_CGPDN",
		"CAMSYS_LARB_CGPDN",
		"CAMSYS_MAIN_CAM2MM_GALS_CGPDN",
		"TOPCKGEN_TOP_MUX_CAMTM"
	};

	struct mtk_cam_dev *cam_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	int i;

	dev_info(dev, "camsv | start %s\n", __func__);
	if (!iommu_present(&platform_bus_type))
		return -EPROBE_DEFER;

	cam_dev = devm_kzalloc(dev, sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;

	cam_dev->conf = (struct mtk_cam_conf *)of_device_get_match_data(dev);
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

	cam_dev->num_clks = ARRAY_SIZE(clk_names);
	cam_dev->clks = devm_kcalloc(dev, cam_dev->num_clks,
				     sizeof(*cam_dev->clks), GFP_KERNEL);
	if (!cam_dev->clks)
		return -ENOMEM;

	for (i = 0; i < cam_dev->num_clks; ++i)
		cam_dev->clks[i].id = clk_names[i];

	ret = devm_clk_bulk_get_optional(dev, cam_dev->num_clks, cam_dev->clks);
	if (ret) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	cam_dev->irq = platform_get_irq(pdev, 0);
	ret = devm_request_threaded_irq(dev, cam_dev->irq, isp_irq_camsv, isp_irq_camsv_thread,
					IRQF_SHARED | IRQF_ONESHOT,
					dev_name(dev), cam_dev);
	if (ret) {
		dev_err(dev, "failed to request irq=%d ret=%d\n", cam_dev->irq, ret);
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,cammux-id",
				   &cam_dev->cammux_id);
	if (ret) {
		dev_dbg(dev, "missing cammux id property\n");
		return ret;
	}

	cam_dev->hw_functions = &mtk_camsv_hw_functions;

	/* initialise protection mutex */
	spin_lock_init(&cam_dev->irqlock);

	/* initialise runtime power management */
	pm_runtime_set_autosuspend_delay(dev, MTK_CAMSV_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);

	/* Initialize the v4l2 common part */
	return mtk_cam_dev_init(cam_dev);
}

static int mtk_camsv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_dev *cam_dev = dev_get_drvdata(dev);

	mtk_cam_dev_cleanup(cam_dev);
	pm_runtime_put_autosuspend(dev);
	pm_runtime_disable(dev);

	return 0;
}

static const struct dev_pm_ops mtk_camsv_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(mtk_camsv_runtime_suspend,
			   mtk_camsv_runtime_resume, NULL)
};

static const struct of_device_id mtk_camsv_of_ids[] = {
	{
		.compatible = "mediatek,mt8189-camsv",
		.data = &camsv_conf,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_camsv_of_ids);

static struct platform_driver mtk_camsv_driver = {
	.probe = mtk_camsv_probe,
	.remove = mtk_camsv_remove,
	.driver = {
		.name = "mtk-camsv",
		.of_match_table = of_match_ptr(mtk_camsv_of_ids),
		.pm = &mtk_camsv_pm_ops,
	}
};

module_platform_driver(mtk_camsv_driver);

MODULE_DESCRIPTION("MediaTek CAMSV driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: mtk-seninf6s");
