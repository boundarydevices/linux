/*
 * include/linux/amlogic/media/vout/lcd/lcd_vout.h
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

#ifndef _INC_LCD_VOUT_H
#define _INC_LCD_VOUT_H
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/vout_notify.h>

/* **********************************
 * debug print define
 * **********************************
 */
/* #define LCD_DEBUG_INFO */
extern unsigned char lcd_debug_print_flag;
#define LCDPR(fmt, args...)     pr_info("lcd: "fmt"", ## args)
#define LCDERR(fmt, args...)    pr_err("lcd: error: "fmt"", ## args)

/* **********************************
 * clk parameter bit define
 * pll_ctrl, div_ctrl, clk_ctrl
 * **********************************
 */
/* ******** pll_ctrl ******** */
#define PLL_CTRL_OD3                20 /* [21:20] */
#define PLL_CTRL_OD2                18 /* [19:18] */
#define PLL_CTRL_OD1                16 /* [17:16] */
#define PLL_CTRL_N                  9  /* [13:9] */
#define PLL_CTRL_M                  0  /* [8:0] */

/* ******** div_ctrl ******** */
#define DIV_CTRL_EDP_DIV1           24 /* [26:24] */
#define DIV_CTRL_EDP_DIV0           20 /* [23:20] */
#define DIV_CTRL_DIV_POST           12 /* [14:12] */
#define DIV_CTRL_DIV_PRE            8 /* [10:8] */
#define DIV_CTRL_DIV_SEL            8 /* [15:8] */
#define DIV_CTRL_XD                 0 /* [7:0] */

/* ******** clk_ctrl ******** */
#define CLK_CTRL_LEVEL              12 /* [14:12] */
#define CLK_CTRL_FRAC               0  /* [11:0] */

/* **********************************
 * VENC to TCON sync delay
 * **********************************
 */
#define TTL_DELAY                   13

/* ******** AXG ******** */
/* bit[15:11] */
#define BIT_PHY_LANE_AXG        11
#define PHY_LANE_WIDTH_AXG       5

/* MIPI-DSI */
#define DSI_LANE_0              (1 << 4)
#define DSI_LANE_1              (1 << 3)
#define DSI_LANE_CLK            (1 << 2)
#define DSI_LANE_2              (1 << 1)
#define DSI_LANE_3              (1 << 0)
#define DSI_LANE_COUNT_1        (DSI_LANE_CLK | DSI_LANE_0)
#define DSI_LANE_COUNT_2        (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1)
#define DSI_LANE_COUNT_3        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2)
#define DSI_LANE_COUNT_4        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2 | DSI_LANE_3)


/* **********************************
 * global control define
 * **********************************
 */
enum lcd_mode_e {
	LCD_MODE_TV = 0,
	LCD_MODE_TABLET,
	LCD_MODE_MAX,
};


enum lcd_chip_e {
	LCD_CHIP_GXTVBB = 0,
	LCD_CHIP_GXL,   /* 1 */
	LCD_CHIP_GXM,   /* 2 */
	LCD_CHIP_TXL,   /* 3 */
	LCD_CHIP_TXLX,  /* 4 */
	LCD_CHIP_AXG,   /* 5 */
	LCD_CHIP_MAX,
};


enum lcd_type_e {
	LCD_TTL = 0,
	LCD_LVDS,
	LCD_VBYONE,
	LCD_MIPI,
	LCD_TYPE_MAX,
};

#define MOD_LEN_MAX        30
struct lcd_basic_s {
	char model_name[MOD_LEN_MAX];
	enum lcd_type_e lcd_type;
	unsigned short lcd_bits;

	unsigned short h_active;    /* Horizontal display area */
	unsigned short v_active;    /* Vertical display area */
	unsigned short h_period;    /* Horizontal total period time */
	unsigned short v_period;    /* Vertical total period time */
	unsigned short h_period_min;
	unsigned short h_period_max;
	unsigned short v_period_min;
	unsigned short v_period_max;
	unsigned int lcd_clk_min;
	unsigned int lcd_clk_max;

	unsigned int screen_width;  /* screen physical width(unit: mm) */
	unsigned int screen_height; /* screen physical height(unit: mm) */
};

#define LCD_CLK_FRAC_UPDATE     (1 << 0)
#define LCD_CLK_PLL_CHANGE      (1 << 1)
struct lcd_timing_s {
	unsigned char clk_auto; /* clk parameters auto generation */
	unsigned int lcd_clk;   /* pixel clock(unit: Hz) */
	unsigned int lcd_clk_dft; /* internal used */
	unsigned int h_period_dft; /* internal used */
	unsigned int v_period_dft; /* internal used */
	unsigned char clk_change; /* internal used */
	unsigned int pll_ctrl;  /* pll settings */
	unsigned int div_ctrl;  /* divider settings */
	unsigned int clk_ctrl;  /* clock settings */

	unsigned char fr_adjust_type; /* 0=clock, 1=htotal, 2=vtotal */
	unsigned char ss_level;

	unsigned int sync_duration_num;
	unsigned int sync_duration_den;

	unsigned short video_on_pixel;
	unsigned short video_on_line;

	unsigned short hsync_width;
	unsigned short hsync_bp;
	unsigned short hsync_pol;
	unsigned short vsync_width;
	unsigned short vsync_bp;
	unsigned short vsync_pol;
	/* unsigned int vsync_h_phase; // [31]sign, [15:0]value */
	unsigned int h_offset;
	unsigned int v_offset;

	unsigned short de_hs_addr;
	unsigned short de_he_addr;
	unsigned short de_vs_addr;
	unsigned short de_ve_addr;

	unsigned short hs_hs_addr;
	unsigned short hs_he_addr;
	unsigned short hs_vs_addr;
	unsigned short hs_ve_addr;

	unsigned short vs_hs_addr;
	unsigned short vs_he_addr;
	unsigned short vs_vs_addr;
	unsigned short vs_ve_addr;
};

/* **********************************
 * HDR info define
 * **********************************
 */
struct lcd_hdr_info_s {
	unsigned int hdr_support;
	unsigned int features;
	unsigned int primaries_r_x;
	unsigned int primaries_r_y;
	unsigned int primaries_g_x;
	unsigned int primaries_g_y;
	unsigned int primaries_b_x;
	unsigned int primaries_b_y;
	unsigned int white_point_x;
	unsigned int white_point_y;
	unsigned int luma_max;
	unsigned int luma_min;
	unsigned int luma_avg;
};

struct ttl_config_s {
	unsigned int clk_pol;
	unsigned int sync_valid; /* [1]DE, [0]hvsync */
	unsigned int swap_ctrl; /* [1]rb swap, [0]bit swap */
};

#define LVDS_PHY_VSWING_DFT        3
#define LVDS_PHY_PREEM_DFT         0
#define LVDS_PHY_CLK_VSWING_DFT    0
#define LVDS_PHY_CLK_PREEM_DFT     0
struct lvds_config_s {
	unsigned int lvds_vswing;
	unsigned int lvds_repack;
	unsigned int dual_port;
	unsigned int pn_swap;
	unsigned int port_swap;
	unsigned int lane_reverse;
	unsigned int port_sel;
	unsigned int phy_vswing;
	unsigned int phy_preem;
	unsigned int phy_clk_vswing;
	unsigned int phy_clk_preem;
};

#define VX1_PHY_VSWING_DFT        3
#define VX1_PHY_PREEM_DFT         0

#define VX1_PWR_ON_RESET_DLY_DFT  500 /* 500ms */
#define VX1_HPD_DATA_DELAY_DFT    10 /* 10ms */
#define VX1_CDR_TRAINING_HOLD_DFT 200 /* 200ms */

struct vbyone_config_s {
	unsigned int lane_count;
	unsigned int region_num;
	unsigned int byte_mode;
	unsigned int color_fmt;
	unsigned int phy_div;
	unsigned int bit_rate;
	unsigned int phy_vswing; /*[4]:ext_pullup, [3:0]vswing*/
	unsigned int phy_preem;
	unsigned int intr_en;
	unsigned int vsync_intr_en;

	unsigned int ctrl_flag;
		/*  bit[0]:power_on_reset_en
		 *  bit[1]:hpd_data_delay_en
		 *  bit[2]:cdr_training_hold_en
		 *  bit[3]:hw_filter_en
		 *  bit[5:4]:sw_filter
		 */

	/* ctrl timing */
	unsigned int power_on_reset_delay; /* ms */
	unsigned int hpd_data_delay; /* ms */
	unsigned int cdr_training_hold; /* ms */
	/* hw filter */
	unsigned int hpd_hw_filter_time; /* ms */
	unsigned int hpd_hw_filter_cnt;
	unsigned int lockn_hw_filter_time; /* ms */
	unsigned int lockn_hw_filter_cnt;
};

/* mipi-dsi config */
/* Operation mode parameters */
#define OPERATION_VIDEO_MODE     0
#define OPERATION_COMMAND_MODE   1

#define SYNC_PULSE               0x0
#define SYNC_EVENT               0x1
#define BURST_MODE               0x2

/* command config */
#define DSI_CMD_SIZE_INDEX       1  /* byte[1] */
#define DSI_GPIO_INDEX           2  /* byte[2] */

#define DSI_INIT_ON_MAX          100
#define DSI_INIT_OFF_MAX         30

struct dsi_config_s {
	unsigned char lane_num;
	unsigned int bit_rate_max; /* MHz */
	unsigned int bit_rate_min; /* MHz*/
	unsigned int bit_rate; /* Hz */
	unsigned int clk_factor; /* bit_rate/pclk */
	unsigned int factor_numerator;
	unsigned int factor_denominator; /* 100 */
	unsigned char operation_mode_init; /* 0=video mode, 1=command mode */
	unsigned char operation_mode_display; /* 0=video mode, 1=command mode */
	unsigned char video_mode_type; /* 0=sync_pulse, 1=sync_event, 2=burst */
	unsigned char clk_lp_continuous; /* 0=stop, 1=continue */
	unsigned char phy_stop_wait; /* 0=auto, 1=standard, 2=slow */

	unsigned int venc_data_width;
	unsigned int dpi_data_format;
	unsigned int venc_fmt;

	unsigned char *dsi_init_on;
	unsigned char *dsi_init_off;
	unsigned char extern_init;
};

struct lcd_control_config_s {
	struct ttl_config_s *ttl_config;
	struct lvds_config_s *lvds_config;
	struct vbyone_config_s *vbyone_config;
	struct dsi_config_s *mipi_config;
};

/* **********************************
 * power control define
 * **********************************
 */
enum lcd_power_type_e {
	LCD_POWER_TYPE_CPU = 0,
	LCD_POWER_TYPE_PMU,
	LCD_POWER_TYPE_SIGNAL,
	LCD_POWER_TYPE_EXTERN,
	LCD_POWER_TYPE_MAX,
};

enum lcd_pmu_gpio_e {
	LCD_PMU_GPIO0 = 0,
	LCD_PMU_GPIO1,
	LCD_PMU_GPIO2,
	LCD_PMU_GPIO3,
	LCD_PMU_GPIO4,
	LCD_PMU_GPIO_MAX,
};

#define LCD_GPIO_MAX                    0xff
#define LCD_GPIO_OUTPUT_LOW             0
#define LCD_GPIO_OUTPUT_HIGH            1
#define LCD_GPIO_INPUT                  2

/* Power Control */
#define LCD_CPU_GPIO_NUM_MAX         10
struct lcd_cpu_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int flag;
};

#define LCD_PMU_GPIO_NUM_MAX         3
struct lcd_pmu_gpio_s {
	char name[15];
	int gpio;
};

#define LCD_PWR_STEP_MAX         15
struct lcd_power_step_s {
	unsigned char type;
	unsigned int index; /* point to lcd_cpu/pmu_gpio_s or lcd_extern */
	unsigned int value;
	unsigned int delay;
};

struct lcd_power_ctrl_s {
	struct lcd_cpu_gpio_s cpu_gpio[LCD_CPU_GPIO_NUM_MAX];
	struct lcd_pmu_gpio_s pmu_gpio[LCD_PMU_GPIO_NUM_MAX];
	struct lcd_power_step_s power_on_step[LCD_PWR_STEP_MAX];
	struct lcd_power_step_s power_off_step[LCD_PWR_STEP_MAX];
	int power_on_step_max; /*  internal use for debug */
	int power_off_step_max; /* internal use for debug */
};

struct lcd_clk_gate_ctrl_s {
	struct reset_control *encl;
	struct reset_control *vencl;
};

struct lcd_config_s {
	char *lcd_propname;
	unsigned int backlight_index;
	struct lcd_basic_s lcd_basic;
	struct lcd_timing_s lcd_timing;
	struct lcd_hdr_info_s hdr_info;
	struct lcd_control_config_s lcd_control;
	struct lcd_power_ctrl_s *lcd_power;
	struct pinctrl *pin;
	unsigned char pinmux_flag;
	struct lcd_clk_gate_ctrl_s rstc;
};

struct lcd_duration_s {
	unsigned int duration_num;
	unsigned int duration_den;
};

struct aml_lcd_drv_s {
	char *version;
	enum lcd_chip_e chip_type;
	unsigned char lcd_mode;
	unsigned char lcd_status;
	unsigned char lcd_key_valid;
	unsigned char lcd_config_load;
	unsigned char lcd_test_flag;
	unsigned char lcd_resume_flag; /* 0=directly, 1=workqueue */
	unsigned char lcd_mute;

	struct clk *vencl_top;
	struct clk *vencl_int;
	struct clk *dsi_host;
	struct clk *dsi_phy;
	struct clk *dsi_meas;
	struct clk *mipi_enable_gate;
	struct clk *mipi_bandgap_gate;

	struct device *dev;
	struct lcd_config_s *lcd_config;
	struct vinfo_s *lcd_info;
	struct class *lcd_debug_class;
	int fr_auto_policy;
	struct lcd_duration_s std_duration;

	void (*vout_server_init)(void);
	void (*driver_init_pre)(void);
	int (*driver_init)(void);
	void (*driver_disable)(void);
	int (*driver_change)(void);
	void (*module_reset)(void);
	void (*power_tiny_ctrl)(int status);
	void (*driver_tiny_enable)(void);
	void (*driver_tiny_disable)(void);
	void (*module_tiny_reset)(void);
	void (*lcd_test_pattern_restore)(void);
	void (*power_ctrl)(int status);

	struct workqueue_struct *workqueue;
	struct delayed_work lcd_probe_delayed_work;
	struct delayed_work lcd_vx1_delayed_work;
	struct work_struct  lcd_resume_work;
};

extern struct aml_lcd_drv_s *aml_lcd_get_driver(void);


/* **********************************
 * IOCTL define
 * **********************************
 */
#define LCD_IOC_TYPE               'C'
#define LCD_IOC_NR_GET_HDR_INFO    0x0
#define LCD_IOC_NR_SET_HDR_INFO    0x1

#define LCD_IOC_CMD_GET_HDR_INFO   \
	_IOR(LCD_IOC_TYPE, LCD_IOC_NR_GET_HDR_INFO, struct lcd_hdr_info_s)
#define LCD_IOC_CMD_SET_HDR_INFO   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_NR_SET_HDR_INFO, struct lcd_hdr_info_s)

#endif
