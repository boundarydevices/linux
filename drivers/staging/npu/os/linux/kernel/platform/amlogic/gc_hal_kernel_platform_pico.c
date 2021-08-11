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
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>

#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
#include <dt-bindings/clock/g12a-clkc.h>
#else
#include <dt-bindings/clock/amlogic,g12a-clkc.h>
#endif

/*======== power version 0 hardware reg begin ===========*/
#define AO_RTI_BASE           0xff800000
#define AO_RTI_GEN_PWR_SLEEP0 (AO_RTI_BASE + (0x3a<<2))
#define AO_RTI_GEN_PWR_ISO0   (AO_RTI_BASE + (0x3b<<2))


/*======== power version 1 hardware reg begin ===========*/
#define P_PWRCTRL_ISO_EN1      0xfe007818
#define P_PWRCTRL_PWR_OFF1     0xfe00781c

static    uint32_t HHI_NANOQ_MEM_PD_REG0 = 0xff63c10c;
static    uint32_t HHI_NANOQ_MEM_PD_REG1 = 0xff63c110;
static  uint32_t RESET_LEVEL2 = 0xffd01088;

static  uint32_t NN_clk = 0xff63c1c8;
static  uint32_t nn_power_version = 0;


static int hardwareResetNum = 0;
module_param(hardwareResetNum, int, 0644);
static int nanoqFreq = 800000000;
module_param(nanoqFreq, int, 0644);

gceSTATUS _InitDtsRegValue(IN gcsPLATFORM *Platform)
{
    int ret = 0;
    struct resource *res = NULL;
    struct platform_device *pdev = Platform->device;


    res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
    if (res)
    {
        HHI_NANOQ_MEM_PD_REG0 = (unsigned long)res->start;
        printk("reg resource 2, start: %lx,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
    if (res)
    {
        HHI_NANOQ_MEM_PD_REG1 = (unsigned long)res->start;
        printk("reg resource 3, start: %lx,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
    if (res)
    {
        RESET_LEVEL2 = (unsigned long)res->start;
        printk("reg resource 4, start: %lx,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "NN_CLK");
    if (res)
    {
        NN_clk = (unsigned long)res->start;
        printk("reg resource NN_CLK, start: %lx,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
    }

    ret = of_property_read_u32(pdev->dev.of_node,"nn_power_version",&nn_power_version);
    printk("npu_version: %d\n",nn_power_version);
    return gcvSTATUS_OK;
}

gceSTATUS _AdjustParam(IN gcsPLATFORM *Platform,OUT gcsMODULE_PARAMETERS *Args)
{
    struct resource *res = NULL;
    struct platform_device *pdev = Platform->device;
    int irqLine = platform_get_irq_byname(pdev, "galcore");

    if (irqLine >= 0)
        printk("galcore irq number is %d.\n", irqLine);
    if (irqLine < 0) {
        printk("get galcore irq resource error\n");
        irqLine = platform_get_irq(pdev, 0);
        printk("galcore irq number is %d\n", irqLine);
        if (irqLine < 0)
            return gcvSTATUS_OUT_OF_RESOURCES;
    }
    Args->irqs[gcvCORE_MAJOR] = irqLine;

    /*================read reg value from dts===============*/
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res)
    {
        Args->registerBases[0] = (gctPHYS_ADDR_T)res->start;
        Args->registerSizes[0] = (gctSIZE_T)(res->end - res->start+1);
    }
    else
    {
        printk("no memory resource 0\n");
        Args->registerBases[0] = 0xFF100000;
        Args->registerSizes[0] = 2 << 10;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (res)
    {
        Args->extSRAMBases[0] = (gctPHYS_ADDR_T)res->start;

        Args->contiguousBase = 0;
        Args->contiguousSize = (gctSIZE_T)(res->end - res->start+1);
    }
    else
    {
        printk("no memory resource 1\n");
        Args->contiguousBase = 0;
        Args->contiguousSize = 0x400000;
    }
    return gcvSTATUS_OK;
}

gceSTATUS _RegWrite(uint32_t reg, uint32_t writeval)
{
    void __iomem *vaddr = NULL;
    reg = round_down(reg, 0x3);

    vaddr = ioremap(reg, 0x4);
    writel(writeval, vaddr);
    iounmap(vaddr);

    return gcvSTATUS_OK;
}

gceSTATUS _RegRead(uint32_t reg,uint32_t *readval)
{
    void __iomem *vaddr = NULL;
    reg = round_down(reg, 0x3);
    vaddr = ioremap(reg, 0x4);
    *readval = readl(vaddr);
    iounmap(vaddr);
    return gcvSTATUS_OK;
}
gceSTATUS get_nna_status(struct platform_device *dev)
{
    int ret = 0;
    uint32_t readReg = 0;
    uint32_t nn_ef[2];
    struct platform_device *pdev = dev;

    ret = of_property_read_u32_array(pdev->dev.of_node,"nn_efuse", &nn_ef[0], 2);
    if (ret == 0)
    {
        _RegRead(nn_ef[0],&readReg);
        readReg = (readReg & nn_ef[1]);
        if (readReg == 0)
            return gcvSTATUS_OK;
        else
            return gcvSTATUS_MISMATCH;
    }
    else
    {
        return gcvSTATUS_OK;
    }
}
//us
void delay(uint32_t time)
{
    int i = 0,j = 0;
    for (j=0;j<1000;j++)
    {
        for (i = 0;i<time;i++);
    }
}
/********To Do: add dynamic set clock function*********/

void Getpower_88(void)
{
    uint32_t readReg = 0;
    _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
    readReg = (readReg & 0xfffcffff);
    _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);

    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);

    _RegRead(RESET_LEVEL2,&readReg);
    readReg = (readReg & 0xffffefff);
    _RegWrite(RESET_LEVEL2, readReg);

    _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
    readReg = (readReg & 0xfffcffff);
    _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);

    _RegRead(RESET_LEVEL2,&readReg);
    readReg = (readReg | (0x1<<12));
    _RegWrite(RESET_LEVEL2, readReg);

    _RegWrite(NN_clk, 0x7000700);
}

void Getpower_99(void)
{
    uint32_t readReg = 0;
    _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
    readReg = (readReg & 0xfffeffff);
    _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);

    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);

    _RegRead(RESET_LEVEL2,&readReg);
    readReg = (readReg & 0xffffefff);
    _RegWrite(RESET_LEVEL2, readReg);

    _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
    readReg = (readReg & 0xfffeffff);
    _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);

    _RegRead(RESET_LEVEL2,&readReg);
    readReg = (readReg | (0x1<<12));
    _RegWrite(RESET_LEVEL2, readReg);

    _RegWrite(NN_clk, 0x7000700);
}
void Getpower_a1(void)
{
    uint32_t readReg = 0;
    _RegRead(RESET_LEVEL2,&readReg);
    readReg = (readReg & (~(1<<3)));
    _RegWrite(RESET_LEVEL2, readReg);

    _RegRead(P_PWRCTRL_PWR_OFF1,&readReg);
    readReg = (readReg & (~(1<<3)));
    _RegWrite(P_PWRCTRL_PWR_OFF1, readReg);

    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);

    _RegRead(P_PWRCTRL_ISO_EN1,&readReg);
    readReg = (readReg & (~(1<<3)));
    _RegWrite(P_PWRCTRL_ISO_EN1, readReg);

    _RegRead(RESET_LEVEL2,&readReg);
    readReg = (readReg | (1<<3));
    _RegWrite(RESET_LEVEL2, readReg);

    _RegWrite(NN_clk, 0x3000300);
}
gceSTATUS _GetPower(IN gcsPLATFORM *Platform)
{
    _InitDtsRegValue(Platform);
    switch (nn_power_version)
    {
        case 1:
            Getpower_a1();
            break;
        case 2:
            Getpower_88();
            break;
        case 3:
            Getpower_99();
            break;
        default:
            printk("not find power_version\n");
    }
    return gcvSTATUS_OK;
}

void Downpower_88(void)
{
    uint32_t readReg = 0;
    _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
    readReg = (readReg | 0x30000);
    _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);

    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);

    _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
    readReg = (readReg | 0x30000);
    _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
}
void Downpower_99(void)
{
    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);
}
void Downpower_a1(void)
{
    uint32_t readReg=0;
    _RegRead(P_PWRCTRL_ISO_EN1,&readReg);
    readReg = (readReg | (1<<3));
    _RegWrite(P_PWRCTRL_ISO_EN1, readReg);

    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);

    _RegRead(P_PWRCTRL_PWR_OFF1,&readReg);
    readReg = (readReg | (1<<3));
    _RegWrite(P_PWRCTRL_PWR_OFF1, readReg);
}
gceSTATUS _DownPower(IN gcsPLATFORM *Platform)
{
    switch (nn_power_version)
    {
        case 1:
            Downpower_a1();
            break;
        case 2:
            Downpower_88();
            break;
        case 3:
            Downpower_99();
            break;
        default:
            printk("not find power_version\n");
    }
    return gcvSTATUS_OK;
}

gceSTATUS _Reset(IN gcsPLATFORM * Platform, IN gceCORE GPU)
{
    switch (nn_power_version)
    {
        case 1:
            Downpower_a1();
            mdelay(10);
            Getpower_a1();
            break;
        case 2:
            Downpower_88();
            mdelay(10);
            Getpower_88();
            break;
        case 3:
            Downpower_99();
            mdelay(10);
            Getpower_99();
            break;
        default:
            printk("not find power_version\n");
    }
    mdelay(2);
    printk("====>>>>npu hardware reset end!\n");
    hardwareResetNum++;
    if (hardwareResetNum > 10000)
    {
        printk("hardwareResetNum is too large over 10000,just set zero\n");
        hardwareResetNum = 0;
    }
    return gcvSTATUS_OK;
}


gceSTATUS  _SetPower(IN gcsPLATFORM * Platform,IN gceCORE GPU,IN gctBOOL Enable)
{
    if (Enable == 0)
    {
        switch (nn_power_version)
        {
            case 1:
                Downpower_a1();
                break;
            case 2:
                Downpower_88();
                break;
            case 3:
                Downpower_99();
                break;
            default:
                printk("not find power_version\n");
        }
    }
    else
    {
        switch (nn_power_version)
        {
            case 1:
                Getpower_a1();
                break;
            case 2:
                Getpower_88();
                break;
            case 3:
                Getpower_99();
                break;
            default:
                printk("not find power_version\n");
        }
    }
    return gcvSTATUS_OK;
}
static gcsPLATFORM_OPERATIONS default_ops =
{
    .adjustParam   = _AdjustParam,
    .getPower  = _GetPower,
    .reset = _Reset,
    .putPower = _DownPower,
    .setPower = _SetPower,
};

static gcsPLATFORM default_platform =
{
    .name = __FILE__,
    .ops  = &default_ops,
};


static struct platform_device *default_dev;

static const struct of_device_id galcore_dev_match[] = {
    {
        .compatible = "amlogic, galcore"
    },
    { },
};

int gckPLATFORM_Init(struct platform_driver *pdrv, gcsPLATFORM **platform)
{
    pdrv->driver.of_match_table = galcore_dev_match;

    *platform = &default_platform;
    /*  default_dev = platform;  hot plug just not support  */
    return 0;
}

int gckPLATFORM_Terminate(gcsPLATFORM *platform)
{
    if (default_dev) {
        platform_device_unregister(default_dev);
        default_dev = NULL;
    }

    return 0;
}
