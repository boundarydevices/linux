/*
 * drivers/amlogic/mtd/aml_mtd.h
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

#ifndef AMLMTD_H_INCLUDED
#define AMLMTD_H_INCLUDED

#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <linux/list.h>
#include <linux/sizes.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/version.h>

#include <linux/amlogic/cpu_version.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand_ecc.h>
/*#include <nand.h>*/
#include "aml_hwctrl.h"
/*#include "partition_table.h"*/

#define CONFIG_MTD_PARTITIONS 1
#define NAND_MAX_DEVICE	4
extern struct mtd_info *nand_info[NAND_MAX_DEVICE];
extern unsigned char pagelist_hynix256[128];
#define CONFIG_ENV_SIZE  (64*1024U)

/*
 * open this macro, the env will store in
 * normal partition: "environment". and the
 * env region in reserve zone will be hidden
 *
 * please remember add a partition named
 * "environment" in dts file.
 * #define CONFIG_MTD_ENV_IN_NAND
 */

/*
 ** Max page list cnt for usrdef mode
 */
#define NAND_PAGELIST_CNT 16

/*
 ** nand read retry info, max equals to Zero, that means no need retry.
 */
struct nand_retry_t {
	unsigned int id;
	unsigned int max;
	unsigned int no_rb;
};

struct _nand_cmd {
	unsigned char type;
	unsigned char val;
};

/*
 ** must same with asm/arch/nand.h
 *
 *ext bits:
 *	bit 26: pagelist enable flag,
 *	bit 24: a2 cmd enable flag,
 *	bit 23: no_rb,
 *	bit 22: large.  large for what?
 *	bit 19: randomizer mode.
 *	bit 14-16: ecc mode
 *	bit 13: short mode
 *	bit 6-12:short page size
 *	bit 0-5: ecc pages.
 */
struct nand_setup {
	union {
		uint32_t d32;
		struct {
			unsigned cmd:22;
			unsigned large_page:1;
			unsigned no_rb:1;
			unsigned a2:1;
			unsigned reserved25:1;
			unsigned page_list:1;
			unsigned sync_mode:2;
			unsigned size:2;
			unsigned active:1;
		} b;
	} cfg;
	uint16_t id;
	uint16_t max;
};

struct _ext_info {
	uint32_t read_info;
	uint32_t new_type;
	uint32_t page_per_blk;
	uint32_t xlc;
	uint32_t ce_mask;
	/* copact mode: boot means whole uboot
	 * it's easy to understood that copies of
	 * bl2 and fip are the same.
	 * discrete mode, boot means the fip only
	 */
	uint32_t boot_num;
	uint32_t each_boot_pages;
	/* for comptible reason */
	uint32_t bbt_occupy_pages;
	uint32_t bbt_start_block;
};
#define NAND_FIPMODE_COMPACT    (0)
#define NAND_FIPMODE_DISCRETE   (1)

/* if you don't need skip the bad blocks when add
 * partitions, please enable this macro.
 * #define CONFIG_NOT_SKIP_BAD_BLOCK
 */

struct _fip_info {
	/* version */
	uint16_t version;
	/* compact or discrete */
	uint16_t mode;
	/* fip start, pages */
	uint32_t fip_start;
};

/*max size is 384 bytes*/
struct _nand_page0 {
	struct nand_setup nand_setup;
	unsigned char page_list[16];
	struct _nand_cmd retry_usr[32];
	struct _ext_info ext_info;
	/* added for slc */
	struct _fip_info fip_info;
};


union nand_core_clk {
	/*raw register data */
	uint32_t d32;
	/*register bits */
	struct {
		unsigned clk_div:7;
		unsigned reserved0:1;
		unsigned clk_en:1;
		unsigned clk_sel:3;
		unsigned not_used:20;
	} b;
};


/***************ERROR CODING*******************/
#define NAND_CHIP_ID_ERR            1
#define NAND_SHIP_BAD_BLOCK_ERR     2
#define NAND_CHIP_REVB_HY_ERR       3

/*fixme container_of this function is in common.h, but if include it, it
 *will case serial compile error
 */
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */

/** Register defination **/
#define NAND_BOOT_NAME	"bootloader"
#define NAND_NORMAL_NAME "nandnormal"
#define NAND_RESERVED_NAME "nandreserved"
/* Max total is 1024 as romboot says so... */
#define BOOT_TOTAL_PAGES	(1024)
/* This depends on uboot size */
#define BOOT_PAGES_PER_COPY (1024)
#define BOOT_COPY_NUM (BOOT_TOTAL_PAGES / BOOT_PAGES_PER_COPY)
/*it also means normal device start addrress */
#define RESERVED_BLOCK_NUM 48
/* reserved region info */
#define NAND_GAP_BLOCK_NUM 4
#define NAND_BBT_BLOCK_NUM 4
#define NAND_ENV_BLOCK_NUM 8
#define NAND_KEY_BLOCK_NUM 8
#define NAND_DTB_BLOCK_NUM 4

#define AML_CHIP_NONE_RB	4
#define AML_INTERLEAVING_MODE	8

#define AML_NAND_CE0	0xe
#define AML_NAND_CE1	0xd
#define AML_NAND_CE2	0xb
#define AML_NAND_CE3	0x7

#define AML_BADBLK_POS	0


#define NAND_ECC_OPTIONS_MASK	0x0000000f
#define NAND_PLANE_OPTIONS_MASK	0x000000f0
#define NAND_TIMING_OPTIONS_MASK	0x00000f00
#define NAND_BUSW_OPTIONS_MASK		0x0000f000
#define NAND_INTERLEAVING_OPTIONS_MASK	0x000f0000

#define NAND_TWO_PLANE_MODE	0x00000010
#define NAND_TIMING_MODE0	0x00000000
#define NAND_TIMING_MODE1	0x00000100
#define NAND_TIMING_MODE2	0x00000200
#define NAND_TIMING_MODE3	0x00000300
#define NAND_TIMING_MODE4	0x00000400
#define NAND_TIMING_MODE5	0x00000500
#define NAND_INTERLEAVING_MODE	0x00010000

#define DEFAULT_T_REA	40
#define DEFAULT_T_RHOH	0
#define NAND_DEFAULT_OPTIONS	(NAND_TIMING_MODE5 | NAND_ECC_BCH8_MODE)

#define AML_NAND_BUSY_TIMEOUT	0x40000
#define AML_DMA_BUSY_TIMEOUT	0x100000
#define MAX_ID_LEN	8

#define NAND_CMD_PLANE2_READ_START	0x06
#define NAND_CMD_TWOPLANE_PREVIOS_READ	0x60
#define NAND_CMD_TWOPLANE_READ1	0x5a
#define NAND_CMD_TWOPLANE_READ2	0xa5
#define NAND_CMD_TWOPLANE_WRITE2_MICRO	0x80
#define NAND_CMD_TWOPLANE_WRITE2	0x81
#define NAND_CMD_DUMMY_PROGRAM	0x11
#define NAND_CMD_ERASE1_END	0xd1
#define NAND_CMD_MULTI_CHIP_STATUS	0x78
/*#define NAND_CMD_SET_FEATURES	0xEF*/
/*#define NAND_CMD_GET_FEATURES	0xEE*/
#define ONFI_TIMING_ADDR	0x01


#define NAND_STATUS_READY_MULTI	0x20

#define NAND_BLOCK_GOOD	0
#define NAND_BLOCK_BAD	1
#define NAND_FACTORY_BAD	2
#define BAD_BLK_LEVEL	2
#define	FACTORY_BAD_BLOCK_ERROR	159
#define MINI_PART_SIZE	0x100000
#define NAND_MINI_PART_NUM	4
#define MAX_BAD_BLK_NUM	2000
#define MAX_MTD_PART_NUM	16
#define MAX_MTD_PART_NAME_LEN	24

#define BBT_NAND_MAGIC	"nbbt"
#define ENV_NAND_MAGIC	"nenv"
#define KEY_NAND_MAGIC	"nkey"
#define SEC_NAND_MAGIC	"nsec"
#define DTB_NAND_MAGIC  "ndtb"
#define NAND_SYS_PART_SIZE	0x8000000

struct nand_menson_key {
	u32 crc;
	u8 data[252];
};

struct aml_nand_flash_dev {
	char *name;
	u8 id[MAX_ID_LEN];
	unsigned int pagesize;
	unsigned int chipsize;
	unsigned int erasesize;
	unsigned int oobsize;
	unsigned int internal_chipnr;
	unsigned int T_REA;
	unsigned int T_RHOH;
	u8 onfi_mode;
	/* store new type directly */
	unsigned char new_type;
	unsigned int options;
};

struct aml_nand_part_info {
	char mtd_part_magic[4];
	char mtd_part_name[MAX_MTD_PART_NAME_LEN];
	uint64_t size;
	uint64_t offset;
	u_int32_t mask_flags;
};

struct aml_nand_bbt_info {
	int16_t nand_bbt[MAX_BAD_BLK_NUM];
};

struct aml_nandrsv_info_t {
	struct mtd_info *mtd;
	struct valid_node_t *valid_node;
	struct free_node_t *free_node;
	unsigned int start_block;
	unsigned int end_block;
	unsigned int size;
	char name[8];
	u_char valid;
	u_char init;
	u_char part_num_before_sys;
};

/*define abnormal state for reserved area*/
#define POWER_ABNORMAL_FLAG	0x01
#define ECC_ABNORMAL_FLAG	0x02
struct valid_node_t {
	int16_t ec;
	int16_t	phy_blk_addr;
	int16_t	phy_page_addr;
	int timestamp;
	int16_t status;
};

struct free_node_t {
	unsigned int index;
	int16_t ec;
	int16_t	phy_blk_addr;
	int dirty_flag;
	struct free_node_t *next;
};

struct oobinfo_t {
	char name[4];
	int16_t ec;
	unsigned timestamp:15;
	unsigned status_page:1;
};

struct aml_nand_bch_desc {
	char *name;
	unsigned int bch_mode;
	unsigned int bch_unit_size;
	unsigned int bch_bytes;
	unsigned int user_byte_mode;
};

#define RETRY_NAND_MAGIC	"refv"
#define RETRY_NAND_BLK_NUM	2
#define RETRY_NAND_COPY_NUM	4

#define	READ_RETRY_REG_NUM	8
#define	READ_RETRY_CNT		30
#define	HYNIX_RETRY_CNT		20


#define	ENHANCE_SLC_REG_NUM	5

#define	READ_RETRY_ZERO	((char)-1)
#define	NAND_CMD_HYNIX_GET_VALUE	0x37
#define	NAND_CMD_HYNIX_SET_VALUE_START	0x36
#define	NAND_CMD_HYNIX_SET_VALUE_END	0x16

#define	NAND_CMD_TOSHIBA_PRE_CON1	0x5c
#define	NAND_CMD_TOSHIBA_PRE_CON2	0xc5
#define	NAND_CMD_TOSHIBA_SET_VALUE	0x55
#define	NAND_CMD_TOSHIBA_BEF_COMMAND1	0x26
#define	NAND_CMD_TOSHIBA_BEF_COMMAND2	0x5d
#define NAND_CMD_SAMSUNG_SET_VALUE	0XA1
#define NAND_CMD_MICRON_SET_VALUE	0XEF

#define	HYNIX_26NM_8GB	1
#define	HYNIX_26NM_4GB	2
#define	HYNIX_20NM_8GB	3
#define	HYNIX_20NM_4GB	4
#define	HYNIX_20NM_LGA_8GB	5
#define	HYNIX_1YNM_8GB	6
#define	TOSHIBA_24NM	20
#define	TOSHIBA_A19NM	21
#define	SUMSUNG_2XNM	30
#define	MICRON_20NM	40
#define	SANDISK_19NM	50
#define	SANDISK_24NM	51
#define	SANDISK_A19NM	52
#define	SANDISK_A19NM_4G	53
#define	INTEL_20NM	60

#define	DYNAMIC_REG_NUM	3
#define	DYNAMIC_REG_INIT_NUM	9
#define	DYNAMIC_READ_CNT_LOWER	15
#define	DYNAMIC_READ_CNT_UPPER	19
#define	DYNAMIC_CNT_LOWER	16
#define	DYNAMIC_CNT_UPPER	20

#define	DYNAMIC_READ_CASE_NUM	20

#define	NAND_CMD_SANDISK_INIT_ONE	0x3B
#define	NAND_CMD_SANDISK_INIT_TWO	0xB9

#define NAND_CMD_SANDISK_DSP_OFF	0x25
#define	NAND_CMD_SANDISK_DSP_ON	0x26
#define	NAND_CMD_SANDISK_RETRY_STA	0x5D
#define	NAND_CMD_SANDISK_LOAD_VALUE_ONE	0x53
#define	NAND_CMD_SANDISK_LOAD_VALUE_TWO	0x54

#define	NAND_CMD_SANDISK_DYNAMIC_ENABLE	0xB6
#define	NAND_CMD_SANDISK_DYNAMIC_DISABLE	0xD6
#define	NAND_CMD_SANDISK_SLC	0xA2
#define	NAND_CMD_SANDISK_SET_VALUE	0XEF
#define	NAND_CMD_SANDISK_GET_VALUE	0XEE
#define	NAND_CMD_SANDISK_SET_OUTPUT_DRV	0x10
#define	NAND_CMD_SANDISK_SET_VENDOR_SPC	0x80

#define	NAND_CMD_MICRON_SET_TOGGLE_SPC	0x01

#define NAND_MINIKEY_PART_SIZE	0x800000
#define NAND_MINIKEY_PART_NUM	4
#define NAND_MINIKEY_PART_BLOCKNUM	4
#define CONFIG_KEYSIZE	(0x4000*4)
#define ENV_KEY_MAGIC	"keyx"

#define NAND_MINI_PART_BLOCKNUM	2

struct aml_nand_read_retry {
	u8 flag;
	u8 reg_cnt;
	u8 retry_cnt;
	u8 default_flag;
	u8 cur_cnt[MAX_CHIP_NUM];
	u8 reg_addr[READ_RETRY_REG_NUM];
	u8 reg_default_value[MAX_CHIP_NUM][READ_RETRY_REG_NUM];
	char reg_offset_value[MAX_CHIP_NUM][READ_RETRY_CNT][READ_RETRY_REG_NUM];
	void (*get_default_value)(struct mtd_info *mtd);
	void (*set_default_value)(struct mtd_info *mtd);
	void (*save_default_value)(struct mtd_info *mtd);
	void (*read_retry_handle)(struct mtd_info *mtd, int chipnr);
	void (*read_retry_exit)(struct mtd_info *mtd, int chipnr);
};

struct aml_nand_slc_program {
	u8 flag;
	u8 reg_cnt;
	u8 reg_addr[ENHANCE_SLC_REG_NUM];
	u8 reg_default_value[MAX_CHIP_NUM][ENHANCE_SLC_REG_NUM];
	char reg_offset_value[ENHANCE_SLC_REG_NUM];
	void (*get_default_value)(struct mtd_info *mtd);
	void (*exit_enslc_mode)(struct mtd_info *mtd);
	void (*enter_enslc_mode)(struct mtd_info *mtd);
};

/* this for sandisk dynamic read */
struct aml_nand_dynamic_read {
	u8 slc_flag;
	u8 dynamic_read_flag;
	u8 read_case_num_max_lower_page;
	u8 read_case_num_max_upper_page;
	u8 cur_case_num_lower_page[MAX_CHIP_NUM];
	u8 cur_case_num_upper_page[MAX_CHIP_NUM];
	u8 reg_addr_init[DYNAMIC_REG_INIT_NUM];
	u8 reg_addr_lower_page[DYNAMIC_REG_NUM];
	u8 reg_addr_upper_page[DYNAMIC_REG_NUM];
	char reg_offset_value_lower_page[DYNAMIC_CNT_LOWER][DYNAMIC_REG_NUM];
	char reg_offset_value_upper_page[DYNAMIC_CNT_UPPER][DYNAMIC_REG_NUM];
	void (*dynamic_read_init)(struct mtd_info *mtd);
	void (*dynamic_read_handle)(struct mtd_info *mtd, int page, int chipnr);
	void (*dynamic_read_exit)(struct mtd_info *mtd, int chipnr);
	void (*exit_slc_mode)(struct mtd_info *mtd);
	void (*enter_slc_mode)(struct mtd_info *mtd);
};

struct new_tech_nand_t {
	u8 type;
	struct aml_nand_slc_program slc_program_info;
	struct aml_nand_read_retry read_rety_info;
	struct aml_nand_dynamic_read dynamic_read_info;
	void (*new_nand_param_init)(struct mtd_info *mtd,
		struct aml_nand_flash_dev *type);
};

struct aml_nand_chip {
	struct mtd_info *mtd;
	struct nand_chip chip;
	struct hw_controller *controller;

	/* mtd info */
	u8 mfr_type;
	unsigned int onfi_mode;
	unsigned int T_REA;
	unsigned int T_RHOH;
	unsigned int options;
	unsigned int page_size;
	unsigned int block_size;
	unsigned int oob_size;
	unsigned int virtual_page_size;
	unsigned int virtual_block_size;
	u8 plane_num;
	u8 internal_chipnr;
	unsigned int internal_page_nums;

	unsigned int internal_chip_shift;
	unsigned int ran_mode;
	unsigned int rbpin_mode;
	unsigned int rbpin_detect;
	unsigned int short_pgsz;
	/* bch for infopage on short mode */
	unsigned int bch_info;

	unsigned int bch_mode;
	u8 user_byte_mode;
	u8 ops_mode;
	u8 cached_prog_status;
	u8 max_bch_mode;
	unsigned int valid_chip[MAX_CHIP_NUM];
	unsigned int page_addr;
	unsigned char *aml_nand_data_buf;
	unsigned int *user_info_buf;
	int8_t *block_status;
	unsigned int toggle_mode;
	u8 ecc_cnt_limit;
	u8 ecc_cnt_cur;
	u8 ecc_max;
	unsigned int zero_cnt;
	unsigned int oob_fill_cnt;
	unsigned int boot_oob_fill_cnt;
	/*add property field for key private data*/
	int dtbsize;
	int keysize;
	int boot_copy_num; /*tell how many bootloader copies*/
	unsigned int  bl_mode;
	unsigned int fip_copies;
	unsigned int fip_size;

	u8 key_protect;
	unsigned char *rsv_data_buf;
	unsigned long long freeNodeBitmask;
	struct free_node_t *free_node[RESERVED_BLOCK_NUM];
	struct aml_nand_bbt_info *nand_bbt_info;
	struct aml_nandrsv_info_t *aml_nandbbt_info;
	struct aml_nandrsv_info_t *aml_nandenv_info;
	struct aml_nandrsv_info_t *aml_nandkey_info;
	struct aml_nandrsv_info_t *aml_nanddtb_info;
	struct aml_nand_bch_desc *bch_desc;

	/*golbal variable for retry support*/
	u8 support_new_nand;

	struct new_tech_nand_t  new_nand_info;
	/* platform info */
	struct aml_nand_platform *platform;
#ifndef AML_NAND_UBOOT
	dma_addr_t data_dma_addr;
	dma_addr_t info_dma_addr;
#endif
	/* device info */
	struct device *device;

	unsigned int max_ecc;
	struct ecc_desc_s *ecc;
	/* unsigned onfi_mode;*/
	unsigned int err_sts;
	/* plateform operation function*/
	void (*aml_nand_hw_init)(struct aml_nand_chip *aml_chip);
	void (*aml_nand_adjust_timing)(struct aml_nand_chip *aml_chip);
	int (*aml_nand_options_confirm)(struct aml_nand_chip *aml_chip);
	void (*aml_nand_cmd_ctrl)(struct aml_nand_chip *aml_chip,
		int cmd,  unsigned int ctrl);
	void (*aml_nand_select_chip)(struct aml_nand_chip *aml_chip,
		int chipnr);
	void (*aml_nand_write_byte)(struct aml_nand_chip *aml_chip,
		uint8_t data);
	void (*aml_nand_get_user_byte)(struct aml_nand_chip *aml_chip,
		unsigned char *oob_buf, int byte_num);
	void (*aml_nand_set_user_byte)(struct aml_nand_chip *aml_chip,
		unsigned char *oob_buf, int byte_num);
	void (*aml_nand_command)(struct aml_nand_chip *aml_chip,
		unsigned int command, int column, int page_addr, int chipnr);
	int (*aml_nand_wait_devready)(struct aml_nand_chip *aml_chip,
		int chipnr);
	int (*aml_nand_dma_read)(struct aml_nand_chip *aml_chip,
		unsigned char *buf, int len, unsigned int bch_mode);
	int (*aml_nand_dma_write)(struct aml_nand_chip *aml_chip,
		unsigned char *buf, int len, unsigned int bch_mode);
	int (*aml_nand_hwecc_correct)(struct aml_nand_chip *aml_chip,
		unsigned char *buf, unsigned int size, unsigned char *oob_buf);
	int (*aml_nand_block_bad_scrub)(struct mtd_info *mtd);
};

struct aml_nand_device;

struct aml_nand_platform {
	struct aml_nand_flash_dev *nand_flash_dev;
	char *name;
	unsigned int chip_enable_pad;
	unsigned int ready_busy_pad;

	/* DMA RD/WR delay loop  timing */
	unsigned int T_REA;	/* for dma  waiting delay */
	/* not equal of (nandchip->delay, which is for dev ready func)*/
	unsigned int T_RHOH;
	unsigned int ran_mode;	/*def close, for all part*/
	unsigned int rbpin_mode;	/*may get from romboot*/
	unsigned int rbpin_detect;
	unsigned int short_pgsz;	/*zero means no short*/
	struct aml_nand_chip *aml_chip;
	/* back pointer to the device*/
	struct aml_nand_device *aml_nand_device;
	struct platform_nand_data platform_nand_data;
};

struct aml_nand_device {
	struct aml_nand_platform *aml_nand_platform;
	u8 dev_num;
#ifndef AML_NAND_UBOOT
	struct notifier_block nb;
	u32 bl_mode;
	u32 fip_copies;
	u32 fip_size;
	/* for mapping regsters */
	u32 nand_clk_ctrl;
#endif
};

static inline struct aml_nand_chip *mtd_to_nand_chip(struct mtd_info *mtd)
{
	//struct nand_chip *chip = mtd->priv;
	struct nand_chip *chip = mtd_to_nand(mtd);

	return container_of(chip, struct aml_nand_chip, chip);
}


#ifdef CONFIG_PARAMETER_PAGE
struct parameter_page {
	/*0~31 byte: Revision information and features block*/
	unsigned char signature[4];
	unsigned short ver;
	unsigned short feature;
	unsigned short opt_commd;
	unsigned short reserve0;
	unsigned short ex_para_page_len;
	unsigned char num_para_page;
	unsigned char reserve1[17];
	/*32~79 byte: Manufacturer information block*/
	unsigned char dev_manu[12];
	unsigned char dev_model[20];
	unsigned char JEDEC_manu_ID;
	unsigned short date_code;
	unsigned char reserve2[13];
	/*80~127 byte: Memory organization block*/
	unsigned int data_bytes_perpage;
	unsigned short spare_bytes_perpage;
	unsigned int data_bytes_perpartial;
	unsigned short spare_bytes_perpartial;
	unsigned int pages_perblk;
	unsigned int blks_perLUN;
	unsigned char num_LUN;
	/* 4-7: column addr cycles; 0-3: row addr cycles*/
	unsigned char num_addr_cycle;
	unsigned char bits_percell;
	unsigned short max_badblk_perLUN;
	unsigned short blk_edurce;
	/*Guaranteed valid blocks at beginning of target*/
	unsigned char g_v_blk_begin;
	unsigned short blk_edurce_g_v_blk;
	unsigned char progm_perpage;
	unsigned char prt_prog_att;/* obsolete */
	unsigned char bits_ECC_corretable;
	/*0-3: number of interleaved address bits*/
	unsigned char bits_intleav_addr;
	/*
	 * 6-7 Reserved (0)
	 * 5 1 = lower bit XNOR block address restriction
	 * 4 1 = read cache supported
	 * 3 Address restrictions for cache operations
	 * 2 1 = program cache supported
	 * 1 1 = no block address restrictions
	 * 0 Overlapped / concurrent interleaving support
	 */
	unsigned char intleav_op_attr;
	unsigned char reserve3[13];
	/*128~163 byte: Electrical parameters block*/
	unsigned char max_io_pin;
	/*
	 * 6-15 Reserved (0)
	 * 5 1 = supports timing mode 5
	 * 4 1 = supports timing mode 4
	 * 3 1 = supports timing mode 3
	 * 2 1 = supports timing mode 2
	 * 1 1 = supports timing mode 1
	 * 0 1 = supports timing mode 0, shall be 1
	 */
	unsigned short asy_time_mode;
	unsigned short asy_prog_cach_time_mode;	/*obsolete*/
	unsigned short Tprog;	/*Maximum page program time (Ts)*/
	unsigned short Tbers;	/*Maximum block erase time (Ts)*/
	unsigned short Tr;	/*Maximum page read time (Ts)*/
	unsigned short Tccs;	/*Minimum change column setup time (ns)*/
	/*
	 * 6-15 Reserved (0)
	 * 5 1 = supports timing mode 5
	 * 4 1 = supports timing mode 4
	 * 3 1 = supports timing mode 3
	 * 2 1 = supports timing mode 2
	 * 1 1 = supports timing mode 1
	 * 0 1 = supports timing mode 0
	 */
	unsigned short src_syn_time_mode;
	/*
	 * 3-7 Reserved (0)
	 * 2 1 = device supports CLK stopped for data input
	 * 1 1 = typical capacitance values present
	 * 0 tCAD value to use
	 */
	unsigned char src_syn_feature;
	unsigned short CLK_input_pin;
	unsigned short IO_pin;
	unsigned short input_pin;
	unsigned char max_input_pin;
	unsigned char dr_strgth;
	/*Maximum interleaved page read time (Ts)*/
	unsigned short Tir;
	/*Program page register clear enhancement tADL value (ns)*/
	unsigned short Tadl;
	unsigned char reserve4[8];
	/*164~255 byte: Vendor block*/
	unsigned short vd_ver;
	unsigned char vd_spec[88];
	unsigned short int_CRC;
	/*256~ byte: Redundant Parameter Pages*/

} __attribute__ ((__packed__));
#endif

int nand_init(struct platform_device *pdev);

void aml_nand_set_onfi_features(struct aml_nand_chip *aml_chip,
	uint8_t *buf, int addr);

uint8_t aml_nand_get_onfi_features(struct aml_nand_chip *aml_chip,
	uint8_t *buf, int addr);

int aml_nand_init(struct aml_nand_chip *aml_chip);

int aml_nand_read_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page);

int aml_nand_write_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required, int page);

int aml_nand_read_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page);

int aml_nand_write_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required, int page);

int aml_nand_read_oob(struct mtd_info *mtd,
	struct nand_chip *chip, int page);

int aml_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page);

int aml_nand_block_bad(struct mtd_info *mtd, loff_t ofs);

int aml_nand_block_markbad(struct mtd_info *mtd, loff_t ofs);

void aml_nand_dma_read_buf(struct mtd_info *mtd, uint8_t *buf, int len);

void aml_nand_dma_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len);

void aml_platform_cmd_ctrl(struct aml_nand_chip *aml_chip,
	int cmd, unsigned int ctrl);

void aml_platform_write_byte(struct aml_nand_chip *aml_chip, uint8_t data);

int aml_platform_wait_devready(struct aml_nand_chip *aml_chip, int chipnr);

void aml_platform_get_user_byte(struct aml_nand_chip *aml_chip,
	unsigned char *oob_buf, int byte_num);

void aml_platform_set_user_byte(struct aml_nand_chip *aml_chip,
	unsigned char *oob_buf, int byte_num);

void aml_nand_base_command(struct aml_nand_chip *aml_chip,
	unsigned int command, int column, int page_addr, int chipnr);

int aml_nand_block_bad_scrub_update_bbt(struct mtd_info *mtd);

int aml_ubootenv_init(struct aml_nand_chip *aml_chip);

int aml_nand_read_env(struct mtd_info *mtd, size_t offset, u_char *buf);

int aml_nand_save_env(struct mtd_info *mtd, u_char *buf);

int amlnf_dtb_init(struct aml_nand_chip *aml_chip);

int aml_nand_read_dtb(struct mtd_info *mtd, size_t offset, u_char *buf);

int aml_nand_save_dtb(struct mtd_info *mtd, u_char *buf);

int aml_key_init(struct aml_nand_chip *aml_chip);

int aml_nand_rsv_erase_protect(struct mtd_info *mtd, unsigned int block_addr);

int aml_nand_save_key(struct mtd_info *mtd, u_char *buf);

int aml_nand_read_key(struct mtd_info *mtd, size_t offset, u_char *buf);

int aml_nand_key_check(struct mtd_info *mtd);

/*int aml_nand_free_valid_env(struct mtd_info *mtd);*/

int aml_nand_save_bbt(struct mtd_info *mtd, u_char *buf);

int aml_nand_rsv_info_init(struct mtd_info *mtd);

int aml_nand_scan_rsv_info(struct mtd_info *mtd,
	struct aml_nandrsv_info_t *nandrsv_info);

int aml_nand_env_check(struct mtd_info *mtd);

int aml_nand_bbt_check(struct mtd_info *mtd);

int aml_nand_dtb_check(struct mtd_info *mtd);

int aml_nand_scan_bbt(struct mtd_info *mtd);

int aml_nand_scan(struct mtd_info *mtd, int maxchips);

int aml_nand_write_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required, int page);

int aml_nand_write_page(struct mtd_info *mtd,
	struct nand_chip *chip, uint32_t offset,
	int data_len,
	const uint8_t *buf,
	int oob_required, int page, int cached, int raw);

void aml_nand_base_command(struct aml_nand_chip *aml_chip,
	unsigned int command, int column, int page_addr, int chipnr);

void aml_nand_command(struct mtd_info *mtd,
	unsigned int command, int column, int page_addr);

int aml_nand_wait(struct mtd_info *mtd, struct nand_chip *chip);

int aml_nand_erase_cmd(struct mtd_info *mtd, int page);
/* for boot operations */
int m3_nand_boot_erase_cmd(struct mtd_info *mtd, int page);

int m3_nand_boot_read_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page);

int m3_nand_boot_read_oob(struct mtd_info *mtd,
	struct nand_chip *chip, int page);

int m3_nand_boot_write_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required, int page);

int m3_nand_boot_write_page(struct mtd_info *mtd, struct nand_chip *chip,
	uint32_t offset, int data_len, const uint8_t *buf,
	int oob_required, int page, int cached, int raw);

int boot_device_register(struct aml_nand_chip *aml_chip);
int add_mtd_partitions(struct mtd_info *mtd,
	const struct mtd_partition *part, int num);

#ifdef AML_NAND_UBOOT
extern int get_partition_from_dts(unsigned char *buffer);
#endif

extern unsigned char pagelist_1ynm_hynix256_mtd[128];
extern struct hw_controller *controller;
extern struct aml_nand_device aml_nand_mid_device;
extern struct nand_hw_control upper_controller;
extern uint8_t nand_boot_flag;
/*external defined variable*/
extern int info_disprotect;
extern struct aml_nand_flash_dev aml_nand_flash_ids[];

void aml_nand_new_nand_param_init(struct mtd_info *mtd,
	struct aml_nand_flash_dev *type);

int aml_nand_get_fbb_issue(void);

void aml_nand_check_fbb_issue(u8 *dev_id);

#endif

