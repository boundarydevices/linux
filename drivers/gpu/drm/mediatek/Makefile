# SPDX-License-Identifier: GPL-2.0

mediatek-drm-y := mtk_disp_aal.o \
		  mtk_disp_ccorr.o \
		  mtk_disp_color.o \
		  mtk_disp_gamma.o \
		  mtk_disp_merge.o \
		  mtk_disp_ovl.o \
		  mtk_disp_ovl_adaptor.o \
		  mtk_disp_rdma.o \
		  mtk_disp_padding.o \
		  mtk_drm_crtc.o \
		  mtk_drm_ddp_comp.o \
		  mtk_drm_drv.o \
		  mtk_drm_gem.o \
		  mtk_drm_plane.o \
		  mtk_dsi.o \
		  mtk_dpi.o \
		  mtk_ethdr.o \
		  mtk_mdp_rdma.o

obj-$(CONFIG_DRM_MEDIATEK) += mediatek-drm.o

mediatek-drm-hdmi-objs := mtk_cec.o \
			  mtk_hdmi.o \
			  mtk_hdmi_ddc.o \
			  mtk_hdmi_common.o \
			  mtk_mt8195_hdmi.o \
			  mtk_mt8195_hdmi_ddc.o \

ifneq ($(findstring $(CONFIG_DRM_MEDIATEK_HDMI_HDCP), y m),)
ccflags-y += -I$(srctree)/drivers/tee/optee/
mediatek-drm-hdmi-objs += mtk_hdmi_hdcp.o \
			  mtk_hdmi_ca.o
endif

obj-$(CONFIG_DRM_MEDIATEK_HDMI) += mediatek-drm-hdmi.o

mediatek-drm-hdmi-cec-objs := mtk_cec_v2.o
obj-$(CONFIG_DRM_MEDIATEK_CEC) += mediatek-drm-hdmi-cec.o

obj-$(CONFIG_MTK_DPTX_SUPPORT) += mtk_dp.o
obj-$(CONFIG_MTK_LVDS_SUPPORT) += mtk_lvds.o
