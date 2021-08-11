/****************************************************************************
*
*    Copyright (c) 2005 - 2020 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#include <linux/io.h>
#include <linux/kernel.h>
#include <dt-bindings/clock/amlogic,g12a-clkc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"

/*========add by zxw for g12b hardware reg define========*/
#define AO_RTI_BASE           0xff800000
#define AO_RTI_GEN_PWR_SLEEP0 (AO_RTI_BASE + (0x3a<<2))   //0xff8000e8
#define AO_RTI_GEN_PWR_ISO0   (AO_RTI_BASE + (0x3b<<2))   //0xff8000ec

#define HHI_BASE_ADDR         0xff63c000
#define HHI_NANOQ_MEM_PD_REG0 (HHI_BASE_ADDR+(0x43<<2))
#define HHI_NANOQ_MEM_PD_REG1 (HHI_BASE_ADDR+(0x44<<2))

#define MAX_NANOQ_FREQ        800000000

gceSTATUS
_AdjustParam(
    IN gcsPLATFORM *Platform,
    OUT gcsMODULE_PARAMETERS *Args
    )
{
    struct platform_device *pdev = Platform->device;
    int irqLine = platform_get_irq_byname(pdev, "galcore");

    printk("galcore irq number is %d.\n", irqLine);
    if (irqLine< 0) {
        printk("get galcore irq resource error\n");
        irqLine = platform_get_irq(pdev, 0);
        printk("galcore irq number is %d\n", irqLine);
    }
    if (irqLine < 0) return gcvSTATUS_OUT_OF_RESOURCES;

    Args->irqs[gcvCORE_MAJOR] = irqLine;

    Args->contiguousBase = 0;
    Args->contiguousSize = 0x01000000;

    Args->extSRAMBases[0] = 0xFF000000;

    Args->registerBases[0] = 0xFF100000;
    Args->registerSizes[0] = 0x20000;

    return gcvSTATUS_OK;
}

int _RegWrite(unsigned int reg, unsigned int writeval)
{
    void __iomem *vaddr;
    reg = round_down(reg, 0x3);

    vaddr = ioremap(reg, 0x4);
    writel(writeval, vaddr);
    iounmap(vaddr);

    return 0;
}

int _RegRead(unsigned int reg,unsigned int *readval)
{
    void __iomem *vaddr;
    reg = round_down(reg, 0x3);
    vaddr = ioremap(reg, 0x4);
    *readval = readl(vaddr);
    iounmap(vaddr);
    return 0;
}
//us
void delay(unsigned int time)
{
    int i,j;
    for(j=0;j<1000;j++)
    {
        for(i = 0;i<time;i++);
    }
}

static void set_clock(struct platform_device *pdev)
{
    struct clk *npu_axi_clk = NULL;
    struct clk *npu_core_clk = NULL;
    npu_axi_clk = clk_get(&pdev->dev, "cts_vipnanoq_axi_clk_composite");
    if (IS_ERR(npu_axi_clk))
    {
        printk("%s: get npu_axi_clk error!!!\n", __func__);
        return;
    }
    else
    {
        clk_prepare_enable(npu_axi_clk);
    }
    clk_set_rate(npu_axi_clk, MAX_NANOQ_FREQ);

    npu_core_clk = clk_get(&pdev->dev, "cts_vipnanoq_core_clk_composite");
    if (IS_ERR(npu_core_clk))
    {
        printk("%s: get npu_core_clk error!!!\n", __func__);
        return;
    }
    else
    {
        clk_prepare_enable(npu_core_clk);
    }
    clk_set_rate(npu_core_clk, MAX_NANOQ_FREQ);
    return;
}
gceSTATUS
_GetPower(IN gcsPLATFORM *Platform)
{
    unsigned int readReg=0;
    _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
    readReg = (readReg & 0xfffcffff);
    _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
    _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
    readReg = (readReg & 0xfffcffff);
    _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);
    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);
    set_clock(Platform->device);
    delay(500);
    return gcvSTATUS_OK;
}

static struct _gcsPLATFORM_OPERATIONS default_ops =
{
    .adjustParam   = _AdjustParam,
    .getPower  = _GetPower,
};

static struct _gcsPLATFORM default_platform =
{
    .name = __FILE__,
    .ops  = &default_ops,
};

static struct platform_device *default_dev;

static const
struct of_device_id galcore_dev_match[] = {
    {
        .compatible = "amlogic, galcore"
    },
    { },
};

int gckPLATFORM_Init(struct platform_driver *pdrv,
            struct _gcsPLATFORM **platform)
{
    pdrv->driver.of_match_table = galcore_dev_match;

    *platform = &default_platform;
    return 0;
}

int gckPLATFORM_Terminate(struct _gcsPLATFORM *platform)
{
    if (default_dev) {
        platform_device_unregister(default_dev);
        default_dev = NULL;
    }

    return 0;
}
