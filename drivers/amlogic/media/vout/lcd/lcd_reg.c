/*
 * drivers/amlogic/media/vout/lcd/lcd_reg.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_common.h"
#include "lcd_reg.h"

/* ********************************************* */
#define LCD_REG_GLOBAL_API     0
#define LCD_REG_IOREMAP        1
#define LCD_REG_IF             LCD_REG_IOREMAP
/* ********************************************* */

#if (LCD_REG_IF == LCD_REG_IOREMAP)
struct reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
	char dummy;
};
static struct reg_map_s *lcd_map;
static int lcd_map_num;

#define LCD_MAP_HIUBUS    0
#define LCD_MAP_VCBUS     1
#define LCD_MAP_PERIPHS   2
#define LCD_MAP_CBUS      3
#define LCD_MAP_DSI_HOST  4
#define LCD_MAP_DSI_PHY   5

static struct reg_map_s lcd_reg_maps_gxb[] = {
	{ /* HIU */
		.base_addr = 0xc883c000,
		.size = 0x400,
		.flag = 0,
		.dummy = 0,
	},
	{ /* VCBUS */
		.base_addr = 0xd0100000,
		.size = 0x10000,
		.flag = 0,
		.dummy = 0,
	},
	{ /* PERIPHS */
		.base_addr = 0xc8834400,
		.size = 0x100,
		.flag = 0,
		.dummy = 0,
	},
	{ /* CBUS */
		.base_addr = 0xc1100000,
		.size = 0x8000,
		.flag = 0,
		.dummy = 0,
	},
};

static struct reg_map_s lcd_reg_maps_txlx[] = {
	{ /* HIU */
		.base_addr = 0xff63c000,
		.size = 0x400,
		.flag = 0,
		.dummy = 0,
	},
	{ /* VCBUS */
		.base_addr = 0xff900000,
		.size = 0xa000,
		.flag = 0,
		.dummy = 0,
	},
	{ /* PERIPHS */
		.base_addr = 0xff634000,
		.size = 0x100,
		.flag = 0,
		.dummy = 0,
	},
};

static struct reg_map_s lcd_reg_maps_axg[] = {
	{ /* HIU */
		.base_addr = 0xff63c000,
		.size = 0x400,
		.flag = 0,
		.dummy = 0,
	},
	{ /* VCBUS */
		.base_addr = 0xff900000,
		.size = 0xa000,
		.flag = 0,
		.dummy = 0,
	},
	{ /* PERIPHS */
		.base_addr = 0xff634000,
		.size = 0x100,
		.flag = 0,
		.dummy = 0,
	},
	{ /* CBUS, dummy */
		.base_addr = 0xffd00000,
		.size = 0x10,
		.flag = 0,
		.dummy = 1,
	},
	{ /* mipi_dsi_host */
		.base_addr = 0xffd00000, /* 0xffd06000 */
		.size = 0x6400,
		.flag = 0,
		.dummy = 0,
	},
	{ /* mipi_dsi_phy */
		.base_addr = 0xff640000,
		.size = 0x100,
		.flag = 0,
		.dummy = 0,
	},
};

int lcd_ioremap(void)
{
	int i;
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_map = NULL;
	lcd_map_num = 0;

	switch (lcd_drv->chip_type) {
	case LCD_CHIP_TXLX:
		lcd_map = lcd_reg_maps_txlx;
		lcd_map_num = ARRAY_SIZE(lcd_reg_maps_txlx);
		break;
	case LCD_CHIP_AXG:
		lcd_map = lcd_reg_maps_axg;
		lcd_map_num = ARRAY_SIZE(lcd_reg_maps_axg);
		break;
	default:
		lcd_map = lcd_reg_maps_gxb;
		lcd_map_num = ARRAY_SIZE(lcd_reg_maps_gxb);
		break;
	}

	for (i = 0; i < lcd_map_num; i++) {
		if (lcd_map[i].dummy)
			continue;
		lcd_map[i].p = ioremap(lcd_map[i].base_addr,
					lcd_map[i].size);
		if (lcd_map[i].p == NULL) {
			lcd_map[i].flag = 0;
			LCDPR("reg map failed: 0x%x\n",
				lcd_map[i].base_addr);
			ret = -1;
		} else {
			lcd_map[i].flag = 1;
			if (lcd_debug_print_flag) {
				LCDPR("reg mapped: 0x%x -> %p\n",
					lcd_map[i].base_addr, lcd_map[i].p);
			}
		}
	}
	return ret;
}

static int check_lcd_ioremap(int n)
{
	if (lcd_map == NULL)
		return -1;
	if (n >= lcd_map_num)
		return -1;

	if (lcd_map[n].dummy) {
		LCDERR("reg 0x%x is invalid(dummy)\n", lcd_map[n].base_addr);
		return -1;
	}
	if (lcd_map[n].flag == 0) {
		LCDERR("reg 0x%x mapped error\n", lcd_map[n].base_addr);
		return -1;
	}
	return 0;
}
#else
int lcd_ioremap(void)
{
	LCDPR("reg interface is global api\n");
	return 0;
}
#endif

/* register mapping check */
#if (LCD_REG_IF == LCD_REG_IOREMAP)
static inline void __iomem *check_lcd_vcbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_VCBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET_VCBUS(_reg);
	if (reg_offset >= lcd_map[reg_bus].size) {
		LCDERR("invalid vcbus reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		reg_bus = LCD_MAP_HIUBUS;
	else
		reg_bus = LCD_MAP_CBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	if (reg_bus == LCD_MAP_HIUBUS)
		reg_offset = LCD_REG_OFFSET_HIU(_reg);
	else
		reg_offset = LCD_REG_OFFSET_CBUS(_reg);
	if (reg_offset >= lcd_map[reg_bus].size) {
		LCDERR("invalid hiu reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_map[reg_bus].p + reg_offset;

	return p;
}

static inline void __iomem *check_lcd_cbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_CBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET_CBUS(_reg);
	if (reg_offset >= lcd_map[reg_bus].size) {
		LCDERR("invalid cbus reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_periphs_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		reg_bus = LCD_MAP_PERIPHS;
	else
		reg_bus = LCD_MAP_CBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	if (reg_bus == LCD_MAP_PERIPHS)
		reg_offset = LCD_REG_OFFSET_PERIPHS(_reg);
	else
		reg_offset = LCD_REG_OFFSET_PERIPHS(_reg);
	if (reg_offset >= lcd_map[reg_bus].size) {
		LCDERR("invalid periphs reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_dsi_host_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_DSI_HOST;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET_DSI_HOST(_reg);
	if (reg_offset >= lcd_map[reg_bus].size) {
		LCDERR("invalid dsi_host reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_dsi_phy_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_DSI_PHY;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET_DSI_PHY(_reg);
	if (reg_offset >= lcd_map[reg_bus].size) {
		LCDERR("invalid dsi_phy reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_map[reg_bus].p + reg_offset;
	return p;
}
#endif

/* register access api */
#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_vcbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_vcbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_vcbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_vcbus_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_vcbus_read(unsigned int reg)
{
	return aml_read_vcbus(reg);
};

void lcd_vcbus_write(unsigned int reg, unsigned int value)
{
	aml_write_vcbus(reg, value);
};
#endif

void lcd_vcbus_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	lcd_vcbus_write(reg, ((lcd_vcbus_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

unsigned int lcd_vcbus_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (lcd_vcbus_read(reg) >> _start) & ((1L << _len)-1);
}

void lcd_vcbus_set_mask(unsigned int reg, unsigned int _mask)
{
	lcd_vcbus_write(reg, (lcd_vcbus_read(reg) | (_mask)));
}

void lcd_vcbus_clr_mask(unsigned int reg, unsigned int _mask)
{
	lcd_vcbus_write(reg, (lcd_vcbus_read(reg) & (~(_mask))));
}

#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_hiu_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_hiu_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_hiu_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_hiu_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void lcd_hiu_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};
#endif

void lcd_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	lcd_hiu_write(_reg, ((lcd_hiu_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int lcd_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (lcd_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void lcd_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	lcd_hiu_write(_reg, (lcd_hiu_read(_reg) | (_mask)));
}

void lcd_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	lcd_hiu_write(_reg, (lcd_hiu_read(_reg) & (~(_mask))));
}

#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_cbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_cbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_cbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_cbus_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_cbus_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void lcd_cbus_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};
#endif

void lcd_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	lcd_cbus_write(_reg, ((lcd_cbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_periphs_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_periphs_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_periphs_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_periphs_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_periphs_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void lcd_periphs_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};
#endif

void lcd_pinmux_set_mask(unsigned int n, unsigned int _mask)
{
	unsigned int _reg = PERIPHS_PIN_MUX_0;

	_reg += n;
	lcd_periphs_write(_reg, (lcd_periphs_read(_reg) | (_mask)));
}

void lcd_pinmux_clr_mask(unsigned int n, unsigned int _mask)
{
	unsigned int _reg = PERIPHS_PIN_MUX_0;

	_reg += n;
	lcd_periphs_write(_reg, (lcd_periphs_read(_reg) & (~(_mask))));
}

#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int dsi_host_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_dsi_host_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void dsi_host_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_dsi_host_reg(_reg);
	if (p)
		writel(_value, p);
};

void dsi_host_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	dsi_host_write(reg, ((dsi_host_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

unsigned int dsi_host_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (dsi_host_read(reg) >> _start) & ((1L << _len)-1);
}

void dsi_host_set_mask(unsigned int reg, unsigned int _mask)
{
	dsi_host_write(reg, (dsi_host_read(reg) | (_mask)));
}

void dsi_host_clr_mask(unsigned int reg, unsigned int _mask)
{
	dsi_host_write(reg, (dsi_host_read(reg) & (~(_mask))));
}


unsigned int dsi_phy_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_dsi_phy_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void dsi_phy_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_dsi_phy_reg(_reg);
	if (p)
		writel(_value, p);
};

void dsi_phy_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	dsi_phy_write(reg, ((dsi_phy_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

unsigned int dsi_phy_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (dsi_phy_read(reg) >> _start) & ((1L << _len)-1);
}

void dsi_phy_set_mask(unsigned int reg, unsigned int _mask)
{
	dsi_phy_write(reg, (dsi_phy_read(reg) | (_mask)));
}

void dsi_phy_clr_mask(unsigned int reg, unsigned int _mask)
{
	dsi_phy_write(reg, (dsi_phy_read(reg) & (~(_mask))));
}
#endif

