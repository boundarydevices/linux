#ifndef __PANEL_SN65DSI83_H__
#define __PANEL_SN65DSI83_H__

/* register definitions according to the sn65dsi83 data sheet */
#define SN_SOFT_RESET		0x09
#define SN_CLK_SRC		0x0a
#define SN_CLK_DIV		0x0b

#define SN_PLL_EN		0x0d
#define SN_DSI_LANES		0x10
#define SN_DSI_EQ		0x11
#define SN_DSI_CLK		0x12
#define SN_FORMAT		0x18

#define SN_LVDS_VOLTAGE		0x19
#define SN_LVDS_TERM		0x1a
#define SN_LVDS_CM_VOLTAGE	0x1b
#define SN_HACTIVE_LOW		0x20
#define SN_HACTIVE_HIGH		0x21
#define SN_VACTIVE_LOW		0x24
#define SN_VACTIVE_HIGH		0x25
#define SN_SYNC_DELAY_LOW	0x28
#define SN_SYNC_DELAY_HIGH	0x29
#define SN_HSYNC_LOW		0x2c
#define SN_HSYNC_HIGH		0x2d
#define SN_VSYNC_LOW		0x30
#define SN_VSYNC_HIGH		0x31
#define SN_HBP			0x34
#define SN_VBP			0x36
#define SN_HFP			0x38
#define SN_VFP			0x3a
#define SN_TEST_PATTERN		0x3c
#define SN_IRQ_EN		0xe0
#define SN_IRQ_MASK		0xe1
#define SN_IRQ_STAT		0xe5

struct panel_sn65dsi83
{
	struct device		*dev;
	struct i2c_adapter	*i2c;
	unsigned		i2c_max_frequency;
	unsigned		i2c_address;
	struct device_node	*disp_dsi;
	struct gpio_desc	*gp_en;
	struct clk		*mipi_clk;
	struct clk		*pixel_clk;
	struct mutex		power_mutex;
	struct videomode	vm;
	int			irq;
	u32			int_cnt;
	u8			chip_enabled;
	u8			irq_enabled;
	u8			show_reg;
	u8			dsi_lanes;
	u8			spwg;	/* lvds lane 3 has MSBs of color */
	u8			jeida;	/* lvds lane 3 has LSBs of color */
	u8			split_mode;
	u8			dsi_bpp;
	u16			sync_delay;
	u16			hbp;

	u8			dsi_clk_divider;
	u8			mipi_clk_index;
#define SN_STATE_OFF		0
#define SN_STATE_STANDBY	1
#define SN_STATE_ON		2
	u8			state;
	unsigned char tx_buf[63] __attribute__((aligned(64)));
	unsigned char rx_buf[63] __attribute__((aligned(64)));
};

int sn65_init(struct device *dev, struct panel_sn65dsi83 *sn,
		struct device_node *disp_dsi, struct device_node *np);
int sn65_remove(struct panel_sn65dsi83 *sn);

void sn65_enable(struct panel_sn65dsi83 *sn);
void sn65_enable2(struct panel_sn65dsi83 *sn);
void sn65_disable(struct panel_sn65dsi83 *sn);
struct panel_sn65dsi83 *dev_to_panel_sn65(struct device *dev);
#endif
