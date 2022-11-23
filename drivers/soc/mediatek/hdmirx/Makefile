#
# Makefile for the kernel multimedia device drivers.
#

ccflags-y += -I$(srctree)/include/linux/drm/

obj-$(CONFIG_MTK_HDMI_RX) += mtk_hdmirx.o
mtk_hdmirx-objs :=hdmirx_drv.o \
	hdmi_rx2_aud_task.o \
	hdmi_rx2_cmd.o \
	hdmi_rx2_ctrl.o \
	hdmi_rx2_dvi.o \
	hdmi_rx2_hal.o \
	hdmi_rx2_hdcp.o \
	hdmi_rx2_edid.o \
	hdmi_rx21_phy.o \
	vga_table.o
