// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
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
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/nvmem-consumer.h>
#include <linux/regulator/consumer.h>

#include "mtk_hdmi_rpt.h"

#if IS_ENABLED(CONFIG_MTK_HDMI_RPT)

#include <linux/hdmi.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include "mtk_hdmi_rpt.h"
#include "mtk_hdmi.h"
#include "mtk_hdmi_phy.h"
#include "mtk_hdmi_regs.h"
#include "mtk_hdmi_hdcp.h"

/* AVI Info Frame */
#define AVI_TYPE            0x82
#define AVI_VERS            0x02
#define AVI_LEN             0x0d

/* Audio Info Frame */
#define AUDIO_TYPE             0x84
#define AUDIO_VERS             0x01
#define AUDIO_LEN              0x0A

#define VS_TYPE            0x81
#define VS_VERS            0x01
#define VS_LEN             0x05
/* VS_LEN+1 include checksum */
#define VS_PB_LEN          0x0b

#define VS_DV_LEN  0x18

/* GAMUT Data */
#define GAMUT_TYPE             0x0A
#define GAMUT_PROFILE          0x81
#define GAMUT_SEQ              0x31

/* ACP Info Frame */
#define ACP_TYPE            0x04
/* #define ACP_VERS            0x02 */
#define ACP_LEN             0x00

/* ISRC1 Info Frame */
#define ISRC1_TYPE            0x05
/* #define ACP_VERS            0x02 */
#define ISRC1_LEN             0x00

/* SPD Info Frame */
#define SPD_TYPE            0x83
#define SPD_VERS            0x01
#define SPD_LEN             0x19

/* HDR */
#define HDR_TYPE            0x87
#define HDR_VERS            0x01
#define HDR_LEN             0x1A

#define HDR_PACKET_DISABLE    0x00
#define HDR_PACKET_ACTIVE        0x01
#define HDR_PACKET_ZERO        0x02

unsigned char mtk_hdmi_rpt_log = 1;

#define HDMI_RPT_LOG(fmt, arg...) \
	do {	if (mtk_hdmi_rpt_log) { \
		pr_info("[HDMI_RPT] %s,%d "fmt, __func__, __LINE__, ##arg); \
		} \
	} while (0)

#define HDMI_RPT_FUNC()	\
	do {	if (mtk_hdmi_rpt_log) \
		pr_info("[HDMI_RPT] %s\n", __func__); \
	} while (0)

#define BYTE unsigned char
#define BOOL bool

enum hdmi_mode_setting hdmi_mode;

struct HdmiInfo *tx_AviInfo;
struct HdmiInfo *tx_SpdInfo;
struct HdmiInfo *tx_AudInfo;
struct HdmiInfo *tx_AcpInfo;
struct HdmiInfo *tx_VsiInfo;
struct HdmiInfo *tx_Isrc0Info;
struct HdmiInfo *tx_Isrc1Info;
struct HdmiInfo *tx_Info87;

struct HdmiInfo d_AviInfo;
struct HdmiInfo d_SpdInfo;
struct HdmiInfo d_AudInfo;
struct HdmiInfo d_AcpInfo;
struct HdmiInfo d_VsiInfo;
struct HdmiInfo d_Isrc0Info;
struct HdmiInfo d_Isrc1Info;
struct HdmiInfo d_Info87;

void vHalSendStaticHdrInfoFrameRx(BYTE bEnable, BYTE *pr_bData)
{
	BYTE bHDR_CHSUM = 0;
	BYTE i;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN4_EN | GEN4_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN4_RPT_EN);

	if (bEnable == HDR_PACKET_ACTIVE) {
		mtk_hdmi_write(hdmi, TOP_GEN4_HEADER,
			(HDR_LEN << 16) + (HDR_VERS << 8) + (HDR_TYPE << 0));
		bHDR_CHSUM = HDR_LEN + HDR_VERS + HDR_TYPE;
		for (i = 0; i < HDR_LEN; i++)
			bHDR_CHSUM += (*(pr_bData+i));
		bHDR_CHSUM = 0x100 - bHDR_CHSUM;
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT00,
			((*(pr_bData+2)) << 24) + (*(pr_bData+1) << 16) +
			((*(pr_bData+0)) << 8) + (bHDR_CHSUM << 0));
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT01,
			(((*(pr_bData+5)) << 16) + ((*(pr_bData+4)) << 8) +
			((*(pr_bData+3)) << 0)));
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT02,
			((*(pr_bData+9)) << 24) + ((*(pr_bData+8)) << 16) +
			((*(pr_bData+7)) << 8) + ((*(pr_bData+6)) << 0));
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT03,
			((*(pr_bData+12)) << 16) + ((*(pr_bData+11)) << 8) +
			((*(pr_bData+10)) << 0));
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT04,
			((*(pr_bData+16)) << 24) + (*(pr_bData+15) << 16) +
			((*(pr_bData+14)) << 8) + ((*(pr_bData+13)) << 0));
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT05,
			(*(pr_bData+19) << 16) + ((*(pr_bData+18)) << 8) +
			((*(pr_bData+17)) << 0));
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT06,
			((*(pr_bData+23)) << 24) + (*(pr_bData+22) << 16) +
			((*(pr_bData+21)) << 8) + ((*(pr_bData+20)) << 0));
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT07,
			((*(pr_bData+25)) << 8) + ((*(pr_bData+24)) << 0));
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, GEN4_RPT_EN, GEN4_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN,
			GEN4_EN | GEN4_EN_WR, GEN4_EN | GEN4_EN_WR);

	} else if (bEnable == HDR_PACKET_ZERO) {
		mtk_hdmi_write(hdmi, TOP_GEN4_HEADER,
			(HDR_LEN << 16) + (HDR_VERS << 8) + (HDR_TYPE << 0));
		bHDR_CHSUM = HDR_LEN + HDR_VERS + HDR_TYPE;
		bHDR_CHSUM = 0x100 - bHDR_CHSUM;
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT00, bHDR_CHSUM);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT01, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT02, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT03, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT04, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT05, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT06, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT07, 0);

		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, GEN4_RPT_EN, GEN4_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN,
			GEN4_EN | GEN4_EN_WR, GEN4_EN | GEN4_EN_WR);
	}
}

void vHalSendGamutPacket(BYTE *pr_bData, BYTE bFmtFlag)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, HDMI2_GAMUT_EN | GAMUT_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GAMUT_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_GAMUT_HEADER,
		(GAMUT_SEQ << 16) + (GAMUT_PROFILE << 8) + (GAMUT_TYPE << 0));

	if (bFmtFlag == 0) {
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT00,
			(*(pr_bData+3) << 24) + (*(pr_bData+2) << 16) +
			(*(pr_bData+1) << 8) + (*(pr_bData+0) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT01,
			(*(pr_bData+6) << 16) + (*(pr_bData+5) << 8) +
			(*(pr_bData+4) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT02,
			(*(pr_bData+10) << 24) + (*(pr_bData+9) << 16) +
			(*(pr_bData+8) << 8) + (*(pr_bData+7) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT03,
			(*(pr_bData+13) << 16) +
			(*(pr_bData+12) << 8) + (*(pr_bData+11) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT04, (*(pr_bData+14) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT05, 0);
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT06, 0);
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT07, 0);
	} else {
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT00,
			(*(pr_bData+3) << 24) + (*(pr_bData+2) << 16) +
			(*(pr_bData+1) << 8) + (*(pr_bData+0) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT01,
			(*(pr_bData+6) << 16) +
			(*(pr_bData+5) << 8) + (*(pr_bData+4) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT02,
			(*(pr_bData+9) << 16) +
			(*(pr_bData+8) << 8) + (*(pr_bData+7) << 0));
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT03, 0);
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT04, 0);
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT05, 0);
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT06, 0);
		mtk_hdmi_write(hdmi, TOP_GAMUT_PKT07, 0);
	}

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, GAMUT_RPT_EN, GAMUT_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN,
		HDMI2_GAMUT_EN | GAMUT_EN_WR, HDMI2_GAMUT_EN | GAMUT_EN_WR);

}

void vHalSendBypassACPInfoFrame(BYTE *pr_bData)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN_EN | GEN_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_GEN_HEADER,
		((*(pr_bData+2)) << 16) + ((*(pr_bData+1)) << 8) +
		(*(pr_bData)));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT00,
		((*(pr_bData+6)) << 24) + (*(pr_bData+5) << 16) +
		((*(pr_bData+4)) << 8) + ((*(pr_bData)+3) << 0));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT01,
		((*(pr_bData+9)) << 16) + ((*(pr_bData+8)) << 8) +
		((*(pr_bData+7)) << 0));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT02,
		((*(pr_bData+13)) << 24) + ((*(pr_bData+12)) << 16) +
		((*(pr_bData+11)) << 8) + ((*(pr_bData+10)) << 0));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT03,
		((*(pr_bData+16)) << 16) + ((*(pr_bData+15)) << 8) +
		((*(pr_bData+14)) << 0));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT04,
		((*(pr_bData+20)) << 24) + ((*(pr_bData+19)) << 16) +
		((*(pr_bData+18)) << 8) + ((*(pr_bData+17)) << 0));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT05,
		((*(pr_bData+23)) << 16) + ((*(pr_bData+22)) << 8) +
		((*(pr_bData+21)) << 0));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT06,
		((*(pr_bData+27)) << 24) + ((*(pr_bData+26)) << 16) +
		((*(pr_bData+25)) << 8) + ((*(pr_bData+24)) << 0));
	mtk_hdmi_write(hdmi, TOP_GEN_PKT07,
		((*(pr_bData+30)) << 16) + ((*(pr_bData+29)) << 8) +
		((*(pr_bData+28)) << 0));

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, GEN_RPT_EN, GEN_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, GEN_EN | GEN_EN_WR, GEN_EN | GEN_EN_WR);
}

void vHalSendBypassVendorSpecificInfoFrame(BYTE *pr_bData)
{
	BYTE bLength = 28;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	bLength = (*(pr_bData+2))+1;

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, VSIF_EN | VSIF_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, VSIF_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_VSIF_HEADER,
		((*(pr_bData+2)) << 16) +
		((*(pr_bData+1)) << 8) + (*(pr_bData)));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT00,
		((*(pr_bData+6)) << 24) + ((*(pr_bData+5)) << 16) +
		((*(pr_bData+4)) << 8) + (*(pr_bData+3)));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT01,
		((*(pr_bData+9)) << 16) +
		((*(pr_bData+8)) << 8) + (*(pr_bData+7)));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT02,
		((*(pr_bData+13)) << 24) + ((*(pr_bData+12)) << 16) +
		((*(pr_bData+11)) << 8) + (*(pr_bData+10)));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT03,
		((*(pr_bData+16)) << 16) +
		((*(pr_bData+15)) << 8) + (*(pr_bData+14)));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT04,
		((*(pr_bData+20)) << 24) + ((*(pr_bData+19)) << 16) +
		((*(pr_bData+18)) << 8) + (*(pr_bData+17)));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT05,
		((*(pr_bData+23)) << 16) +
		((*(pr_bData+22)) << 8) + (*(pr_bData+21)));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT06,
		((*(pr_bData+27)) << 24) + ((*(pr_bData+26)) << 16) +
		((*(pr_bData+25)) << 8) + ((*(pr_bData+24)) << 0));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT07,
		((*(pr_bData+30)) << 16) +
		((*(pr_bData+29)) << 8) + ((*(pr_bData+28)) << 0));

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, VSIF_RPT_EN, VSIF_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN,
		VSIF_EN | VSIF_EN_WR, VSIF_EN | VSIF_EN_WR);

}

void vHalSendSPDInfoFrame(BYTE *pr_bData)
{
	BYTE bSPD_CHSUM = 0;
	BYTE i = 0;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}


	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, SPD_EN | SPD_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, SPD_RPT_EN);

	bSPD_CHSUM = SPD_TYPE + SPD_VERS + SPD_LEN;

	if (1) {
		mtk_hdmi_write(hdmi, TOP_SPDIF_HEADER,
			((*(pr_bData+2)) << 16) + ((*(pr_bData+1)) << 8) +
			((*(pr_bData+0)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT00,
			((*(pr_bData+6)) << 24)+((*(pr_bData+5)) << 16) +
			((*(pr_bData+4)) << 8) + ((*(pr_bData+3)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT01,
			((*(pr_bData+9)) << 16) + ((*(pr_bData+8)) << 8) +
			((*(pr_bData+7)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT02,
			((*(pr_bData+13)) << 24)+((*(pr_bData+12)) << 16) +
			((*(pr_bData+11)) << 8) + ((*(pr_bData+10)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT03,
			((*(pr_bData+16)) << 16) + ((*(pr_bData+15)) << 8) +
			((*(pr_bData+14)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT04,
			((*(pr_bData+20)) << 24)+((*(pr_bData+19)) << 16) +
			((*(pr_bData+18)) << 8) + ((*(pr_bData+17)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT05,
			((*(pr_bData+23)) << 16) + ((*(pr_bData+22)) << 8) +
			((*(pr_bData+21)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT06,
			((*(pr_bData+27)) << 24)+((*(pr_bData+26)) << 16) +
			((*(pr_bData+25)) << 8) + ((*(pr_bData+24)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT07,
			((*(pr_bData+30)) << 16) + ((*(pr_bData+29)) << 8) +
			((*(pr_bData+28)) << 0));

		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, SPD_RPT_EN, SPD_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN,
			SPD_EN | SPD_EN_WR, SPD_EN | SPD_EN_WR);


	} else {
		for (i = 0; i < SPD_LEN; i++)
			bSPD_CHSUM += (*(pr_bData+i));

		bSPD_CHSUM = 0x100 - bSPD_CHSUM;

		mtk_hdmi_write(hdmi, TOP_SPDIF_HEADER, (SPD_LEN << 16) +
			(SPD_VERS << 8) + (SPD_TYPE << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT00,
			((*(pr_bData+2)) << 24) + ((*(pr_bData+1)) << 16) +
			((*(pr_bData)) << 8)+(bSPD_CHSUM << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT01,
			((*(pr_bData+5)) << 16) + ((*(pr_bData+4)) << 8) +
			((*(pr_bData+3)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT02,
			((*(pr_bData+9)) << 24)+((*(pr_bData+8)) << 16) +
			((*(pr_bData+7)) << 8) + ((*(pr_bData+6)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT03,
			((*(pr_bData+12)) << 16) + ((*(pr_bData+11)) << 8) +
			((*(pr_bData+10)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT04,
			((*(pr_bData+16)) << 24)+((*(pr_bData+15)) << 16) +
			((*(pr_bData+14)) << 8) + ((*(pr_bData+13)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT05,
			((*(pr_bData+19)) << 16) + ((*(pr_bData+18)) << 8) +
			((*(pr_bData+17)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT06,
			((*(pr_bData+23)) << 24)+((*(pr_bData+22)) << 16) +
			((*(pr_bData+21)) << 8) + ((*(pr_bData+20)) << 0));
		mtk_hdmi_write(hdmi, TOP_SPDIF_PKT07,
			(*(pr_bData+24) << 0));

		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, SPD_RPT_EN, SPD_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN,
			SPD_EN | SPD_EN_WR, SPD_EN | SPD_EN_WR);

	}
}

void vHalSendBypassAudioInfoFrame(BYTE *pr_bData)
{
	unsigned int bData1 = 0, bData2 = 0, bData3 = 0, bData4 = 0, bData5 = 0;
	unsigned char i;
	unsigned char bLength = 11;
	unsigned char bAUDIO_CHSUM = 0;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, AUD_EN | AUD_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, AUD_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_AIF_HEADER, ((*(pr_bData+2)) << 16) +
		((*(pr_bData+1)) << 8) + (*(pr_bData)));
	for (i = 0; i < bLength; i++) {
		if (i != 3)
			bAUDIO_CHSUM += *(pr_bData+i);
	}
	bAUDIO_CHSUM = 0x100-bAUDIO_CHSUM;

	bData1 = *(pr_bData+4);
	bData2 = *(pr_bData+5);
	bData3 = *(pr_bData+6);
	bData4 = *(pr_bData+7);
	bData5 = *(pr_bData+8);

	mtk_hdmi_write(hdmi, TOP_AIF_PKT00,
		(bData3 << 24) + (bData2 << 16) + (bData1 << 8) + bAUDIO_CHSUM);
	mtk_hdmi_write(hdmi, TOP_AIF_PKT01, (bData5 << 8) + bData4);
	mtk_hdmi_write(hdmi, TOP_AIF_PKT02, 0x00000000);
	mtk_hdmi_write(hdmi, TOP_AIF_PKT03, 0x00000000);

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AUD_RPT_EN, AUD_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AUD_EN | AUD_EN_WR, AUD_EN | AUD_EN_WR);

}

void vHalSendBypassAVIInfoFrame(unsigned char *pr_bData)
{
	unsigned char bAVI_CHSUM = 0;
	unsigned char i;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, AVI_EN_WR | AVI_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, AVI_RPT_EN);

	for (i = 0; i < AVI_LEN + 1; i++)
		HDMI_RPT_LOG(
		"RxTx packet bypass: (Pbdata+%d) = 0x%x\n", i, *(pr_bData+i));

	bAVI_CHSUM = *(pr_bData+0);
	bAVI_CHSUM += *(pr_bData+1);
	bAVI_CHSUM += *(pr_bData+2);
	bAVI_CHSUM += *(pr_bData+4);
	bAVI_CHSUM += *(pr_bData+5);
	bAVI_CHSUM += *(pr_bData+6);
	bAVI_CHSUM += *(pr_bData+7);
	bAVI_CHSUM += *(pr_bData+8);
	bAVI_CHSUM = 0x100 - bAVI_CHSUM;

	mtk_hdmi_write(hdmi, TOP_AVI_HEADER,
		(*(pr_bData+2) << 16) +
		(*(pr_bData+1) << 8) + (*(pr_bData+0) << 0));
	mtk_hdmi_write(hdmi, TOP_AVI_PKT00,
	(*(pr_bData+6) << 24) + (*(pr_bData+5) << 16) +
	(*(pr_bData+4) << 8) + (bAVI_CHSUM << 0));
	mtk_hdmi_write(hdmi, TOP_AVI_PKT01,
		(*(pr_bData+8) << 8) + (*(pr_bData+7) << 0));
	mtk_hdmi_write(hdmi, TOP_AVI_PKT02, 0);
	mtk_hdmi_write(hdmi, TOP_AVI_PKT03, 0);
	mtk_hdmi_write(hdmi, TOP_AVI_PKT04, 0);

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AVI_RPT_EN, AVI_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AVI_EN_WR | AVI_EN, AVI_EN_WR | AVI_EN);
}

void vHalSendHdmiPacket(BYTE bType, BOOL fgEnable, BYTE *pr_bData)
{
	BYTE bLength = 28;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	HDMI_RPT_LOG(
"RxTx packet bypass: bType = %d, fgEnable = %d, HB0 = 0x%x, HB1 = 0x%x, HB2 = 0x%x, PB0 = 0x%x\n",
	bType, fgEnable, *(pr_bData), *(pr_bData+1),
	*(pr_bData+2), *(pr_bData+3));
	if (bType == AVI_INFOFRAME) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, AVI_EN | AVI_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, AVI_RPT_EN);
		bLength = 14;
	} else if (bType == AUDIO_INFOFRAME) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, AUD_EN | AUD_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, AUD_RPT_EN);
		bLength = 11;
	} else if (bType == MPEG_INFOFRAME) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, MPEG_EN | MPEG_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, MPEG_RPT_EN);
		bLength = 11;
	} else if (bType == VENDOR_INFOFRAME) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, VSIF_EN | VSIF_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, VSIF_RPT_EN);
		bLength = (*(pr_bData+2))+1;
	} else if (bType == SPD_INFOFRAME) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, SPD_EN | SPD_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, SPD_RPT_EN);
		bLength = 26;
	} else if (bType == ACP_PACKET) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN_EN | GEN_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN_RPT_EN);
	} else if (bType == ISRC1_PACKET) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN2_EN | GEN2_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN2_RPT_EN);
	} else if (bType == ISRC2_PACKET) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN3_EN | GEN3_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN3_RPT_EN);
	} else if (bType == GAMUT_PACKET) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN,
			0, HDMI2_GAMUT_EN | GAMUT_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GAMUT_RPT_EN);
	} else if (bType == HDR87_PACKET) {
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN4_EN | GEN4_EN_WR);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN4_RPT_EN);
	}
	if (!fgEnable)
		return;

	if (fgEnable) {
		if (bType == AVI_INFOFRAME)
			vHalSendBypassAVIInfoFrame(pr_bData);
		else if (bType == AUDIO_INFOFRAME)
			vHalSendBypassAudioInfoFrame(pr_bData);
		else if (bType == MPEG_INFOFRAME) {
			mtk_hdmi_mask(hdmi, TOP_INFO_RPT,
				MPEG_RPT_EN, MPEG_RPT_EN);
			mtk_hdmi_mask(hdmi, TOP_INFO_EN,
				MPEG_EN_WR | MPEG_EN, MPEG_EN_WR | MPEG_EN);
		} else if (bType == SPD_INFOFRAME)
			vHalSendSPDInfoFrame(pr_bData);
		else if (bType == VENDOR_INFOFRAME)
			vHalSendBypassVendorSpecificInfoFrame(pr_bData);
		else if (bType == ACP_PACKET)
			vHalSendBypassACPInfoFrame(pr_bData);
		else if (bType == ISRC1_PACKET) {
			mtk_hdmi_mask(hdmi, TOP_INFO_RPT,
				GEN2_RPT_EN, GEN2_RPT_EN);
			mtk_hdmi_mask(hdmi, TOP_INFO_EN,
				GEN2_EN_WR | GEN2_EN, GEN2_EN_WR | GEN2_EN);
		} else if (bType == ISRC2_PACKET) {
			mtk_hdmi_mask(hdmi, TOP_INFO_RPT,
				GEN3_RPT_EN, GEN3_RPT_EN);
			mtk_hdmi_mask(hdmi, TOP_INFO_EN,
				GEN3_EN_WR | GEN3_EN, GEN3_EN_WR | GEN3_EN);
		} else if (bType == GAMUT_PACKET)
			vHalSendGamutPacket((pr_bData+4), 1);
		else if (bType == HDR87_PACKET)
			vHalSendStaticHdrInfoFrameRx(HDR_PACKET_ACTIVE,
			(pr_bData+4));
	}
}

void vSendHdmiPacket(BYTE bType, BOOL fgEnable, BYTE *pr_bData)
{
	vHalSendHdmiPacket(bType, fgEnable, pr_bData);
}

void vSetRptMode2(struct device *dev,
	enum hdmi_mode_setting mode)
{
	HDMI_RPT_FUNC();

	hdmi_mode = mode;
	HDMI_RPT_LOG("RPT mode = %d\n", mode);

	if ((hdmi_mode == HDMI_RPT_PIXEL_MODE) ||
		(hdmi_mode == HDMI_RPT_TMDS_MODE) ||
		(hdmi_mode == HDMI_RPT_DGI_MODE) ||
		(hdmi_mode == HDMI_RPT_DRAM_MODE)) {
		if ((vdout_hd_sel == NULL) ||
			(vdout_sd_sel == NULL) ||
			(hdmitx_pxl_clk == NULL) ||
			(hdmitx_pxl_clk_d6 == NULL)) {
			return;
		}
		ret = clk_set_parent(vdout_hd_sel, hdmitx_pxl_clk);
		if (ret)
			HDMI_RPT_LOG("[RX]Failed to set vdout_hd_sel\n");
		ret = clk_set_parent(vdout_sd_sel, hdmitx_pxl_clk_d6);
		if (ret)
			HDMI_RPT_LOG("[RX]Failed to set vdout_sd_sel\n");
	}
}

void hdmirx_get_edid(unsigned char *edid, unsigned int num, unsigned int len)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();
	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}
	if (num >= 4) {
		HDMI_RPT_LOG("rx edid requestion invalid\n");
		return;
	}
	if (len != 128) {
		HDMI_RPT_LOG("rx edid len invalid\n");
		return;
	}

	memcpy(edid, &(hdmi->raw_edid.edid[num * len]), 128);
}

bool bGetEdid(struct device *dev,
		u8 *p,
		u32 num,
		u32 len)
{
	HDMI_RPT_FUNC();

	hdmirx_get_edid(p, num, len);
	return 0;
}

void vHdcpStart(struct device *dev,
	bool hdcp_on)
{
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_LOG("%s(), hdcp_on=%d\n", __func__, hdcp_on);

	hdmi->hpd = true;
	HDMI_RPT_LOG("force HPD high\n");

	if (hdcp_on) {
		hdmi->repeater_hdcp = true;
		vHDCPInitAuth();
	} else {
		mtk_hdmi_AV_mute(hdmi);
		//vSend_AVUNMUTE();
		vHDCPReset();
		vSetHDCPState(HDCP_RECEIVER_NOT_READY);
		hdmi->repeater_hdcp = false;
	}
#else
	HDMI_RPT_LOG("Tx cannot support HDCP\n");
#endif
}

u8 cHdcpVer(struct device *dev)
{
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();

	if (hdmi->hdcp_2x_support == false) {
		HDMI_RPT_LOG("support hdcp 1.x\n");
		return 0;
	}
	HDMI_RPT_LOG("support hdcp 2.x\n");
	return 1;
#else
	HDMI_RPT_LOG("Tx cannot support HDCP\n");
	return 0;
#endif
}

void vHdmiEnable(struct device *dev,
	bool en)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();
	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	vTxSignalOnOff(hdmi->hdmi_phy_base, en);
}

bool bIsPlugIn(struct device *dev)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();
	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return false;
	}

	if (hdmi->hpd == true)
		return true;
	return false;
}

void vSignalOff(struct device *dev)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();
	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return;
	}

	vTxSignalOnOff(hdmi->hdmi_phy_base, false);
}

void vTxSendPkt(struct device *dev,
	enum packet_type type,
	bool enable,
	u8 *pkt_data,
	struct HdmiInfo *pkt_p)
{
	HDMI_RPT_FUNC();

	if (type == MAX_PACKET) {
		memset(&d_AviInfo, 0, sizeof(struct HdmiInfo));
		memset(&d_SpdInfo, 0, sizeof(struct HdmiInfo));
		memset(&d_AudInfo, 0, sizeof(struct HdmiInfo));
		memset(&d_AcpInfo, 0, sizeof(struct HdmiInfo));
		memset(&d_VsiInfo, 0, sizeof(struct HdmiInfo));
		memset(&d_Isrc0Info, 0, sizeof(struct HdmiInfo));
		memset(&d_Isrc1Info, 0, sizeof(struct HdmiInfo));
		memset(&d_Info87, 0, sizeof(struct HdmiInfo));
		tx_AviInfo = NULL;
		tx_SpdInfo = NULL;
		tx_AudInfo = NULL;
		tx_AcpInfo = NULL;
		tx_VsiInfo = NULL;
		tx_Isrc0Info = NULL;
		tx_Isrc1Info = NULL;
		tx_Info87 = NULL;
		return;
	}

	if (type == AVI_INFOFRAME) {
		tx_AviInfo = &d_AviInfo;
		memcpy(tx_AviInfo, pkt_p, sizeof(struct HdmiInfo));
	} else if (type == AUDIO_INFOFRAME) {
		tx_AudInfo = &d_AudInfo;
		memcpy(tx_AudInfo, pkt_p, sizeof(struct HdmiInfo));
	} else if (type == ACP_PACKET) {
		tx_AcpInfo = &d_AcpInfo;
		memcpy(tx_AcpInfo, pkt_p, sizeof(struct HdmiInfo));
	} else if (type == ISRC1_PACKET) {
		tx_Isrc0Info = &d_Isrc0Info;
		memcpy(tx_Isrc0Info, pkt_p, sizeof(struct HdmiInfo));
	} else if (type == ISRC2_PACKET) {
		tx_Isrc1Info = &d_Isrc1Info;
		memcpy(tx_Isrc1Info, pkt_p, sizeof(struct HdmiInfo));
	} else if (type == VENDOR_INFOFRAME) {
		tx_VsiInfo = &d_VsiInfo;
		memcpy(tx_VsiInfo, pkt_p, sizeof(struct HdmiInfo));
	} else if (type == SPD_INFOFRAME) {
		tx_SpdInfo = &d_SpdInfo;
		memcpy(tx_SpdInfo, pkt_p, sizeof(struct HdmiInfo));
	} else if (type == HDR87_PACKET) {
		tx_Info87 = &d_Info87;
		memcpy(tx_Info87, pkt_p, sizeof(struct HdmiInfo));
	}

	vSendHdmiPacket(type, enable, pkt_data);
}

void vHdmiRxSetTxPacket(void)
{
	HDMI_RPT_FUNC();

	if (tx_AviInfo != NULL)
		vSendHdmiPacket(AVI_INFOFRAME,
			tx_AviInfo->fgValid,
			&tx_AviInfo->u1InfoData[0]);

	if (tx_SpdInfo != NULL)
		vSendHdmiPacket(SPD_INFOFRAME,
			tx_SpdInfo->fgValid,
			&tx_SpdInfo->u1InfoData[0]);

	if (tx_AudInfo != NULL)
		vSendHdmiPacket(AUDIO_INFOFRAME,
			tx_AudInfo->fgValid,
			&tx_AudInfo->u1InfoData[0]);

	if (tx_AcpInfo != NULL)
		vSendHdmiPacket(ACP_PACKET,
			tx_AcpInfo->fgValid,
			&tx_AcpInfo->u1InfoData[0]);

	if (tx_VsiInfo != NULL)
		vSendHdmiPacket(VENDOR_INFOFRAME,
			tx_VsiInfo->fgValid,
			&tx_VsiInfo->u1InfoData[0]);

	if (tx_Isrc0Info != NULL)
		vSendHdmiPacket(ISRC1_PACKET,
			tx_Isrc0Info->fgValid,
			&tx_Isrc0Info->u1InfoData[0]);

	if (tx_Isrc1Info != NULL)
		vSendHdmiPacket(ISRC2_PACKET,
			tx_Isrc1Info->fgValid,
			&tx_Isrc1Info->u1InfoData[0]);
}


struct drm_display_mode *convert_id_to_drm_mode(unsigned int id)
{
	struct drm_display_mode *mode;

	HDMI_RPT_LOG("RX_ID = %d\n", id);

	if (id == MODE_HDMI_640_480P)
		mode = &mode_640x480_60hz_4v3;
	else if (id == MODE_480P)
		mode = &mode_720x480_60hz_4v3;
	else if (id == MODE_576P)
		mode = &mode_720x576_50hz_16v9;
	else if (id == MODE_720p_50)
		mode = &mode_1280x720_50hz_16v9;
	else if (id == MODE_720p_60)
		mode = &mode_1280x720_60hz_16v9;
	else if (id == MODE_1080p_50)
		mode = &mode_1920x1080_50hz_16v9;
	else if (id == MODE_1080p_60)
		mode = &mode_1920x1080_60hz_16v9;
	else if (id == MODE_1080p_24)
		mode = &mode_1920x1080_24hz_16v9;
	else if (id == MODE_1080p_25)
		mode = &mode_1920x1080_25hz_16v9;
	else if (id == MODE_1080p_30)
		mode = &mode_1920x1080_30hz_16v9;
	else if (id == MODE_3840_2160P_24)
		mode = &mode_3840x2160_24hz_16v9;
	else if	(id == MODE_3840_2160P_25)
		mode = &mode_3840x2160_25hz_16v9;
	else if	(id == MODE_3840_2160P_30)
		mode = &mode_3840x2160_30hz_16v9;
	else if ((id == MODE_3840_2160P_50) || (id == MODE_3840_2160P_50_420))
		mode = &mode_3840x2160_50hz_16v9;
	else if ((id == MODE_3840_2160P_60) || (id == MODE_3840_2160P_60_420))
		mode = &mode_3840x2160_60hz_16v9;
	else if (id == MODE_4096_2160P_24)
		mode = &mode_4096x2160_24hz;
	else if	(id == MODE_4096_2160P_25)
		mode = &mode_4096x2160_25hz;
	else if	(id == MODE_4096_2160P_30)
		mode = &mode_4096x2160_30hz;
	else if ((id == MODE_4096_2160P_50) || (id == MODE_4096_2160P_50_420))
		mode = &mode_4096x2160_50hz;
	else if ((id == MODE_4096_2160P_60) || (id == MODE_4096_2160P_60_420))
		mode = &mode_4096x2160_60hz;
	else {
		HDMI_RPT_LOG("INVALID ID\n");
		HDMI_RPT_LOG("default return 1080p@60HZ\n");
		mode = &mode_1920x1080_60hz_16v9;
	}

	return mode;
}

bool rpt_pixel_cfg(struct device *dev,
	struct MTK_VIDEO_PARA *vid_para)
{
	unsigned int bRxTmdsRate;
	struct drm_display_mode *mode;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();
	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return 0;
	}

	if (vid_para == NULL) {
		HDMI_RPT_LOG("[RX]para null\n");
		return 0;
	}

	HDMI_RPT_LOG("[RX]pxl,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,,%s=%d\n",
		"cs", vid_para->cs,
		"dp", vid_para->dp,
		"hdmi_mode", vid_para->hdmi_mode,
		"pixclk", vid_para->pixclk,
		"tmdsclk", vid_para->tmdsclk,
		"is_40x", vid_para->is_40x,
		"id", vid_para->id);

	if (hdmi->enabled == true)
		drm_bridge_disable(&hdmi->bridge);

	if (hdmi->powered == true)
		drm_bridge_post_disable(&hdmi->bridge);

	mode = convert_id_to_drm_mode(vid_para->id);
	drm_bridge_mode_set(&hdmi->bridge, NULL, mode);

	if (vid_para->cs == CS_YUV420) {
		if (vid_para->dp == 0x03)
			hdmi->set_csp_depth = YCBCR420_16bit;
		else if (vid_para->dp == 0x02)
			hdmi->set_csp_depth = YCBCR420_12bit;
		else if (vid_para->dp == 0x01)
			hdmi->set_csp_depth = YCBCR420_10bit;
		else
			hdmi->set_csp_depth = YCBCR420_8bit;
	} else if (vid_para->cs == CS_YUV444) {
		if (vid_para->dp == 0x03)
			hdmi->set_csp_depth = YCBCR444_16bit;
		else if (vid_para->dp == 0x02)
			hdmi->set_csp_depth = YCBCR444_12bit;
		else if (vid_para->dp == 0x01)
			hdmi->set_csp_depth = YCBCR444_10bit;
		else
			hdmi->set_csp_depth = YCBCR444_8bit;
	} else if (vid_para->cs == CS_YUV422) {
		//yuv422 12bit only
		hdmi->set_csp_depth = YCBCR422_12bit;
	} else {
		if (vid_para->dp == 0x03)
			hdmi->set_csp_depth = RGB444_16bit;
		else if (vid_para->dp == 0x02)
			hdmi->set_csp_depth = RGB444_12bit;
		else if (vid_para->dp == 0x01)
			hdmi->set_csp_depth = RGB444_10bit;
		else
			hdmi->set_csp_depth = RGB444_8bit;
	}

	if (vid_para->is_40x)
		bRxTmdsRate = vid_para->tmdsclk * 4;
	else
		bRxTmdsRate = vid_para->tmdsclk;

	hdmi->hdmi_phy_base->clk_parent = HDMIRX;
	//In pixel mode, Rx ref clock alwasy equals to pixel clock,
	//not matter deep color or YUV420, if YUV420, Rx transform to YUV444 first
	hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000;//in HZ

	msleep(100);

	drm_bridge_pre_enable(&hdmi->bridge);
	drm_bridge_enable(&hdmi->bridge);

	vTxSignalOnOff(hdmi->hdmi_phy_base, false);

	if (vid_para->hdmi_mode)
		vHdmiRxSetTxPacket();

	mtk_hdmi_mask(hdmi, HDMITX_CONFIG, HDMITX_MUX, HDMITX_MUX);
	mtk_hdmi_mask(hdmi, HDMI_BYPS_CFG,	0, rg_full_byps_mode);

	mtk_hdmi_write(hdmi, HDCP_TOP_CTRL, 0x0);

	vTxSignalOnOff(hdmi->hdmi_phy_base, true);

	return 0;
}

bool rpt_tmds_cfg(struct device *dev,
	struct MTK_VIDEO_PARA *vid_para)
{
	unsigned int bRxTmdsRate;
	//unsigned char arg;
	struct drm_display_mode *mode;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();
	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return 0;
	}

	if (vid_para == NULL) {
		HDMI_RPT_LOG("[RX]para null\n");
		return 0;
	}

	HDMI_RPT_LOG("[RX]%s\n", __func__);

	if (vid_para->is_40x)
		bRxTmdsRate = vid_para->tmdsclk * 4;
	else
		bRxTmdsRate = vid_para->tmdsclk;

	HDMI_RPT_LOG("[RX]pxl,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d\n",
		"cs", vid_para->cs,
		"dp", vid_para->dp,
		"hdmi_mode", vid_para->hdmi_mode,
		"pixclk", vid_para->pixclk,
		"tmdsclk", vid_para->tmdsclk,
		"is_40x", vid_para->is_40x,
		"id", vid_para->id);

	if (hdmi->enabled == true)
		drm_bridge_disable(&hdmi->bridge);

	if (hdmi->powered == true)
		drm_bridge_post_disable(&hdmi->bridge);

	mode = convert_id_to_drm_mode(vid_para->id);
	drm_bridge_mode_set(&hdmi->bridge, NULL, mode);

	if (vid_para->cs == CS_YUV420)
		hdmi->csp = HDMI_COLORSPACE_YUV420;
	else if (vid_para->cs == CS_YUV444)
		hdmi->csp = HDMI_COLORSPACE_YUV444;
	else if (vid_para->cs == CS_YUV422)
		hdmi->csp = HDMI_COLORSPACE_YUV422;
	else
		hdmi->csp = HDMI_COLORSPACE_RGB;

	if (vid_para->dp == 0x03)
		hdmi->color_depth = HDMI_16_BIT;
	else if (vid_para->dp == 0x02)
		hdmi->color_depth = HDMI_12_BIT;
	else if (vid_para->dp == 0x01)
		hdmi->color_depth = HDMI_10_BIT;
	else
		hdmi->color_depth = HDMI_8_BIT;

	if (vid_para->is_40x)
		bRxTmdsRate = vid_para->tmdsclk * 4;
	else
		bRxTmdsRate = vid_para->tmdsclk;

	hdmi->hdmi_phy_base->clk_parent = HDMIRX;
	//In tmds mode, Rx ref clock equals to pixel clock too, same as pixel mode
	//not matter deep color or YUV420, if YUV420, Rx transform to YUV444 first
	hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000;//in HZ

	//In tmds mode, Rx ref clock is the tmds clock, it includes deep color information
	//for instance:
	//1. RGB/YCbCr44 10bit 1080p, the Rx ref clock is the 1.25x148.5MHZ;
	//2. YCbCr422 only 12bit, YCbCr422 12bit equals to RGB/YCbCr444 8bit
	//3. YCbCr420 8bit 4K@60, the Rx ref clock is 594M/2, if 10bit, then 594MHZ/2 * 1.25
	if ((hdmi->csp == HDMI_COLORSPACE_RGB) || (hdmi->csp == HDMI_COLORSPACE_YUV444)) {
		if (hdmi->color_depth == HDMI_8_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000; //in HZ
		else if (hdmi->color_depth == HDMI_10_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000 * 5 / 4; // *1.25
		else if (hdmi->color_depth == HDMI_12_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000 * 3 / 2; // *1.5
		else if (hdmi->color_depth == HDMI_16_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000 * 2; // *2
	} else if (hdmi->csp == HDMI_COLORSPACE_YUV422)
		hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000; //equal to RGB444 8bit
	else if (hdmi->csp == HDMI_COLORSPACE_YUV420) {
		if (hdmi->color_depth == HDMI_8_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate = mode->clock * 1000 / 2; // /2
		else if (hdmi->color_depth == HDMI_10_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate =
				mode->clock * 1000 / 2 * 5 / 4; // /2*1.25
		else if (hdmi->color_depth == HDMI_12_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate =
				mode->clock * 1000 / 2 * 3 / 2; // /2*1.5
		else if (hdmi->color_depth == HDMI_16_BIT)
			hdmi->hdmi_phy_base->rx_clk_rate =
				mode->clock * 1000 / 2 * 2; // /2*2
	} else
		HDMI_RPT_LOG("ERROR:wrong color space\n");
	HDMI_RPT_LOG("Rx ref clock = %d HZ\n", hdmi->hdmi_phy_base->rx_clk_rate);

	msleep(100);

	drm_bridge_pre_enable(&hdmi->bridge);
	drm_bridge_enable(&hdmi->bridge);

	mtk_hdmi_mask(hdmi, HDMI_BYPS_CFG,
		rg_full_byps_mode, rg_full_byps_mode);

	mtk_hdmi_write(hdmi, HDCP_TOP_CTRL, 0x0);

	return 0;
}

//test
unsigned int clkrate = 296852;
unsigned int resid = 0xd;

struct MTK_VIDEO_PARA vid_para1 = {
	.cs = 0,
	.dp = 0,
	.htotal = 100,
	.vtotal = 100,
	.hactive = 100,
	.vactive = 100,
	.is_pscan = 100,
	.hdmi_mode = 1,
	.frame_rate = 60,
	.pixclk = 296852,
	.tmdsclk = 296852,
	.is_40x = 0,
	.id = 116,
};

void rpt_pixel_cfg2(void)
{
	unsigned int bRxTmdsRate;
	unsigned char arg = HDMI_VIDEO_720x480p_60Hz;
	unsigned int temp;
	struct MTK_VIDEO_PARA *vid_para;

	HDMI_RPT_FUNC();

	vid_para = &vid_para1;
	vid_para->pixclk = clkrate;
	vid_para->tmdsclk = clkrate;
	vid_para->id = resid;
	if (vid_para == NULL) {
		HDMI_RPT_LOG("[RX]para null\n");
		return;
	}

	HDMI_RPT_LOG("[RX]%s\n", __func__);

	arg = convert_id_to_tx(vid_para->id);
	arg = vid_para->id;

	HDMI_RPT_LOG("[RX]pxl,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d\n",
		"cs", vid_para->cs,
		"dp", vid_para->dp,
		"hdmi_mode", vid_para->hdmi_mode,
		"pixclk", vid_para->pixclk,
		"tmdsclk", vid_para->tmdsclk,
		"is_40x", vid_para->is_40x,
		"id", vid_para->id,
		"res", arg);
	mtk_hdmi_mask(hdmi, HDMITX_CONFIG, 0, HDMITX_MUX);
	mtk_hdmi_mask(hdmi, HDMI_BYPS_CFG, 0, rg_full_byps_mode);
	if (vid_para->cs == CS_YUV420)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_420;
	else if (vid_para->cs == CS_YUV444)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_444;
	else if (vid_para->cs == CS_YUV422)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_422;
	else
		_stAvdAVInfo.e_video_color_space = HDMI_RGB;

	if (vid_para->dp == 0x03)
		_stAvdAVInfo.e_deep_color_bit = HDMI_DEEP_COLOR_16_BIT;
	else if (vid_para->dp == 0x02)
		_stAvdAVInfo.e_deep_color_bit = HDMI_DEEP_COLOR_12_BIT;
	else if (vid_para->dp == 0x01)
		_stAvdAVInfo.e_deep_color_bit = HDMI_DEEP_COLOR_10_BIT;
	else
		_stAvdAVInfo.e_deep_color_bit = HDMI_NO_DEEP_COLOR;

	if (vid_para->is_40x)
		bRxTmdsRate = vid_para->tmdsclk * 4;
	else
		bRxTmdsRate = vid_para->tmdsclk;

	_stAvdAVInfo.e_resolution = HDMI_VIDEO_RESOLUTION_NUM;
	hdmi_internal_video_config(arg);

	if (vid_para->hdmi_mode) {
		vEnableHdmiMode(TRUE);
		vHdmiRxSetTxPacket();
	} else {
		vEnableHdmiMode(FALSE);
	}

	vWriteIoHdmiAnaMsk(HDMI_CTL_3,
			(1 << reg_osdtvd_rx_dpclr_sel_SHIFT),
			reg_osdtvd_rx_dpclr_sel);

	mtk_hdmi_mask(hdmi, HDMITX_CONFIG, 0, HDMITX_MUX);
	mtk_hdmi_mask(hdmi, HDMI_BYPS_CFG,	0, rg_full_byps_mode);

	mtk_hdmi_write(hdmi, HDCP_TOP_CTRL, 0x0);

	vTxSignalOnOff(SV_OFF);
	vSetHDMITxPLL(arg, 1, 0, bRxTmdsRate);
	vTxSignalOnOff(SV_ON);

}

void rpt_tmds_cfg2(void)
{
	unsigned int bRxTmdsRate;
	unsigned char arg;
	struct MTK_VIDEO_PARA *vid_para;

	HDMI_RPT_FUNC();

	vid_para = &vid_para1;
	vid_para->pixclk = clkrate;
	vid_para->tmdsclk = clkrate;
	vid_para->id = resid;
	if (vid_para == NULL) {
		HDMI_RPT_LOG("[RX]para null\n");
		return;
	}
	mtk_hdmi_mask(hdmi, HDMITX_CONFIG, 0, HDMITX_MUX);
	mtk_hdmi_mask(hdmi, HDMI_BYPS_CFG, 0, rg_full_byps_mode);
	HDMI_RPT_LOG("[RX]%s\n", __func__);

	if (vid_para->is_40x)
		bRxTmdsRate = vid_para->tmdsclk * 4;
	else
		bRxTmdsRate = vid_para->tmdsclk;

	if (bRxTmdsRate < (27000*4/4+74175*4/4)/2)
		arg = 2;
	else if (bRxTmdsRate < (74175*4/4+74175*8/4)/2)
		arg = 4;
	else if (bRxTmdsRate < (148350*4/4+148350*8/4)/2)
		arg = 0xd;
	else
		arg = 0x16;

	HDMI_RPT_LOG("[RX]tmds,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d\n",
		"cs", vid_para->cs,
		"dp", vid_para->dp,
		"hdmi_mode", vid_para->hdmi_mode,
		"pixclk", vid_para->pixclk,
		"tmdsclk", vid_para->tmdsclk,
		"is_40x", vid_para->is_40x,
		"id", vid_para->id,
		"res", arg);

	_stAvdAVInfo.e_resolution = arg;
	vChgHDMIVideoResolution();

	vWriteIoHdmiAnaMsk(HDMI_CTL_3,
			(1 << reg_osdtvd_rx_dpclr_sel_SHIFT),
			reg_osdtvd_rx_dpclr_sel);
	mtk_hdmi_mask(hdmi, HDMI_BYPS_CFG,
		0, rg_full_byps_mode);

	mtk_hdmi_write(hdmi, HDCP_TOP_CTRL, 0x0);

	vTxSignalOnOff(SV_OFF);
	vSetHDMITxPLL(arg, 1, 0, bRxTmdsRate);
	vTxSignalOnOff(SV_ON);

}

bool bVideoConfig(struct device *dev,
	struct MTK_VIDEO_PARA *vid_para)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_RPT_FUNC();
	if (hdmi == NULL) {
		HDMI_RPT_LOG("hdmi is NULL\n");
		return 0;
	}

	HDMI_RPT_FUNC();

	if (vid_para == NULL) {
		HDMI_RPT_LOG("[RX]para null\n");
		return 0;
	}

	if (hdmi_mode == HDMI_SOURCE_MODE) {
		mtk_hdmi_mask(hdmi, HDMITX_CONFIG, 0, HDMITX_MUX);
		mtk_hdmi_mask(hdmi, HDMI_BYPS_CFG, 0, rg_full_byps_mode);
		hdmi->hdmi_phy_base->clk_parent = XTAL26M;
		//can.zeng todo, transfer to Source mode
		//ex. clk input source shall be xtal 26M
		HDMI_RPT_LOG("[RX]source mode, return\n");
	} else if (hdmi_mode == HDMI_RPT_PIXEL_MODE)
		rpt_pixel_cfg(dev, vid_para);
	else if (hdmi_mode == HDMI_RPT_TMDS_MODE) {
		rpt_tmds_cfg(dev, vid_para);
	}

	return 0;
}

bool bAudioConfig(struct device *dev, u8 aud)
{
	HDMI_RPT_FUNC();

	//vChgHDMIAudioOutput(0, 0, 0); can.zeng todo verify
	return 0;
}

static struct MTK_HDMITX HDMITX = {
		.dev = NULL,
		.rpt_mode = vSetRptMode2,
		.get_edid = bGetEdid,
		.hdcp_start = vHdcpStart,
		.hdcp_ver = cHdcpVer,
		.hdmi_enable = vHdmiEnable,
		.is_plugin = bIsPlugIn,
		.signal_off = vSignalOff,
		.send_pkt = vTxSendPkt,
		.cfg_video = bVideoConfig,
		.cfg_audio = bAudioConfig,
};
#endif

struct MTK_HDMIRX *hdmirxhandle;
struct device *hdmirxdev;

int mtk_hdmi_repeater_register(
	struct device *dev, struct MTK_HDMIRX *rx,
	struct MTK_HDMITX **tx)
{
#if IS_ENABLED(CONFIG_MTK_HDMI_RPT)
	*tx = &HDMITX;
#else
	*tx = NULL;
#endif
	hdmirxhandle = rx;
	hdmirxdev = dev;

	pr_info("[RX]dev=0x%p,rx=0x%p,tx=0x%p\n", dev, rx, tx);

	return 0;
}
#endif
