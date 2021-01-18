/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MACH_IMX_CLK_BLK_CTRL_H
#define __MACH_IMX_CLK_BLK_CTRL_H

enum imx_blk_ctrl_hw_type {
	BLK_CTRL_CLK_MUX,
	BLK_CTRL_CLK_GATE,
	BLK_CTRL_CLK_SHARED_GATE,
	BLK_CTRL_CLK_PLL14XX,
	BLK_CTRL_RESET,
};

struct imx_blk_ctrl_hw {
	int type;
	char *name;
	u32 offset;
	u32 shift;
	u32 mask;
	u32 width;
	u32 flags;
	u32 id;
	void *parents;
	u32 parents_count;
	int *shared_count;
	const struct imx_pll14xx_clk *pll_tbl;
};

struct imx_blk_ctrl_dev_data {
	struct imx_blk_ctrl_hw *hws;
	u32 hws_num;

	u32 clocks_max;
	u32 resets_max;

	u32 pm_runtime_saved_regs_num;
	u32 pm_runtime_saved_regs[];
};

#define IMX_BLK_CTRL(_type, _name, _id, _offset, _shift, _width, _mask, _parents, _parents_count, _flags, sh_count, _pll_tbl) \
	{						\
		.type = _type,				\
		.name = _name,				\
		.id = _id,				\
		.offset = _offset,			\
		.shift = _shift,			\
		.width = _width,			\
		.mask = _mask,				\
		.parents = _parents,			\
		.parents_count = _parents_count,	\
		.flags = _flags,			\
		.shared_count = sh_count,		\
		.pll_tbl = _pll_tbl,			\
	}

#define IMX_BLK_CTRL_CLK_MUX(_name, _id, _offset, _shift, _width, _parents) \
	IMX_BLK_CTRL(BLK_CTRL_CLK_MUX, _name, _id, _offset, _shift, _width, 0, _parents, ARRAY_SIZE(_parents), 0, NULL, NULL)

#define IMX_BLK_CTRL_CLK_MUX_FLAGS(_name, _id, _offset, _shift, _width, _parents, _flags) \
	IMX_BLK_CTRL(BLK_CTRL_CLK_MUX, _name, _id, _offset, _shift, _width, 0, _parents, ARRAY_SIZE(_parents), _flags, NULL, NULL)

#define IMX_BLK_CTRL_CLK_GATE(_name, _id, _offset, _shift, _parents) \
	IMX_BLK_CTRL(BLK_CTRL_CLK_GATE, _name, _id, _offset, _shift, 1, 0, _parents, 1, 0, NULL, NULL)

#define IMX_BLK_CTRL_CLK_SHARED_GATE(_name, _id, _offset, _shift, _parents, sh_count) \
	IMX_BLK_CTRL(BLK_CTRL_CLK_SHARED_GATE, _name, _id, _offset, _shift, 1, 0, _parents, 1, 0, sh_count, NULL)

#define IMX_BLK_CTRL_CLK_PLL14XX(_name, _id, _offset, _parents, _pll_tbl) \
	IMX_BLK_CTRL(BLK_CTRL_CLK_PLL14XX, _name, _id, _offset, 0, 0, 0, _parents, 1, 0, NULL, _pll_tbl)

#define IMX_BLK_CTRL_RESET(_id, _offset, _shift) \
	IMX_BLK_CTRL(BLK_CTRL_RESET, NULL, _id, _offset, _shift, 0, 1, NULL, 0, 0, NULL, NULL)

#define IMX_BLK_CTRL_RESET_MASK(_id, _offset, _shift, mask) \
	IMX_BLK_CTRL(BLK_CTRL_RESET, NULL, _id, _offset, _shift, 0, mask, NULL, 0, 0, NULL, NULL)

extern const struct imx_blk_ctrl_dev_data imx8mp_audio_blk_ctrl_dev_data __initconst;
extern const struct imx_blk_ctrl_dev_data imx8mp_media_blk_ctrl_dev_data __initconst;
extern const struct imx_blk_ctrl_dev_data imx8mp_hdmi_blk_ctrl_dev_data __initconst;

#endif

