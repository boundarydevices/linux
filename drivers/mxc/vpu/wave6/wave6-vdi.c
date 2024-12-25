// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Wave6 series multi-standard codec IP - low level access functions
 *
 * Copyright (C) 2021 CHIPS&MEDIA INC
 */

#include <linux/delay.h>
#include <linux/bug.h>
#include "wave6-vdi.h"
#include "wave6-vpu.h"
#include "wave6-regdefine.h"
#include "wave6-trace.h"

#define VDI_SYSTEM_ENDIAN VDI_LITTLE_ENDIAN
#define VDI_128BIT_BUS_SYSTEM_ENDIAN VDI_128BIT_LITTLE_ENDIAN

void wave6_vdi_writel(struct vpu_device *vpu_dev, unsigned int addr, unsigned int data)
{
	writel(data, vpu_dev->reg_base + addr);
	trace_writel(vpu_dev->dev, addr, data);
}

unsigned int wave6_vdi_readl(struct vpu_device *vpu_dev, u32 addr)
{
	unsigned int data;

	data = readl(vpu_dev->reg_base + addr);
	trace_readl(vpu_dev->dev, addr, data);

	return data;
}

unsigned int wave6_vdi_convert_endian(unsigned int endian)
{
	switch (endian) {
	case VDI_LITTLE_ENDIAN:
		endian = 0x00;
		break;
	case VDI_BIG_ENDIAN:
		endian = 0x0f;
		break;
	case VDI_32BIT_LITTLE_ENDIAN:
		endian = 0x04;
		break;
	case VDI_32BIT_BIG_ENDIAN:
		endian = 0x03;
		break;
	}

	return (endian & 0x0f);
}
