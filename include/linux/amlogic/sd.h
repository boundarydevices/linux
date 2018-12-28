/*
 * include/linux/amlogic/sd.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef __AML_SD_H__
#define __AML_SD_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mmc/host.h>
/* #include <linux/earlysuspend.h> */
#undef pr_fmt
#define pr_fmt(fmt) "meson-mmc: " fmt

#define	 AML_ERROR_RETRY_COUNTER		 10
#define	 AML_TIMEOUT_RETRY_COUNTER	   2
#define AML_CALIBRATION
#define AML_SDHC_MAGIC		"amlsdhc"
#define AML_SDIO_MAGIC		"amlsdio"
#define AML_SD_EMMC_MAGIC	"amlsd_emmc"
#define SD_EMMC_MANUAL_CMD23
#define MAX_TUNING_RETRY 4
#define TUNING_NUM_PER_POINT 40
#define CALI_PATTERN_OFFSET ((SZ_1M * (36 + 3)) / 512)
/* #define AML_RESP_WR_EXT */
/* pio to transfer data */
#define CFG_SDEMMC_PIO		(1)

#ifdef AML_CALIBRATION
#define MAX_CALI_RETRY	3
#define MAX_DELAY_CNT	16
#define CALI_BLK_CNT	80
#endif

#define SD_EMMC_CLOCK 0x0
#define SD_EMMC_DELAY 0x4
#define SD_EMMC_ADJUST 0x8
#define SD_EMMC_CALOUT 0x10
#define SD_EMMC_START 0x40
#define SD_EMMC_CFG 0x44
#define SD_EMMC_STATUS 0x48
#define SD_EMMC_IRQ_EN 0x4c
#define SD_EMMC_CMD_RSP 0x5c
#define SD_EMMC_CMD_RSP1 0x60
#define SD_EMMC_CMD_RSP2 0x64
#define SD_EMMC_CMD_RSP3 0x68

#define SD_EMMC_CLOCK_V3 0x0
#define SD_EMMC_DELAY1_V3 0x4
#define SD_EMMC_DELAY2_V3 0x8
#define SD_EMMC_ADJUST_V3 0xc
#define SD_EMMC_ADJ_IDX_LOG 0x20
#define SD_EMMC_CLKTEST_LOG	0x24
#define SD_EMMC_CLKTEST_OUT	0x28
#define SD_EMMC_EYETEST_LOG	0x2C
#define SD_EMMC_EYETEST_OUT0 0x30
#define SD_EMMC_EYETEST_OUT1 0x34
#define SD_EMMC_INTF3	0x38

#define SD_EMMC_DESC_OFF	0x200
/* using SD_EMMC_CMD_RSP */
#define SD_EMMC_RESP_SIZE	0x0
#define SD_EMMC_DESC_SIZE	(0x200 - SD_EMMC_RESP_SIZE)
#define SD_EMMC_RESP_OFF	(SD_EMMC_DESC_OFF + SD_EMMC_DESC_SIZE)

#define SD_EMMC_PING_OFF	0x400
#define SD_EMMC_PING_SIZE	0x200
#define SD_EMMC_PONG_OFF	0x600
#define SD_EMMC_PONE_SIZE	0x200
/* join ping/pong as one */
#define SD_EMMC_PIO_OFF		(SD_EMMC_PING_OFF)
#define SD_EMMC_PIO_SIZE	(SD_EMMC_PING_SIZE + SD_EMMC_PONE_SIZE)

#define   CLK_DIV_SHIFT 0
#define   CLK_DIV_WIDTH 6
#define   CLK_DIV_MASK 0x3f
#define   CLK_DIV_MAX 63
#define   CLK_SRC_SHIFT 6
#define   CLK_SRC_WIDTH 2
#define   CLK_SRC_MASK 0x3
#define   CLK_SRC_XTAL_RATE 24000000
#define   CLK_SRC_PLL_RATE 1000000000

#define   CFG_BLK_LEN_SHIFT 4
#define   CFG_BLK_LEN_MASK 0xf

#define CMD_CFG_LENGTH_SHIFT 0
#define CMD_CFG_LENGTH_MASK 0x1ff
#define CMD_CFG_BLOCK_MODE BIT(9)
#define CMD_CFG_DATA_IO BIT(18)
#define CMD_CFG_DATA_WR BIT(19)
#define CMD_CFG_DATA_NUM BIT(23)

#define CMD_DATA_MASK (~0x3)

struct aml_tuning_data {
	const u8 *blk_pattern;
	unsigned int blksz;
};

enum aml_mmc_waitfor {
	XFER_INIT,			  /* 0 */
	XFER_START,				/* 1 */
	XFER_AFTER_START,		/* 2 */
	XFER_IRQ_OCCUR,			/* 3 */
	XFER_IRQ_TASKLET_CMD,	/* 4 */
	XFER_IRQ_TASKLET_DATA,	/* 5 */
	XFER_IRQ_TASKLET_BUSY,	/* 6 */
	XFER_IRQ_UNKNOWN_IRQ,	/* 7 */
	XFER_TIMER_TIMEOUT,		/* 8 */
	XFER_TASKLET_CMD,		/* 9 */
	XFER_TASKLET_DATA,		/* 10 */
	XFER_TASKLET_BUSY,		/* 11 */
	XFER_TIMEDOUT,			/* 12 */
	XFER_FINISHED,			/* 13 */
};

enum aml_host_status { /* Host controller status */
	HOST_INVALID = 0,	   /* 0, invalid value */
	HOST_RX_FIFO_FULL = 1,  /* 1, start with 1 */
	HOST_TX_FIFO_EMPTY,		/* 2 */
	HOST_RSP_CRC_ERR,		/* 3 */
	HOST_DAT_CRC_ERR,		/* 4 */
	HOST_RSP_TIMEOUT_ERR,   /* 5 */
	HOST_DAT_TIMEOUT_ERR,   /* 6 */
	HOST_ERR_END,			/* 7, end of errors */
	HOST_TASKLET_CMD,		/* 8 */
	HOST_TASKLET_DATA,		/* 9 */
};

enum aml_host_bus_fsm { /* Host bus fsm status */
	BUS_FSM_IDLE,			/* 0, idle */
	BUS_FSM_SND_CMD,		/* 1, send cmd */
	BUS_FSM_CMD_DONE,		/* 2, wait for cmd done */
	BUS_FSM_RESP_START,		/* 3, resp start */
	BUS_FSM_RESP_DONE,		/* 4, wait for resp done */
	BUS_FSM_DATA_START,		/* 5, data start */
	BUS_FSM_DATA_DONE,		/* 6, wait for data done */
	BUS_FSM_DESC_WRITE_BACK,/* 7, wait for desc write back */
	BUS_FSM_IRQ_SERVICE,	/* 8, wait for irq service */
};

enum aml_host_tuning_mode {
	NONE_TUNING,
	ADJ_TUNING_MODE,
	AUTO_TUNING_MODE,
	RX_PHASE_DELAY_TUNING_MODE,
};

struct cali_data {
	u8 ln_delay[8];
	u32 base_index[10];
	u32 base_index_max;
	u32 base_index_min;
};

struct cali_ctrl {
	u8 line_x;
	u8 cal_time;
	u8 dly_tmp;
	u8 max_index;
};

enum mmc_chip_e {
	MMC_CHIP_M8B = 0x1B,
	MMC_CHIP_GXBB = 0x1F,
	MMC_CHIP_GXTVBB = 0x20,
	MMC_CHIP_GXL = 0x21,
	MMC_CHIP_GXM = 0x22,
	MMC_CHIP_TXL = 0x23,
	MMC_CHIP_TXLX = 0x24,
	MMC_CHIP_AXG = 0x25,
	MMC_CHIP_GXLX = 0x26,
	MMC_CHIP_TXHD = 0x27,
	MMC_CHIP_G12A = 0x28,
	MMC_CHIP_G12B = 0x29,
	MMC_CHIP_GXLX2 = 0x2a,
	MMC_CHIP_TL1 = 0X2b,
};

struct mmc_phase {
	unsigned int core_phase;
	unsigned int tx_phase;
	unsigned int rx_phase;
	unsigned int tx_delay;
};

struct para_e {
	struct mmc_phase init;
	struct mmc_phase hs;
	struct mmc_phase calc;
	struct mmc_phase ddr;
	struct mmc_phase hs2;
	struct mmc_phase hs4;
	struct mmc_phase sd_hs;
	struct mmc_phase sdr104;
};

struct meson_mmc_data {
	enum mmc_chip_e chip_type;
	unsigned int port_a_base;
	unsigned int port_b_base;
	unsigned int port_c_base;
	unsigned int pinmux_base;
	unsigned int clksrc_base;
	unsigned int ds_pin_poll;
	unsigned int ds_pin_poll_en;
	unsigned int ds_pin_poll_bit;
	unsigned int latest_dat;
	unsigned int tdma_f;
	struct para_e sdmmc;
};

struct amlsd_host;
struct sd_emmc_desc_info;

struct clock_lay_t {
	/* source clk, 24Mhz, 1Ghz */
	unsigned int source;
	/* core clk, Hz */
	unsigned int core;
	/* old core clk, Hz */
	unsigned int old_core;
	/* bus clk */
	unsigned int sdclk;
};

/* todly in ns*/
#define TODLY_MIN_NS	(2)
#define TODLY_MAX_NS	(14)

struct amlsd_platform {
	struct amlsd_host *host;
	struct mmc_host *mmc;
	struct list_head sibling;
	u32 ocr_avail;
	u32 port;
#define	 PORT_SDIO_A	 0
#define	 PORT_SDIO_B	 1
#define	 PORT_SDIO_C	 2
#define	 PORT_SDHC_A	 3
#define	 PORT_SDHC_B	 4
#define	 PORT_SDHC_C	 5

#ifdef CONFIG_AMLOGIC_M8B_MMC
	unsigned int width;
	unsigned int tune_phase;	/* store tuning result */
#endif
	struct delayed_work cd_detect;
	unsigned int caps;
	unsigned int caps2;
	unsigned int pm_caps;
	unsigned int card_capacity;
	unsigned int tx_phase;
	unsigned int tx_delay;
	unsigned int co_phase;
	unsigned int f_min;
	unsigned int f_max;
	unsigned int clkc;
	unsigned int clk2;
	unsigned int clkc_w;
	unsigned int ctrl;
	unsigned int adj;
	unsigned int dly1;
	unsigned int dly2;
	unsigned int intf3;
	unsigned int irq_sdio_sleep;
	unsigned int clock;
	/* signalling voltage (1.8V or 3.3V) */
	unsigned char signal_voltage;
	struct clock_lay_t clk_lay;
	int	bus_width;
	int	bl_len;
	int	stop_clk;

	unsigned int low_burst;
	struct mutex in_out_lock;
	unsigned int irq_cd;
	unsigned int gpio_cd;
	unsigned int gpio_cd_level;
	unsigned int gpio_cd_sta;
	unsigned int gpio_power;
	unsigned int power_level;
	unsigned int calc_f;

	unsigned int auto_clk_close;
	unsigned int vol_switch;
	unsigned int vol_switch_18;
	unsigned int vol_switch_delay;
	char pinname[32];
	char dmode[8];
	unsigned int gpio_ro;
	unsigned int gpio_dat3;
	unsigned int hw_reset;
	unsigned int jtag_pin;
	int is_sduart;
	unsigned int card_in_delay;
	bool is_in;
	bool is_tuned;		/* if card has been tuning */
	bool need_retuning;
	bool rmpb_cmd_flag;
	bool rpmb_valid_command;
	/* we used this flag to filter
	 * some unnecessary cmd before initialized flow
	 */
	/* has been initialized for the first time */
	bool is_fir_init;
	struct delayed_work	retuning;
#ifdef AML_CALIBRATION
	unsigned char caling;
	unsigned char calout[20][20];
#endif
	unsigned int latest_dat;
	u64 align[10];
	int base_line;
	unsigned int count;
	unsigned int delay_cell;
	/* int order; */
	unsigned int rx_err;
	/* 0:unknown, 1:mmc card(include eMMC), 2:sd card(include tSD),
	 * 3:sdio device(ie:sdio-wifi), 4:SD combo (IO+mem) card,
	 * 5:NON sdio device(means sd/mmc card), other:reserved
	 */
	unsigned int card_type;
	struct cali_ctrl c_ctrl;
	/* unknown */
#define CARD_TYPE_UNKNOWN		0
	/* MMC card */
#define CARD_TYPE_MMC			1
	/* SD card */
#define CARD_TYPE_SD			2
	/* SDIO card */
#define CARD_TYPE_SDIO			3
	/* SD combo (IO+mem) card */
#define CARD_TYPE_SD_COMBO		4
	/* NON sdio device (means SD/MMC card) */
#define CARD_TYPE_NON_SDIO		5

#define aml_card_type_unknown(c)	((c)->card_type == CARD_TYPE_UNKNOWN)
#define aml_card_type_mmc(c)		((c)->card_type == CARD_TYPE_MMC)
#define aml_card_type_sd(c)		 ((c)->card_type == CARD_TYPE_SD)
#define aml_card_type_sdio(c)	   ((c)->card_type == CARD_TYPE_SDIO)
#define aml_card_type_non_sdio(c)   ((c)->card_type == CARD_TYPE_NON_SDIO)

	/* struct pinctrl *uart_ao_pinctrl; */
	void (*irq_init)(struct amlsd_platform *pdata);

	unsigned int max_blk_count;
	unsigned int max_blk_size;
	unsigned int max_req_size;
	unsigned int max_seg_size;

	/*for inand partition: struct mtd_partition, easy porting from nand*/
	struct mtd_partition *parts;
	unsigned int nr_parts;

	struct resource *resource;
	void (*xfer_pre)(struct amlsd_platform *pdata);
	void (*xfer_post)(struct amlsd_platform *pdata);

	int (*port_init)(struct amlsd_platform *pdata);
	int (*cd)(struct amlsd_platform *pdata);
	int (*ro)(struct amlsd_platform *pdata);
	void (*pwr_pre)(struct amlsd_platform *pdata);
	void (*pwr_on)(struct amlsd_platform *pdata);
	void (*pwr_off)(struct amlsd_platform *pdata);

};

struct aml_emmc_adjust {
	int adj_win_start;
	int adj_win_len;
	int adj_point;
	int clk_div;
};

struct aml_emmc_rxclk {
	int rxclk_win_start;
	int rxclk_win_len;
	int rxclk_rx_phase;
	int rxclk_rx_delay;
	int rxclk_point;
};

#define MUX_CLK_NUM_PARENTS 2
struct amlsd_host {
	/* back-link to device */
	struct device *dev;
	struct list_head sibling;
	struct platform_device *pdev;
	struct amlsd_platform *pdata;
	struct mmc_host		*mmc;
	struct mmc_request	*request;
	struct meson_mmc_data *data;

	struct mmc_command	*cmd;
	u32 ocr_mask;
	struct clk *core_clk;
	struct clk_mux mux;
	struct clk *mux_clk;
	struct clk *mux_parent[MUX_CLK_NUM_PARENTS];
	unsigned long mux_parent_rate[MUX_CLK_NUM_PARENTS];
	struct clk_divider cfg_div;
	struct clk *cfg_div_clk;
#ifdef CONFIG_AMLOGIC_M8B_MMC
	struct clk *div3_clk;
#endif

	struct resource		*mem;
	struct sd_emmc_regs *sd_emmc_regs;
	void __iomem		*base;
	void __iomem		*pinmux_base;
	void __iomem		*clksrc_base;
	int			dma;
	char *bn_buf;
	dma_addr_t		bn_dma_buf;
#ifdef AML_RESP_WR_EXT
	u32 *resp_buf;
	dma_addr_t resp_dma_buf;
#endif
	dma_addr_t		dma_gdesc; /* 0x200 */
	dma_addr_t		dma_gping; /* 0x400 */
	dma_addr_t		dma_gpong; /* 0x800 */
	char is_tunning;
	char is_timming;
	char tuning_mode;
	unsigned int is_sduart;
	unsigned int irq;
	unsigned int irq_in;
	unsigned int irq_out;
	unsigned int f_max;
	unsigned int f_max_w;
	unsigned int f_min;
	int	sdio_irqen;
	unsigned int error_bak;
	struct delayed_work	timeout;
	struct class debug;

	unsigned int send;
	unsigned int ctrl;
	unsigned int clkc;
	unsigned int misc;
	unsigned int ictl;
	unsigned int ista;
	unsigned int dma_addr;

	unsigned long		clk_rate;

	u8 *blk_test;
	char *desc_buf;
#ifdef CFG_SDEMMC_PIO
	/* bounce buffer to accomplish 32bit apb access */
	u8 *desc_bn;
	/* pio buffer */
	u8 *pio_buf;
	dma_addr_t pio_dma_buf;
#endif
	dma_addr_t		desc_dma_addr;
	unsigned int dma_sts;
	unsigned int sg_cnt;
	char *desc_cur;
	unsigned int desc_cur_cnt;
	char *desc_pre;
	unsigned int desc_pre_cnt;
	struct  mmc_request	*mrq;
	struct  mmc_request	*mrq2;
	spinlock_t	mrq_lock;
	struct mutex	pinmux_lock;
	struct completion   drv_completion;
	int			cmd_is_stop;
	enum aml_mmc_waitfor	xfer_step;
	enum aml_mmc_waitfor	xfer_step_prev;

	int	 port;
	int	 locked;
	bool	is_gated;
	unsigned char sd_sdio_switch_volat_done;

	int	 status; /* host status: xx_error/ok */
	int init_flag;
	int init_volt;

	char	*msg_buf;
#define MESSAGE_BUF_SIZE			512

#ifdef CONFIG_DEBUG_FS
	struct dentry		*debug_root;
	struct dentry		*debug_state;
	struct dentry		*debug_regs;
#endif

#ifdef CONFIG_CPU_FREQ
	struct notifier_block	freq_transition;
#endif

	u32			opcode;
	u32			arg;
	u32		 cmd25_cnt;

#ifdef CONFIG_MMC_AML_DEBUG
	u32		 req_cnt;
	u32		 trans_size;

	u32		 reg_buf[16];
#endif
	u32		 time_req_sta; /* request start time */

	struct pinctrl  *pinctrl;
	char		pinctrl_name[30];
	/* used for judging if there is a tsd/emmc */
	int		 storage_flag;
	/* bit[7-0]--minor version, bit[31-8]--major version */
	int		 version;
	int ctrl_ver;
	unsigned long	clksrc_rate;
	struct aml_emmc_adjust emmc_adj;
	struct aml_emmc_rxclk emmc_rxclk;
	u32 error_flag;
	/* pre cmd op */
	unsigned int (*pre_cmd_op)(struct amlsd_host *host,
		struct mmc_request *mrq, struct sd_emmc_desc_info *desc);
	/* post cmd op */
	int (*post_cmd_op)(struct amlsd_host *host,
		struct mmc_request *mrq);
};

/*-sdio-*/

#define SDIO_ARGU	   (0x0)
#define SDIO_SEND	   (0x4)
#define SDIO_CONF	   (0x8)
#define SDIO_IRQS	   (0xc)
#define SDIO_IRQC	   (0x10)
#define SDIO_MULT	   (0x14)
#define SDIO_ADDR	   (0x18)
#define SDIO_EXT		(0x1c)
#define SDIO_CCTL	   (0x40)
#define SDIO_CDAT	   (0x44)

#define CLK_DIV		 (0x1f4)

struct cmd_send {
	u32 cmd_command:8; /*[7:0] Command Index*/
	u32 cmd_response_bits:8;
	/*[15:8]
	 * 00 means no response
	 * others: Response bit number
	 * (cmd bits+response bits+crc bits-1)
	 */
	u32 response_do_not_have_crc7:1;
	/*[16]
	 * 0:Response need check CRC7,
	 * 1: dont need check
	 */
	u32 response_have_data:1;
	/*[17]
	 * 0:Receiving Response without data,
	 * 1:Receiving response with data
	 */
	u32 response_crc7_from_8:1;
	/*[18]
	 * 0:Normal CRC7, Calculating CRC7 will
	 * be from bit0 of all response bits,
	 * 1:Calculating CRC7 will be from
	 * bit8 of all response bits
	 */
	u32 check_busy_on_dat0:1;
	/*[19]
	 * used for R1b response
	 * 0: dont check busy on dat0,
	 * 1:need check
	 */
	u32 cmd_send_data:1;
	/*[20]
	 * 0:This command is not for transmitting data,
	 * 1:This command is for transmitting data
	 */
	u32 use_int_window:1;
	/*[21]
	 * 0:SDIO DAT1 interrupt window disabled, 1:Enabled
	 */
	u32 reserved:2;/*[23:22]*/
	u32 repeat_package_times:8;
	/*[31:24] Total packages to be sent*/
};

struct sdio_config {
	u32 cmd_clk_divide:10;
	/*[9:0] Clock rate setting,
	 * Frequency of SD equals to Fsystem/((cmd_clk_divide+1)*2)
	 */
	u32 cmd_disable_crc:1;
	/*[10]
	 * 0:CRC employed, 1:dont send CRC during command being sent
	 */
	u32 cmd_out_at_posedge:1;
	/*[11]
	 * Command out at negedge normally, 1:at posedge
	 */
	u32 cmd_argument_bits:6;
	/*[17:12] before CRC added, normally 39*/
	u32 do_not_delay_data:1;
	/*[18]
	 *0:Delay one clock normally, 1:dont delay
	 */
	u32 data_latch_at_negedge:1;
	/*[19]
	 * 0:Data caught at posedge normally, 1:negedge
	 */
	u32 bus_width:1;
	/*[20] 0:1bit, 1:4bit*/
	u32 m_endian:2;
	/*[22:21]
	 * Change ENDIAN(bytes order) from DMA data (e.g. dma_din[31:0]).
	 * (00: ENDIAN no change, data output equals to original dma_din[31:0];
	 * 01: data output equals to {dma_din[23:16],dma_din[31:24],
	 * dma_din[7:0],dma_din[15:8]};10: data output equals to
	 * {dma_din[15:0],dma_din[31:16]};11: data output equals to
	 * {dma_din[7:0],dma_din[15:8],dma_din[23:16],dma_din[31:24]})
	 */
	u32 sdio_write_nwr:6;
	/*[28:23]
	 * Number of clock cycles waiting before writing data
	 */
	u32 sdio_write_crc_ok_status:3;
	/*[31:29] if CRC status
	 * equals this register, sdio write can be consider as correct
	 */
};

struct sdio_status_irq {
	u32 sdio_status:4;
	/*[3:0] Read Only
	 * SDIO State Machine Current State, just for debug
	 */
	u32 sdio_cmd_busy:1;
	/*[4] Read Only
	 * SDIO Command Busy, 1:Busy State
	 */
	u32 sdio_response_crc7_ok:1;
	/*[5] Read Only
	 * SDIO Response CRC7 status, 1:OK
	 */
	u32 sdio_data_read_crc16_ok:1;
	/*[6] Read Only
	 * SDIO Data Read CRC16 status, 1:OK
	 */
	u32 sdio_data_write_crc16_ok:1;
	/*[7] Read Only
	 * SDIO Data Write CRC16 status, 1:OK
	 */
	u32 sdio_if_int:1;
	/*[8] write 1 clear this int bit
	 * SDIO DAT1 Interrupt Status
	 */
	u32 sdio_cmd_int:1;
	/*[9] write 1 clear this int bit
	 * Command Done Interrupt Status
	 */
	u32 sdio_soft_int:1;
	/*[10] write 1 clear this int bit
	 * Soft Interrupt Status
	 */
	u32 sdio_set_soft_int:1;
	/*[11] write 1 to this bit
	 * will set Soft Interrupt, read out is m_req_sdio, just for debug
	 */
	u32 sdio_status_info:4;
	/*[15:12]
	 * used for change information between ARC and Amrisc
	 */
	u32 sdio_timing_out_int:1;
	/*[16] write 1 clear this int bit
	 * Timeout Counter Interrupt Status
	 */
	u32 amrisc_timing_out_int_en:1;
	/*[17]
	 * Timeout Counter Interrupt Enable for AMRISC
	 */
	u32 arc_timing_out_int_en:1;
	/*[18]
	 * Timeout Counter Interrupt Enable for ARC/ARM
	 */
	u32 sdio_timing_out_count:13;
	/*[31:19]
	 * Timeout Counter Preload Setting and Present Status
	 */
};

struct sdio_irq_config {
	u32 amrisc_if_int_en:1;
	/*[0]
	 * 1:SDIO DAT1 Interrupt Enable for AMRISC
	 */
	u32 amrisc_cmd_int_en:1;
	/*[1]
	 * 1:Command Done Interrupt Enable for AMRISC
	 */
	u32 amrisc_soft_int_en:1;
	/*[2]
	 * 1:Soft Interrupt Enable for AMRISC
	 */
	u32 arc_if_int_en:1;
	/*[3]
	 * 1:SDIO DAT1 Interrupt Enable for ARM/ARC
	 */
	u32 arc_cmd_int_en:1;
	/*[4]
	 * 1:Command Done Interrupt Enable for ARM/ARC
	 */
	u32 arc_soft_int_en:1;
	/*[5]
	 * 1:Soft Interrupt Enable for ARM/ARC
	 */
	u32 sdio_if_int_config:2;
	/*[7:6]
	 * 00:sdio_if_interrupt window will reset after data Tx/Rx or command
	 * done, others: only after command done
	 */
	u32 sdio_force_data:6;
	/*[13:8]
	 * Write operation: Data forced by software
	 * Read operation: {CLK,CMD,DAT[3:0]}
	 */
	u32 sdio_force_enable:1;
	/*[14] Software Force Enable
	 * This is the software force mode, Software can directly
	 * write to sdio 6 ports (cmd, clk, dat0..3) if force_output_en
	 * is enabled. and hardware outputs will be bypassed.
	 */
	u32 soft_reset:1;
	/*[15]
	 * Write 1 Soft Reset, Don't need to clear it
	 */
	u32 sdio_force_output_en:6;
	/*[21:16]
	 * Force Data Output Enable,{CLK,CMD,DAT[3:0]}
	 */
	u32 disable_mem_halt:2;
	/*[23:22] write and read
	 * 23:Disable write memory halt, 22:Disable read memory halt
	 */
	u32 sdio_force_data_read:6;
	/*[29:24] Read Only
	 * Data read out which have been forced by software
	 */
	u32 force_halt:1;
	/*[30] 1:Force halt SDIO by software
	 * Halt in this sdio host controller means stop to transmit or
	 * receive data from sd card. and then sd card clock will be shutdown.
	 * Software can force to halt anytime, and hardware will automatically
	 * halt the sdio when reading fifo is full or writing fifo is empty
	 */
	u32 halt_hole:1;
	/*[31]
	 * 0: SDIO halt for 8bit mode, 1:SDIO halt for 16bit mode
	 */
};

struct sdio_mult_config {
	u32 sdio_port_sel:2; /*[1:0] 0:sdio_a, 1:sdio_b, 2:sdio_c*/
	u32 ms_enable:1; /*[2] 1:Memory Stick Enable*/
	u32 ms_sclk_always:1; /*[3] 1: Always send ms_sclk*/
	u32 stream_enable:1; /*[4] 1:Stream Enable*/
	u32 stream_8_bits_mode:1; /*[5] Stream 8bits mode*/
	u32 data_catch_level:2; /*[7:6] Level of data catch*/
	u32 write_read_out_index:1;
	/*[8] Write response index Enable
	 * [31:16], [11:10], [7:0] is set only when
	 * bit8 of this register is not set.
	 * And other bits are set only when bit8
	 * of this register is also set.
	 */
	u32 data_catch_readout_en:1; /*[9] Data catch readout Enable*/
	u32 sdio_0_data_on_1:1; /*[10] 1:dat0 is on dat1*/
	u32 sdio_1_data_swap01:1; /*[11] 1:dat1 and dat0 swapped*/
	u32 response_read_index:4; /*[15:12] Index of internal read response*/
	u32 data_catch_finish_point:12;
	/*[27:16] If internal data
	 * catch counter equals this register,
	 *	it indicates data catching is finished
	 */
	u32 reserved:4; /*[31:28]*/
};

struct sdio_extension {
	u32 cmd_argument_ext:16;
	/*[15:0] for future use*/
	u32 data_rw_number:14;
	/*[29:16]
	 * Data Read/Write Number in one packet, include CRC16 if has CRC16
	 */
	u32 data_rw_do_not_have_crc16:1;
	/*[30]
	 * 0:data Read/Write has crc16, 1:without crc16
	 */
	u32 crc_status_4line:1;
	/*[31] 1:4Lines check CRC Status*/
};

struct sdio_reg {
	u32 argument; /*2308*/
	struct cmd_send send; /*2309*/
	struct sdio_config config; /*230a*/
	struct sdio_status_irq status; /*230b*/
	struct sdio_irq_config irqc; /*230c*/
	struct sdio_mult_config mult; /*230d*/
	u32 m_addr; /*230e*/
	struct sdio_extension ext;/*230f*/
};

/*-sdhc-*/

#define SDHC_ARGU				(0x00)
#define SDHC_SEND				(0x04)
#define SDHC_CTRL				(0x08)
#define SDHC_STAT				(0x0C)
#define SDHC_CLKC				(0x10)
#define SDHC_ADDR				(0x14)
#define SDHC_PDMA				(0x18)
#define SDHC_MISC				(0x1C)
#define SDHC_DATA				(0x20)
#define SDHC_ICTL				(0x24)
#define SDHC_ISTA				(0x28)
#define SDHC_SRST				(0x2C)
#define SDHC_ESTA				(0x30)
#define SDHC_ENHC				(0x34)
#define SDHC_CLK2				(0x38)

/* sdio cbus register */
#define CBUS_SDIO_ARGU		(0x2308)
#define CBUS_SDIO_SEND		(0x2309)
#define CBUS_SDIO_CONF		(0x230a)
#define CBUS_SDIO_IRQS		(0x230b)
#define CBUS_SDIO_IRQC		(0x230c)
#define CBUS_SDIO_MULT		(0x230d)
#define CBUS_SDIO_ADDR		(0x230e)
#define CBUS_SDIO_EXT		(0x230f)


/* CBUS reg definition */
#define	ISA_TIMERE			0x2655
#define	HHI_GCLK_MPEG0		0x1050
#define	ASSIST_POR_CONFIG	0x1f55

#define PREG_PAD_GPIO0_EN_N 0x200c
#define PREG_PAD_GPIO0_O	0x200d
#define PREG_PAD_GPIO0_I	0x200e
#define PREG_PAD_GPIO1_EN_N 0x200f
#define PREG_PAD_GPIO1_O	0x2010
#define PREG_PAD_GPIO1_I	0x2011
#define PREG_PAD_GPIO2_EN_N 0x2012
#define PREG_PAD_GPIO2_O	0x2013
#define PREG_PAD_GPIO2_I	0x2014
#define PREG_PAD_GPIO3_EN_N 0x2015
#define PREG_PAD_GPIO3_O	0x2016
#define PREG_PAD_GPIO3_I	0x2017
#define PREG_PAD_GPIO4_EN_N 0x2018
#define PREG_PAD_GPIO4_O	0x2019
#define PREG_PAD_GPIO4_I	0x201a
#define PREG_PAD_GPIO5_EN_N 0x201b
#define PREG_PAD_GPIO5_O	0x201c
#define PREG_PAD_GPIO5_I	0x201d

#define	PERIPHS_PIN_MUX_0	0x202c
#define	PERIPHS_PIN_MUX_1	0x202d
#define	PERIPHS_PIN_MUX_2	0x202e
#define	PERIPHS_PIN_MUX_3	0x202f
#define	PERIPHS_PIN_MUX_4	0x2030
#define	PERIPHS_PIN_MUX_5	0x2031
#define	PERIPHS_PIN_MUX_6	0x2032
#define	PERIPHS_PIN_MUX_7	0x2033
#define	PERIPHS_PIN_MUX_8	0x2034
#define	PERIPHS_PIN_MUX_9	0x2035

/* interrupt definition */
#define	INT_SDIO	(60-32)
#define	INT_SDHC	(110-32)

#define INT_GPIO_0	(96-32)
#define INT_GPIO_1	(97-32)
#define INT_GPIO_2	(98-32)
#define INT_GPIO_3	(99-32)
#define INT_GPIO_4	(100-32)
#define INT_GPIO_5	(101-32)
#define INT_GPIO_6	(102-32)
#define INT_GPIO_7	(103-32)


struct sdhc_send {
	/*[5:0] command index*/
	u32 cmd_index:6;
	/*[6] 0:no resp 1:has resp*/
	u32 cmd_has_resp:1;
	/*[7] 0:no data 1:has data*/
	u32 cmd_has_data:1;
	/*[8] 0:48bit 1:136bit*/
	u32 resp_len:1;
	/*[9] 0:check crc7 1:don't check crc7*/
	u32 resp_no_crc:1;
	/*[10] 0:data rx, 1:data tx*/
	u32 data_dir:1;
	/*[11] 0:rx or tx, 1:data stop,ATTN:will give rx a softreset*/
	u32 data_stop:1;
	/*[12] 0: resp with no busy, 1:R1B*/
	u32 r1b:1;
	/*[15:13] reserved*/
	u32 reserved:3;
	/*[31:16] total package number for writing or reading*/
	u32 total_pack:16;
};

struct sdhc_ctrl {
	/*[1:0] 0:1bit, 1:4bits, 2:8bits, 3:reserved*/
	u32 dat_type:2;
	/*[2] 0:SDR mode, 1:Don't set it*/
	u32 ddr_mode:1;
	/*[3] 0:check sd write crc result, 1:disable tx crc check*/
	u32 tx_crc_nocheck:1;
	/*[12:4] 0:512Bytes, 1:1, 2:2, ..., 511:511Bytes*/
	u32 pack_len:9;
	/*[19:13] cmd or wcrc Receiving Timeout, default 64*/
	u32 rx_timeout:7;
	/*[23:20]Period between response/cmd and next cmd, default 8*/
	u32 rx_period:4;
	/*[26:24] Rx Endian Control*/
	u32 rx_endian:3;
	/*[27]0:Normal mode, 1: support data block gap
	 *(need turn off clock gating)
	 */
	u32 sdio_irq_mode:1;
	/*[28] Dat0 Interrupt selection,0:busy check after response,
	 *1:any rising edge of dat0
	 */
	u32 dat0_irq_sel:1;
	/*[31:29] Tx Endian Control*/
	u32 tx_endian:3;
};

struct sdhc_stat {
	/*[0] 0:Ready for command, 1:busy*/
	u32 cmd_busy:1;
	/*[4:1] DAT[3:0]*/
	u32 dat3_0:4;
	/*[5] CMD*/
	u32 cmd:1;
	/*[12:6] RxFIFO count*/
	u32 rxfifo_cnt:7;
	/*[19:13] TxFIFO count*/
	u32 txfifo_cnt:7;
	/*[23:20] DAT[7:4]*/
	u32 dat7_4:4;
	/*[31:24] Reserved*/
	u32 reserved:8;
};

/*
 * to avoid glitch issue,
 * 1. clk_switch_on better be set after cfg_en be set to 1'b1
 * 2. clk_switch_off shall be set before cfg_en be set to 1'b0
 * 3. rx_clk/sd_clk phase diff please see SD_REGE_CLK2.
 */
struct sdhc_clkc {
	/*[11:0] clk_div for TX_CLK 0: don't set it,1:div2, 2:div3, 3:div4 ...*/
	u32 clk_div:12;
	/*[12] TX_CLK 0:switch off, 1:switch on*/
	u32 tx_clk_on:1;
	/*[13] RX_CLK 0:switch off, 1:switch on*/
	u32 rx_clk_on:1;
	/*[14] SD_CLK 0:switch off, 1:switch on*/
	u32 sd_clk_on:1;
	/*[15] Clock Module Enable, Should set before bit[14:12] switch on,
	 *	and after bit[14:12] switch off
	 */
	u32 mod_clk_on:1;
	/*[17:16] 0:osc, 1:fclk_div4, 2:fclk_div3, 3:fclk_div5*/
	u32 clk_src_sel:2;
	/*[23:18] Reserved*/
	u32 reserved:6;
	/*[24] Clock JIC for clock gating control
	 *1: will turn off clock gating
	 */
	u32 clk_jic:1;
	/*[26:25] 00:Memory Power Up, 11:Memory Power Off*/
	u32 mem_pwr_off:2;
	/*[31:27] Reserved*/
	u32 reserved2:5;
};

/*
 * Note1: dma_urgent is just set when bandwidth is very tight
 * Note2: pio_rdresp need to be combined with REG0_ARGU;
 * For R0, when 0, reading REG0 will get the normal 32bit response;
 * For R2, when 1, reading REG0 will get CID[31:0], when 2, get CID[63:32],
 * and so on; 6 or 7, will get original command argument.
 */
struct sdhc_pdma {
	/*[0] 0:PIO mode, 1:DMA mode*/
	u32 dma_mode:1;
	/*[3:1] 0:[39:8] 1:1st 32bits, 2:2nd ...,6 or 7:command argument*/
	u32 pio_rdresp:3;
	/*[4] 0:not urgent, 1:urgent*/
	u32 dma_urgent:1;
	/*[9:5] Number in one Write request burst(0:1,1:2...)*/
	u32 wr_burst:5;
	/*[14:10] Number in one Read request burst(0:1, 1:2...)*/
	u32 rd_burst:5;
	/*[21:15] RxFIFO threshold, >=rxth, will request write*/
	u32 rxfifo_th:7;
	/*[28:22] TxFIFO threshold, <=txth, will request read*/
	u32 txfifo_th:7;
	/*[30:29] [30]self-clear-flush,[29] mode: 0:hw, 1:sw*/
	u32 rxfifo_manual_flush:2;
	/*[31] self-clear-fill, recommand to write before sd send*/
	u32 txfifo_fill:1;
};

struct sdhc_misc {
	/*[3:0] reserved*/
	u32 reserved:4;
	/*[6:4] WCRC Error Pattern*/
	u32 wcrc_err_patt:3;
	/*[9:7] WCRC OK Pattern*/
	u32 wcrc_ok_patt:3;
	/*[15:10] reserved*/
	u32 reserved1:6;
	/*[21:16] Burst Number*/
	u32 burst_num:6;
	/*[27:22] Thread ID*/
	u32 thread_id:6;
	/*[28] 0:auto stop mode, 1:manual stop mode*/
	u32 manual_stop:1;
	/*[31:29] txstart_thres(if (txfifo_cnt/4)>
	 *	(threshold*2), Tx will start)
	 */
	u32 txstart_thres:3;
};

struct sdhc_ictl {
	/*[0] Response is received OK*/
	u32 resp_ok:1;
	/*[1] Response Timeout Error*/
	u32 resp_timeout:1;
	/*[2] Response CRC Error*/
	u32 resp_err_crc:1;
	/*[3] Response is received OK(always no self reset)*/
	u32 resp_ok_noclear:1;
	/*[4] One Package Data Completed ok*/
	u32 data_1pack_ok:1;
	/*[5] One Package Data Failed (Timeout Error)*/
	u32 data_timeout:1;
	/*[6] One Package Data Failed (CRC Error)*/
	u32 data_err_crc:1;
	/*[7] Data Transfer Completed ok*/
	u32 data_xfer_ok:1;
	/*[8] RxFIFO count > threshold*/
	u32 rx_higher:1;
	/*[9] TxFIFO count < threshold*/
	u32 tx_lower:1;
	/*[10] SDIO DAT1 Interrupt*/
	u32 dat1_irq:1;
	/*[11] DMA Done*/
	u32 dma_done:1;
	/*[12] RxFIFO Full*/
	u32 rxfifo_full:1;
	/*[13] TxFIFO Empty*/
	u32 txfifo_empty:1;
	/*[14] Additional SDIO DAT1 Interrupt*/
	u32 addi_dat1_irq:1;
	/*[15] reserved*/
	u32 reserved:1;
	/*[17:16] sdio dat1 interrupt mask windows
	 *	clear delay control,0:2cycle 1:1cycles
	 */
	u32 dat1_irq_delay:2;
	/*[31:18] reserved*/
	u32 reserved1:14;
};

/*Note1: W1C is write one clear.*/
struct sdhc_ista {
	/*[0] Response is received OK (W1C)*/
	u32 resp_ok:1;
	/*[1] Response is received Failed (Timeout Error) (W1C)*/
	u32 resp_timeout:1;
	/*[2] Response is received Failed (CRC Error) (W1C)*/
	u32 resp_err_crc:1;
	/*[3] Response is Received OK (always no self reset)*/
	u32 resp_ok_noclear:1;
	/*[4] One Package Data Completed ok (W1C)*/
	u32 data_1pack_ok:1;
	/*[5] One Package Data Failed (Timeout Error) (W1C)*/
	u32 data_timeout:1;
	/*[6] One Package Data Failed (CRC Error) (W1C)*/
	u32 data_err_crc:1;
	/*[7] Data Transfer Completed ok (W1C)*/
	u32 data_xfer_ok:1;
	/*[8] RxFIFO count > threshold (W1C)*/
	u32 rx_higher:1;
	/*[9] TxFIFO count < threshold (W1C)*/
	u32 tx_lower:1;
	/*[10] SDIO DAT1 Interrupt (W1C)*/
	u32 dat1_irq:1;
	/*[11] DMA Done (W1C)*/
	u32 dma_done:1;
	/*[12] RxFIFO Full(W1C)*/
	u32 rxfifo_full:1;
	/*[13] TxFIFO Empty(W1C)*/
	u32 txfifo_empty:1;
	/*[14] Additional SDIO DAT1 Interrupt*/
	u32 addi_dat1_irq:1;
	/*[31:13] reserved*/
	u32 reserved:17;
};

/*
 * Note1: Soft reset for DPHY TX/RX needs programmer to set it
 * and then clear it manually.
 */
struct sdhc_srst {
	/*[0] Soft reset for MAIN CTRL(self clear)*/
	u32 main_ctrl:1;
	/*[1] Soft reset for RX FIFO(self clear)*/
	u32 rxfifo:1;
	/*[2] Soft reset for TX FIFO(self clear)*/
	u32 txfifo:1;
	/*[3] Soft reset for DPHY RX*/
	u32 dphy_rx:1;
	/*[4] Soft reset for DPHY TX*/
	u32 dphy_tx:1;
	/*[5] Soft reset for DMA IF(self clear)*/
	u32 dma_if:1;
	/*[31:6] reserved*/
	u32 reserved:26;
};

struct  sdhc_enhc {
	union  {
		struct  {
			/*[0] 0:Wrrsp Check in DMA Rx FSM 1:No Check in FSM*/
			u32 wrrsp_mode:1;
			/*[1] Rx Done without checking if Wrrsp count is 0*/
			u32 chk_wrrsp:1;
			/*[2] Rx Done without checking if DMA is IDLE*/
			u32 chk_dma:1;
			/*[5:3] debug only*/
			u32 debug:3;
			u32 reserved:2;
			/*[15:8] SDIO IRQ Period Setting*/
			u32 sdio_irq_period:8;
			u32 reserved1:2;
			/*[24:18] RXFIFO Full Threshold,default 60*/
			u32 rxfifo_th:7;
			/*[31:25] TXFIFO Empty Threshold,default 0*/
			u32 txfifo_th:7;
		}  meson8m2;
		struct  {
			/*[7:0] Data Rx Timeout Setting*/
			u32 rx_timeout:8;
			/*[15:8] SDIO IRQ Period Setting
			 *(IRQ checking window length)
			 */
			u32 sdio_irq_period:8;
			/*[16] No Read DMA Response Check*/
			u32 dma_rd_resp:1;
			/*[16] No Write DMA Response Check*/
			u32 dma_wr_resp:1;
			/*[24:18] RXFIFO Full Threshold,default 60*/
			u32 rxfifo_th:7;
			/*[31:25] TXFIFO Empty Threshold,default 0*/
			u32 txfifo_th:7;
		}  meson;
	} reg;
};

struct sdhc_clk2 {
	/*[11:0] rx_clk phase diff(default 0:no diff,
	 *1:one input clock cycle ...)
	 */
	u32 rx_clk_phase:12;
	/*[23:12] sd_clk phase diff(default 0:half(180 degree),
	 *1:half+one input clock cycle, 2:half+2 input clock cycles, ...)
	 */
	u32 sd_clk_phase:12;
	/*[31:24] reserved*/
	u32 reserved:8;
};

#define SDHC_CLOCK_SRC_OSC			  0 /* 24MHz */
#define SDHC_CLOCK_SRC_FCLK_DIV4		1
#define SDHC_CLOCK_SRC_FCLK_DIV3		2
#define SDHC_CLOCK_SRC_FCLK_DIV5		3
#define SDHC_ISTA_W1C_ALL			   0x7fff
#define SDHC_SRST_ALL				   0x3f
#define SDHC_ICTL_ALL						0x7fff

struct sd_emmc_regs {
	u32 gclock;	 /* 0x00 */
	u32 gdelay;	 /* 0x04 */
	u32 gadjust;	/* 0x08 */
	u32 reserved_0c;	   /* 0x0c */
	u32 gcalout[4];	/* 0x10~0x1c */
	u32 reserved_20[8];   /* 0x20~0x3c */
	u32 gstart;	 /* 0x40 */
	u32 gcfg;	   /* 0x44 */
	u32 gstatus;	/* 0x48 */
	u32 girq_en;	/* 0x4c */
	u32 gcmd_cfg;   /* 0x50 */
	u32 gcmd_arg;   /* 0x54 */
	u32 gcmd_dat;   /* 0x58 */
	u32 gcmd_rsp0;   /* 0x5c */
	u32 gcmd_rsp1;  /* 0x60 */
	u32 gcmd_rsp2;  /* 0x64 */
	u32 gcmd_rsp3;  /* 0x68 */
	u32 reserved_6c;	   /* 0x6c */
	u32 gcurr_cfg;  /* 0x70 */
	u32 gcurr_arg;  /* 0x74 */
	u32 gcurr_dat;  /* 0x78 */
	u32 gcurr_rsp;  /* 0x7c */
	u32 gnext_cfg;  /* 0x80 */
	u32 gnext_arg;  /* 0x84 */
	u32 gnext_dat;  /* 0x88 */
	u32 gnext_rsp;  /* 0x8c */
	u32 grxd;	   /* 0x90 */
	u32 gtxd;	   /* 0x94 */
	u32 reserved_98[90];   /* 0x98~0x1fc */
	u32 gdesc[128]; /* 0x200 */
	u32 gping[128]; /* 0x400 */
	u32 gpong[128]; /* 0x800 */
};
struct sd_emmc_regs_v3 {
	u32 gclock;	 /* 0x00 */
	u32 gdelay1;	 /* 0x04 */
	u32 gdelay2;/*0x08*/
	u32 gadjust;	/* 0x0c */
	u32 gcalout[4];	/* 0x10~0x1c */
	u32 adj_idx_log;/*20*/
	u32 clktest_log;/*0x24*/
	u32 clktest_out;/*0x28*/
	u32 eyetest_log;/*0x2c*/
	u32 eyetest_out0;/*0x30*/
	u32 eyetest_out1;/*0x34*/
	u32 intf3;/*0x38*/
	u32 reserved3c;/*0x3c*/
	u32 gstart;	 /* 0x40 */
	u32 gcfg;	   /* 0x44 */
	u32 gstatus;	/* 0x48 */
	u32 girq_en;	/* 0x4c */
	u32 gcmd_cfg;   /* 0x50 */
	u32 gcmd_arg;   /* 0x54 */
	u32 gcmd_dat;   /* 0x58 */
	u32 gcmd_rsp0;   /* 0x5c */
	u32 gcmd_rsp1;  /* 0x60 */
	u32 gcmd_rsp2;  /* 0x64 */
	u32 gcmd_rsp3;  /* 0x68 */
	u32 reserved_6c;	   /* 0x6c */
	u32 gcurr_cfg;  /* 0x70 */
	u32 gcurr_arg;  /* 0x74 */
	u32 gcurr_dat;  /* 0x78 */
	u32 gcurr_rsp;  /* 0x7c */
	u32 gnext_cfg;  /* 0x80 */
	u32 gnext_arg;  /* 0x84 */
	u32 gnext_dat;  /* 0x88 */
	u32 gnext_rsp;  /* 0x8c */
	u32 grxd;	   /* 0x90 */
	u32 gtxd;	   /* 0x94 */
	u32 reserved_98[90];   /* 0x98~0x1fc */
	u32 gdesc[128]; /* 0x200 */
	u32 gping[128]; /* 0x400 */
	u32 gpong[128]; /* 0x800 */
};
struct sd_emmc_clock {
	/*[5:0]	 Clock divider.
	 *Frequency = clock source/cfg_div, Maximum divider 63.
	 */
	u32 div:6;
	/*[7:6]	 Clock source, 0: Crystal 24MHz, 1: Fix PLL, 850MHz*/
	u32 src:2;
	/*[9:8]	 Core clock phase. 0: 0 phase,
	 *1: 90 phase, 2: 180 phase, 3: 270 phase.
	 */
	u32 core_phase:2;
	/*[11:10]   TX clock phase. 0: 0 phase,
	 *1: 90 phase, 2: 180 phase, 3: 270 phase.
	 */
	u32 tx_phase:2;
	/*[13:12]   RX clock phase. 0: 0 phase,
	 *1: 90 phase, 2: 180 phase, 3: 270 phase.
	 */
	u32 rx_phase:2;
	u32 reserved14:2;
	/*[19:16]   TX clock delay line. 0: no delay,
	 *n: delay n*200ps. Maximum delay 3ns.
	 */
	u32 tx_delay:4;
	/*[23:20]   RX clock delay line. 0: no delay,
	 *n: delay n*200ps. Maximum delay 3ns.
	 */
	u32 rx_delay:4;
	/*[24]	  1: Keep clock always on.
	 *0: Clock on/off controlled by activities.
	 */
	u32 always_on:1;
	/*[25]	1: enable IRQ sdio when in sleep mode. */
	u32 irq_sdio_sleep:1;
	/*[26]	1: select DS as IRQ source during sleep.. */
	u32 irq_sdio_sleep_ds:1;
	u32 reserved27:5;
};
struct sd_emmc_clock_v3 {
	/*[5:0]	 Clock divider.
	 *Frequency = clock source/cfg_div, Maximum divider 63.
	 */
	u32 div:6;
	/*[7:6]	 Clock source, 0: Crystal 24MHz, 1: Fix PLL, 850MHz*/
	u32 src:2;
	/*[9:8]	 Core clock phase. 0: 0 phase,
	 *1: 90 phase, 2: 180 phase, 3: 270 phase.
	 */
	u32 core_phase:2;
	/*[11:10]   TX clock phase. 0: 0 phase,
	 *1: 90 phase, 2: 180 phase, 3: 270 phase.
	 */
	u32 tx_phase:2;
	/*[13:12]   RX clock phase. 0: 0 phase,
	 *1: 90 phase, 2: 180 phase, 3: 270 phase.
	 */
	u32 rx_phase:2;
	u32 sram_pd:2;
	/*[21:16]   TX clock delay line. 0: no delay,
	 *n: delay n*200ps. Maximum delay 3ns.
	 */
	u32 tx_delay:6;
	/*[27:22]   RX clock delay line. 0: no delay,
	 *n: delay n*200ps. Maximum delay 3ns.
	 */
	u32 rx_delay:6;
	/*[28]	  1: Keep clock always on.
	 *0: Clock on/off controlled by activities.
	 */
	u32 always_on:1;
	/*[29]	1: enable IRQ sdio when in sleep mode. */
	u32 irq_sdio_sleep:1;
	/*[30]	1: select DS as IRQ source during sleep.. */
	u32 irq_sdio_sleep_ds:1;
	u32 reserved31:1;
};
struct sd_emmc_delay {
	u32 dat0:4;		 /*[3:0]	   Data 0 delay line. */
	u32 dat1:4;		 /*[7:4]	   Data 1 delay line. */
	u32 dat2:4;		 /*[11:8]	  Data 2 delay line. */
	u32 dat3:4;		 /*[15:12]	 Data 3 delay line. */
	u32 dat4:4;		 /*[19:16]	 Data 4 delay line. */
	u32 dat5:4;		 /*[23:20]	 Data 5 delay line. */
	u32 dat6:4;		 /*[27:24]	 Data 6 delay line. */
	u32 dat7:4;		 /*[31:28]	 Data 7 delay line. */
};
struct sd_emmc_delay1_v3 {
	u32 dat0:6;		 /*[5:0]	   Data 0 delay line. */
	u32 dat1:6;		 /*[11:6]	   Data 1 delay line. */
	u32 dat2:6;		 /*[17:12]	  Data 2 delay line. */
	u32 dat3:6;		 /*[23:18]	 Data 3 delay line. */
	u32 dat4:6;		 /*[29:24]	 Data 4 delay line. */
	u32 spare:2;		 /*[31:30]	 Data 5 delay line. */
};
struct sd_emmc_delay2_v3 {
	u32 dat5:6;		 /*[5:0]	   Data 0 delay line. */
	u32 dat6:6;		 /*[11:6]	   Data 1 delay line. */
	u32 dat7:6;		 /*[17:12]	  Data 2 delay line. */
	u32 dat8:6;		 /*[23:18]	 Data 3 delay line. */
	u32 dat9:6;		 /*[29:24]	 Data 4 delay line. */
	u32 spare:2;		 /*[31:30]	 Data 5 delay line. */
};
struct sd_emmc_adjust {
	/*[3:0]	   Command delay line. */
	u32 cmd_delay:4;
	/*[7:4]	   DS delay line. */
	u32 ds_delay:4;
	/*[11:8]	  Select one signal to be tested.*/
	u32 cali_sel:4;
	/*[12]		Enable calibration. */
	u32 cali_enable:1;
	/*[13]	   Adjust interface timing
	 *by resampling the input signals.
	 */
	u32 adj_enable:1;
	/*[14]	   1: test the rising edge.
	 *0: test the falling edge.
	 */
	u32 cali_rise:1;
	/*[15]	   1: Sampling the DAT based on DS in HS400 mode.
	 *0: Sampling the DAT based on RXCLK.
	 */
	u32 ds_enable:1;
	/*[21:16]	   Resample the input signals
	 *when clock index==adj_delay.
	 */
	u32 adj_delay:6;
	/*[22]	   1: Use cali_dut first falling edge to adjust
	 *	the timing, set cali_enable to 1 to use this function.
	 *0: no use adj auto.
	 */
	u32 adj_auto:1;
	u32 reserved22:9;
};
struct sd_emmc_adjust_v3 {
	u32 reserved8:8;
	/*[11:8]	  Select one signal to be tested.*/
	u32 cali_sel:4;
	/*[12]		Enable calibration. */
	u32 cali_enable:1;
	/*[13]	   Adjust interface timing
	 *by resampling the input signals.
	 */
	u32 adj_enable:1;
	/*[14]	   1: test the rising edge.
	 *0: test the falling edge.
	 */
	u32 cali_rise:1;
	/*[15]	   1: Sampling the DAT based on DS in HS400 mode.
	 *0: Sampling the DAT based on RXCLK.
	 */
	u32 ds_enable:1;
	/*[21:16]	   Resample the input signals
	 *when clock index==adj_delay.
	 */
	u32 adj_delay:6;
	/*[22]	   1: Use cali_dut first falling edge to adjust
	 *	the timing, set cali_enable to 1 to use this function.
	 *0: no use adj auto.
	 */
	u32 adj_auto:1;
	u32 reserved22:9;
};
struct sd_emmc_calout {
	/*[5:0]	   Calibration reading.
	 *The event happens at this index.
	 */
	u32 cali_idx:6;
	u32 reserved6:1;
	/*[7]		 The reading is valid. */
	u32 cali_vld:1;
	/*[15:8]	  Copied from BASE+0x8
	 *[15:8] include cali_sel, cali_enable, adj_enable, cali_rise.
	 */
	u32 cali_setup:8;
	u32 reserved16:16;
};

struct clktest_log {
	u32 clktest_times:31;
	u32 clktest_done:1;
};

struct clktest_out {
	u32 clktest_out;
};

struct eyetest_log {
	u32 eyetest_times:31;
	u32 eyetest_done:1;
};

struct eyetest_out0 {
	u32 eyetest_out0;
};

struct eyetest_out1 {
	u32 eyetest_out1;
};

struct intf3 {
	u32 clktest_exp:5;  /*[4:0]*/
	u32 clktest_on_m:1; /*[5]*/
	u32 eyetest_exp:5;  /*[10:6]*/
	u32 eyetest_on:1;   /*[11]*/
	u32 ds_sht_m:6;     /*[17:12]*/
	u32 ds_sht_exp:4;   /*[21:18]*/
	u32 sd_intf3:1;     /*[22]*/
};

struct sd_emmc_start {
	/*[0]   1: Read descriptor from internal SRAM,
	 *limited to 32 descriptors.
	 */
	u32 init:1;
	/*[1]   1: Start command chain execution process. 0: Stop */
	u32 busy:1;
	/*[31:2] Descriptor address, the last 2 bits are 0,
	 *4 bytes aligned.
	 */
	u32 addr:30;
};
struct sd_emmc_config {
	/*[1:0]	 0: 1 bit, 1: 4 bits,
	 *2: 8 bits, 3: 2 bits (not supported)
	 */
	u32 bus_width:2;
	/*[2]	   1: DDR mode, 0: SDR mode */
	u32 ddr:1;
	/*[3]	   1: DDR access urgent, 0: DDR access normal. */
	u32 dc_ugt:1;
	/*[7:4]	 Block length 2^cfg_bl_len,
	 *because internal buffer size is limited to 512 bytes,
	 *the cfg_bl_len <=9.
	 */
	u32 bl_len:4;
	/*[11:8]	Wait response till 2^cfg_resp_timeout core clock cycles.
	 *Maximum 32768 core cycles.
	 */
	u32 resp_timeout:4;
	/*[15:12]   Wait response-command,
	 *command-command gap before next command,
	 *2^cfg_rc_cc core clock cycles.
	 */
	u32 rc_cc:4;
	/*[16]	  DDR mode only. The command and TXD start from rising edge.
	 *Set 1 to start from falling edge.
	 */
	u32 out_fall:1;
	/*[17]	  1: Enable SDIO data block gap interrupt period.
	 *0: Disabled.
	 */
	u32 blk_gap_ip:1;
	/*[18]	  Spare,  ??? need check*/
	u32 spare:1;
	/*[19]	  Use this descriptor
	 *even if its owner bit is ???0???бь?ии.
	 */
	u32 ignore_owner:1;
	/*[20]	  Check data strobe in HS400.*/
	u32 chk_ds:1;
	/*[21]	  Hold CMD as output Low, eMMC boot mode.*/
	u32 cmd_low:1;
	/*[22]	  1: stop clock. 0: normal clock.*/
	u32 stop_clk:1;
	/*[23]	  1: when BUS is idle and no descriptor is available,
	 *turn off clock, to save power.
	 */
	u32 auto_clk:1;
	/*[24]	TXD add error test*/
	u32 txd_add_err:1;
	/*[25]	When TXD CRC error, host sends the block again.*/
	u32 txd_retry:1;
	/*[26]	1: Use DS pin as SDIO IRQ input,
	 *0: Use DAT1 pin as SDIO IRQ input..
	 */
	u32 irq_ds:1;
	u32 err_abort:1;
	u32 revd:4;			/*[31:27]   reved*/
};
struct sd_emmc_status {
	/*[7:0]	 RX data CRC error per wire, for multiple block read,
	 *the CRC errors are ORed together.
	 */
	u32 rxd_err:8;
	/*[8]	   TX data CRC error, for multiple block write,
	 *any one of blocks CRC error.
	 */
	u32 txd_err:1;
	/*[9]	   SD/eMMC controller doesn???и║?ииt own descriptor.
	 *The owner bit is set cfg_ignore_owner to ignore this error.
	 */
	u32 desc_err:1;
	/*[10]	  Response CRC error.*/
	u32 resp_err:1;
	/*[11]	  No response received before time limit.
	 *The timeout limit is set by cfg_resp_timeout.
	 */
	u32 resp_timeout:1;
	/*[12]	  Descriptor execution time over time limit.
	 *The timeout limit is set by descriptor itself.
	 */
	u32 desc_timeout:1;
	/*[13]	  End of Chain IRQ, Normal IRQ. */
	u32 end_of_chain:1;
	/*[14]	  This descriptor requests an IRQ, Normal IRQ,
	 *the descriptor chain execution keeps going on.
	 */
	u32 desc_irq:1;
	/*[15]	  SDIO device uses DAT[1] to request IRQ. */
	u32 irq_sdio:1;
	/*[23:16]   Input data signals. */
	u32 dat_i:8;
	/*[24]	  nput response signal. */
	u32 cmd_i:1;
	/*[25]	  Input data strobe. */
	u32 ds:1;
	/*[29:26]   BUS fsm */
	u32 bus_fsm:4;
	/*[30]	  Descriptor write back process is done
	 *and it is ready for CPU to read.
	 */
	u32 desc_wr_rdy:1;
	/*[31]	  Core is busy,desc_busy or sd_emmc_irq
	 *  or bus_fsm is not idle.
	 */
	u32 core_wr_rdy:1;
};
struct sd_emmc_irq_en {
	/*[7:0]	 RX data CRC error per wire.*/
	u32 rxd_err:8;
	/*[8]	   TX data CRC error. */
	u32 txd_err:1;
	/*[9]	   SD/eMMC controller doesn???и║?ииt own descriptor. */
	u32 desc_err:1;
	/*[10]	  Response CRC error.*/
	u32 resp_err:1;
	/*[11]	  No response received before time limit. */
	u32 resp_timeout:1;
	/*[12]	  Descriptor execution time over time limit. */
	u32 desc_timeout:1;
	/*[13]	  End of Chain IRQ. */
	u32 end_of_chain:1;
	/*[14]	  This descriptor requests an IRQ. */
	u32 desc_irq:1;
	/*[15]	  Enable sdio interrupt. */
	u32 irq_sdio:1;
	/*[31:16]   reved*/
	u32 revd:16;
};
struct sd_emmc_data_info {
	/*[9:0]	 Rxd words received from BUS. Txd words received from DDR.*/
	u32 cnt:10;
	/*[24:16]   Rxd Blocks received from BUS.
	 *Txd blocks received from DDR.
	 */
	u32 blk:9;
	/*[31:17]   Reved. */
	u32 revd:30;
};
struct sd_emmc_card_info {
	/*[9:0]	 Txd BUS cycle counter. */
	u32 txd_cnt:10;
	/*[24:16]   Txd BUS block counter.*/
	u32 txd_blk:9;
	/*[31:17]   Reved. */
	u32 revd:30;
};
struct cmd_cfg {
	u32 length:9;
	u32 block_mode:1;
	u32 r1b:1;
	u32 end_of_chain:1;
	u32 timeout:4;
	u32 no_resp:1;
	u32 no_cmd:1;
	u32 data_io:1;
	u32 data_wr:1;
	u32 resp_nocrc:1;
	u32 resp_128:1;
	u32 resp_num:1;
	u32 data_num:1;
	u32 cmd_index:6;
	u32 error:1;
	u32 owner:1;
};
struct sd_emmc_desc_info {
	u32 cmd_info;
	u32 cmd_arg;
	u32 data_addr;
	u32 resp_addr;
};
#define HHI_NAND_CLK_CNTL					0x97
#define SD_EMMC_MAX_DESC_MUN		512
#define SD_EMMC_MAX_DESC_MUN_PIO		36
#define SD_EMMC_REQ_DESC_MUN		4
#define SD_EMMC_CLOCK_SRC_OSC		0 /* 24MHz */
#define SD_EMMC_CLOCK_SRC_FCLK_DIV2	1 /* 1GHz */
#define SD_EMMC_CLOCK_SRC_400MHZ	4
#define SD_EMMC_CLOCK_SRC_MPLL		2 /* MPLL */
#define SD_EMMC_CLOCK_SRC_DIFF_PLL	3
#define SD_EMMC_IRQ_ALL			0x3fff
#define SD_EMMC_RESP_SRAM_OFF		0
/*#define SD_EMMC_DESC_SET_REG*/

#define SD_EMMC_DESC_REG_CONF		0x4
#define SD_EMMC_DESC_REG_IRQC		0xC
#define SD_EMMC_DESC_RESP_STAT		0xfff80000
#define SD_EMMC_IRQ_EN_ALL_INIT
#define SD_EMMC_REQ_DMA_SGMAP
/* #define SD_EMMC_CLK_CTRL*/
/* #define SD_EMMC_DATA_TASKLET */
#define STAT_POLL_TIMEOUT			0xfffff
#define STAT_POLL_TIMEOUT			0xfffff

#define MMC_RSP_136_NUM				4
#define MMC_MAX_DEVICE				3
#define MMC_TIMEOUT				5000

/* #define pr_info(a...) */
#define DBG_LINE_INFO() \
{ \
	pr_info("[%s] : %s\n", __func__, __FILE__); \
}
/* #define DBG_LINE_INFO() */
/* #define dev_err(a,s) pr_info(KERN_INFO s); */
/* fixme, those code should not be marco as vairous on chips */
/* reg0 for BOOT */
#define BOOT_POLL_UP	(0x3A << 2)
#define BOOT_POLL_UP_EN (0x48 << 2)
/* reg1 for GPIOC(card) */
#define CARD_POLL_UP	(0x3B << 2)
#define CARD_POLL_UP_EN (0x49 << 2)

/* pinmux for sdcards, gpioC */
#define PIN_MUX_REG6	(0xb6 << 2)
#define PIN_MUX_REG9	(0xb9 << 2)

#define AML_MMC_DISABLED_TIMEOUT	100
#define AML_MMC_SLEEP_TIMEOUT		1000
#define AML_MMC_OFF_TIMEOUT		8000

#define SD_EMMC_BOUNCE_REQ_SIZE		(512*1024)
#define SDHC_BOUNCE_REQ_SIZE		(512*1024)
#define SDIO_BOUNCE_REQ_SIZE		(128*1024)
#define MMC_TIMEOUT_MS			(20)

#define MESON_SDIO_PORT_A 0
#define MESON_SDIO_PORT_B 1
#define MESON_SDIO_PORT_C 2
#define MESON_SDIO_PORT_XC_A 3
#define MESON_SDIO_PORT_XC_B 4
#define MESON_SDIO_PORT_XC_C 5

void aml_sdhc_request(struct mmc_host *mmc, struct mmc_request *mrq);
int aml_sdhc_get_cd(struct mmc_host *mmc);
extern void amlsd_init_debugfs(struct mmc_host *host);

extern struct mmc_host *sdio_host;

#define	 SPI_BOOT_FLAG			0
#define	 NAND_BOOT_FLAG			1
#define	 EMMC_BOOT_FLAG			2
#define	 CARD_BOOT_FLAG			3
#define	 SPI_NAND_FLAG			4
#define	 SPI_EMMC_FLAG			5

#define R_BOOT_DEVICE_FLAG  (aml_read_cbus(ASSIST_POR_CONFIG))


#define POR_BOOT_VALUE ((((R_BOOT_DEVICE_FLAG>>9)&1)<<2)|\
		((R_BOOT_DEVICE_FLAG>>6)&3)) /* {poc[9],poc[7:6]} */

#define POR_NAND_BOOT() ((POR_BOOT_VALUE == 7) \
		|| (POR_BOOT_VALUE == 6))
#define POR_SPI_BOOT() ((POR_BOOT_VALUE == 5) || (POR_BOOT_VALUE == 4))
/* #define POR_EMMC_BOOT() (POR_BOOT_VALUE == 3) */
#define POR_EMMC_BOOT()	(POR_BOOT_VALUE == 3)

#define POR_CARD_BOOT() (POR_BOOT_VALUE == 0)

/* for external codec status, if using external codec,
 *	jtag should not be set.
 */
extern int ext_codec;

#ifndef CONFIG_MESON_TRUSTZONE
/* P_AO_SECURE_REG1 is "Secure Register 1" in <M8-Secure-AHB-Registers.doc> */
#define aml_jtag_gpioao()
/*do{\
 *	aml_clr_reg32_mask(P_AO_SECURE_REG1, ((1<<5) | (1<<9))); \
 *	if(!ext_codec)\
 *	aml_set_reg32_mask(P_AO_SECURE_REG1, ((1<<8) | (1<<1))); \
 *}while(0)
 */

#define aml_jtag_sd()
/*do{\
 *	aml_clr_reg32_mask(P_AO_SECURE_REG1, ((1<<8) | (1<<1))); \
 *	aml_set_reg32_mask(P_AO_SECURE_REG1, ((1<<5) | (1<<9))); \
 *}while(0)
 */
#else
/* Secure REG can only be accessed in Secure World if TrustZone enabled.*/
#include <mach/meson-secure.h>
#define aml_jtag_gpioao() \
{ \
	meson_secure_reg_write(P_AO_SECURE_REG1, \
			meson_secure_reg_read(P_AO_SECURE_REG1) \
			& (~((1<<5) | (1<<9)))); \
}

#define aml_jtag_sd() do {\
	meson_secure_reg_write(P_AO_SECURE_REG1,\
			meson_secure_reg_read(P_AO_SECURE_REG1)\
			& (~(1<<8) | (1<<1))); \
	meson_secure_reg_write(P_AO_SECURE_REG1,\
			meson_secure_reg_read(P_AO_SECURE_REG1)\
			| ((1<<5) | (1<<9))); \
} while (0)
#endif /* CONFIG_MESON_TRUSTZONE */

#define aml_uart_pinctrl() do {\
	\
} while (0)

#endif

