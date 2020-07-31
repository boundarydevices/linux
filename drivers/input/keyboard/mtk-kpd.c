// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author Fengping Yu <fengping.yu@mediatek.com>
 */
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/input/matrix_keypad.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MTK_KPD_NAME		"mtk-kpd"
#define MTK_KPD_MEM		0x0004
#define MTK_KPD_DEBOUNCE	0x0018
#define MTK_KPD_DEBOUNCE_MASK	GENMASK(13, 0)
#define MTK_KPD_DEBOUNCE_MAX_US	256000
#define MTK_KPD_SEL		0x0020
#define MTK_KPD_SEL_DOUBLE_KP_MODE	BIT(0)
#define MTK_KPD_SEL_COL	GENMASK(15, 10)
#define MTK_KPD_SEL_ROW	GENMASK(9, 4)
#define MTK_KPD_SEL_ROWMASK(X)  GENMASK(4 - 1 + X, 4)
#define MTK_KPD_SEL_COLMASK(X)  GENMASK(10 - 1 + X, 10)
#define MTK_KPD_NUM_MEMS	5
#define MTK_KPD_NUM_BITS	136	/* 4*32+8 MEM5 only use 8 BITS */

struct mtk_keypad {
	struct regmap *regmap;
	struct input_dev *input_dev;
	struct clk *clk;
	void __iomem *base;
	u32 n_rows;
	u32 n_cols;
	bool double_keys;
	DECLARE_BITMAP(keymap_state, MTK_KPD_NUM_BITS);
};

static const struct regmap_config keypad_regmap_cfg = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = sizeof(u32),
	.max_register = 36,
};

/*
 * - For single key detection
 * | hardware key code | col0 | col1 | col2|
 * | ----------------- | -----| ---- | --- |
 * | row0              | 0    | 1    | 2   |
 * | row1              | 9    | 10   | 11  |
 * | row2              | 18   | 19   | 20  |
 *
 * - For double key detection
 * | hardware key code | col0 | col1 |
 * | ----------------- | -----| ---- |
 * | row0              | 0/1  | 2/3  |
 * | row1              | 13/14| 15/16|
 */
static void bitnr_to_col_row(bool is_double_keys, int bit_nr, int *col,
			     int *row)
{
	if (!is_double_keys) {
		*row = bit_nr / 9;
		*col = bit_nr % 3;
	} else {
		*row = bit_nr / 10;
		*col = bit_nr / (2 + (*row * 13));
	}
}

static irqreturn_t kpd_irq_handler(int irq, void *dev_id)
{
	struct mtk_keypad *keypad = dev_id;
	unsigned short *keycode = keypad->input_dev->keycode;
	DECLARE_BITMAP(new_state, MTK_KPD_NUM_BITS);
	DECLARE_BITMAP(change, MTK_KPD_NUM_BITS);
	int bit_nr;
	int col, row;
	int pressed;
	unsigned short code;

	regmap_bulk_read(keypad->regmap, MTK_KPD_MEM,
			new_state, MTK_KPD_NUM_MEMS);

	bitmap_xor(change, new_state, keypad->keymap_state, MTK_KPD_NUM_BITS);

	for_each_set_bit(bit_nr, change, MTK_KPD_NUM_BITS) {
		/* 1: not pressed, 0: pressed */
		pressed = !test_bit(bit_nr, new_state);
		dev_dbg(&keypad->input_dev->dev, "%s",
			pressed ? "pressed" : "released");

		bitnr_to_col_row(keypad->double_keys, bit_nr, &col, &row);

		/* 32bit register only use low 16bit as keypad mem register */
		code = keycode[MATRIX_SCAN_CODE(row, col, 0)];

		input_report_key(keypad->input_dev, code, pressed);
		input_sync(keypad->input_dev);

		dev_dbg(&keypad->input_dev->dev,
			"report Linux keycode = %d\n", code);
	}

	bitmap_copy(keypad->keymap_state, new_state, MTK_KPD_NUM_BITS);

	return IRQ_HANDLED;
}

static void kpd_clk_disable(void *data)
{
	clk_disable_unprepare(data);
}

static int kpd_pdrv_probe(struct platform_device *pdev)
{
	struct mtk_keypad *keypad;
	unsigned int irq;
	u32 debounce;
	bool wakeup;
	int ret;
	int i;

	keypad = devm_kzalloc(&pdev->dev, sizeof(*keypad), GFP_KERNEL);
	if (!keypad)
		return -ENOMEM;

	keypad->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(keypad->base))
		return PTR_ERR(keypad->base);

	keypad->regmap = devm_regmap_init_mmio(&pdev->dev,
					       keypad->base,
					       &keypad_regmap_cfg);
	if (IS_ERR(keypad->regmap)) {
		dev_err(&pdev->dev,
			"regmap init failed:%pe\n", keypad->regmap);
		return PTR_ERR(keypad->regmap);
	}

	bitmap_fill(keypad->keymap_state, MTK_KPD_NUM_BITS);
	for (i = 0; i < MTK_KPD_NUM_MEMS; i++) {
		/* initialize bitmap to match the hardware
		 * ff,0000ffff,0000ffff,0000ffff,0000ffff
		 */
		bitmap_clear(keypad->keymap_state,
			     keypad_regmap_cfg.val_bits * i + 16, 16);
	}

	keypad->input_dev = devm_input_allocate_device(&pdev->dev);
	if (!keypad->input_dev) {
		dev_err(&pdev->dev, "Failed to allocate input dev\n");
		return -ENOMEM;
	}

	keypad->input_dev->name = MTK_KPD_NAME;
	keypad->input_dev->id.bustype = BUS_HOST;

	ret = matrix_keypad_parse_properties(&pdev->dev, &keypad->n_rows,
					     &keypad->n_cols);
	if (ret) {
		dev_err(&pdev->dev, "Failed to parse keypad params\n");
		return ret;
	}

	if (device_property_read_u32(&pdev->dev, "mediatek,debounce-us",
				     &debounce))
		debounce = 16000;

	if (debounce > MTK_KPD_DEBOUNCE_MAX_US) {
		dev_err(&pdev->dev, "Debounce time exceeds the maximum allowed time %dus\n",
			MTK_KPD_DEBOUNCE_MAX_US);
		return -EINVAL;
	}

	wakeup = device_property_read_bool(&pdev->dev, "wakeup-source");

	keypad->double_keys =
		device_property_read_bool(&pdev->dev, "mediatek,double-keys");

	dev_dbg(&pdev->dev, "n_row=%d n_col=%d debounce=%d\n",
		keypad->n_rows, keypad->n_cols, debounce);

	ret = matrix_keypad_build_keymap(NULL, NULL,
					 keypad->n_rows,
					 keypad->n_cols,
					 NULL,
					 keypad->input_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to build keymap\n");
		return ret;
	}

	regmap_write(keypad->regmap, MTK_KPD_DEBOUNCE,
		     debounce * 32 / 1000 & MTK_KPD_DEBOUNCE_MASK);

	if (keypad->double_keys)
		regmap_update_bits(keypad->regmap, MTK_KPD_SEL,
				   MTK_KPD_SEL_DOUBLE_KP_MODE,
				   MTK_KPD_SEL_DOUBLE_KP_MODE);

	regmap_update_bits(keypad->regmap, MTK_KPD_SEL, MTK_KPD_SEL_ROW,
			   MTK_KPD_SEL_ROWMASK(keypad->n_rows));

	regmap_update_bits(keypad->regmap, MTK_KPD_SEL, MTK_KPD_SEL_COL,
			   MTK_KPD_SEL_COLMASK(keypad->n_cols));

	keypad->clk = devm_clk_get(&pdev->dev, "kpd");
	if (IS_ERR(keypad->clk))
		return PTR_ERR(keypad->clk);

	ret = clk_prepare_enable(keypad->clk);
	if (ret) {
		dev_err(&pdev->dev, "cannot prepare/enable keypad clock\n");
		return ret;
	}

	ret = devm_add_action_or_reset(&pdev->dev, kpd_clk_disable, keypad->clk);
	if (ret)
		return ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(&pdev->dev, irq,
					NULL, kpd_irq_handler, IRQF_ONESHOT,
					MTK_KPD_NAME, keypad);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ#%d:%d\n",
			irq, ret);
		return ret;
	}

	ret = input_register_device(keypad->input_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register device\n");
		return ret;
	}

	ret =  device_init_wakeup(&pdev->dev, wakeup);
	if (ret)
		dev_warn(&pdev->dev, "device_init_wakeup fail\n");

	return 0;
}

static const struct of_device_id kpd_of_match[] = {
	{ .compatible = "mediatek,mt6779-keypad" },
	{ .compatible = "mediatek,mt6873-keypad" },
	{ /* sentinel */ }
};

static struct platform_driver kpd_pdrv = {
	.probe = kpd_pdrv_probe,
	.driver = {
		   .name = MTK_KPD_NAME,
		   .of_match_table = kpd_of_match,
	},
};
module_platform_driver(kpd_pdrv);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Keypad (KPD) Driver");
MODULE_LICENSE("GPL");
