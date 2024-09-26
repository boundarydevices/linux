/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2014-2018 MediaTek Inc.
 *
 * Author: Maoguang Meng <maoguang.meng@mediatek.com>
 *	   Sean Wang <sean.wang@mediatek.com>
 *
 */
#ifndef __MTK_EINT_H
#define __MTK_EINT_H

#include <linux/irqdomain.h>

#define MAX_PIN 999
#define MTK_EINT_EDGE_SENSITIVE           0
#define MTK_EINT_LEVEL_SENSITIVE          1
#define MTK_EINT_DBNC_SET_DBNC_BITS       4
#define MTK_EINT_DBNC_RST_BIT             (0x1 << 1)
#define MTK_EINT_DBNC_SET_EN              (0x1 << 0)
#define MTK_EINT_NO_OFFSET                0
#define MAX_BIT                           32
#define REG_OFFSET                        4
#define REG_ONE                           5
#define REG_VAL                           0xFFFFFFFF
#define DB_ONE                            8
#define FIRST                             1
#define SECOND                            2
#define THIRD                             3
#define ARRAY_ELEMENT                     4

//#define MTK_EINT_DEBUG

struct mtk_eint_regs {
	unsigned int	stat;
	unsigned int	ack;
	unsigned int	mask;
	unsigned int	mask_set;
	unsigned int	mask_clr;
	unsigned int	sens;
	unsigned int	sens_set;
	unsigned int	sens_clr;
	unsigned int	soft;
	unsigned int	soft_set;
	unsigned int	soft_clr;
	unsigned int	pol;
	unsigned int	pol_set;
	unsigned int	pol_clr;
	unsigned int	dom_en;
	unsigned int	dbnc_ctrl;
	unsigned int	dbnc_set;
	unsigned int	dbnc_clr;
	unsigned int	event;
	unsigned int	event_clr;
	unsigned int	raw_stat;
};

struct mtk_eint_ops {
	void (*ack)(struct irq_data *d);
};

struct mtk_eint_compatible {
	struct mtk_eint_ops ops;
	const struct mtk_eint_regs *regs;
};

struct mtk_eint_pin {
	bool enabled;
	unsigned int instance;
	unsigned int index;
	bool debounce;
	bool dual_edge;
};

struct mtk_eint_instance {
	const char *name;
	void __iomem *base;
	unsigned int number;
	unsigned int pin_list[MAX_PIN];
	unsigned int *wake_mask;
	unsigned int *cur_mask;
};

struct mtk_eint;

struct mtk_eint_xt {
	int (*get_gpio_n)(void *data, unsigned long eint_n,
			  unsigned int *gpio_n,
			  struct gpio_chip **gpio_chip);
	int (*get_gpio_state)(void *data, unsigned long eint_n);
	int (*set_gpio_as_eint)(void *data, unsigned long eint_n);
};

struct mtk_eint {
	struct device *dev;
	struct irq_domain *domain;
	int irq;

	/* An array to record the coordinate, index by global EINT ID */
	struct mtk_eint_pin *pins;
	/* An array to record the global EINT ID, index by coordinate*/
	struct mtk_eint_instance *instances;
	unsigned int total_pin_number;
	unsigned int instance_number;
	unsigned int dump_target_eint;
	const struct mtk_eint_compatible *comp;

	/* Used to fit into various pinctrl device */
	void *pctl;
	const struct mtk_eint_xt *gpio_xlate;
};

#if IS_ENABLED(CONFIG_EINT_MTK)
int mtk_eint_do_init(struct mtk_eint *eint);
int mtk_eint_do_suspend(struct mtk_eint *eint);
int mtk_eint_do_resume(struct mtk_eint *eint);
int mtk_eint_set_debounce(struct mtk_eint *eint, unsigned long eint_n,
			  unsigned int debounce);
int mtk_eint_find_irq(struct mtk_eint *eint, unsigned long eint_n);
int dump_eint_pin_status(unsigned int eint_num, char *buf, unsigned int buf_size);

#else
static inline int mtk_eint_do_init(struct mtk_eint *eint)
{
	return -EOPNOTSUPP;
}

static inline int mtk_eint_do_suspend(struct mtk_eint *eint)
{
	return -EOPNOTSUPP;
}

static inline int mtk_eint_do_resume(struct mtk_eint *eint)
{
	return -EOPNOTSUPP;
}

static inline int mtk_eint_set_debounce(struct mtk_eint *eint, unsigned long eint_n,
			  unsigned int debounce)
{
	return -EOPNOTSUPP;
}

static inline int mtk_eint_find_irq(struct mtk_eint *eint, unsigned long eint_n)
{
	return -EOPNOTSUPP;
}
#endif
#endif /* __MTK_EINT_H */
