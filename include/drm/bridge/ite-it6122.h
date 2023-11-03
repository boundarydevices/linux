/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */

#ifndef __ITE_IT6122__
#define __ITE_IT6122__

#include <linux/types.h>

int it6122_bridge_power_on_off(bool on);
bool it6122_bridge_probed(void);

#endif /* __ITE_IT6122__ */
