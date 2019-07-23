/****************************************************************************
*
*    The MIT License (MIT)
*
*    Copyright (c) 2014 - 2019 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************
*
*    The GPL License (GPL)
*
*    Copyright (C) 2014 - 2019 Vivante Corporation
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version 2
*    of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*****************************************************************************
*
*    Note: This software is released under dual MIT and GPL licenses. A
*    recipient may use this file under the terms of either the MIT license or
*    GPL License. If you wish to use only one license not the other, you can
*    indicate your decision by deleting one of the above license notices in your
*    version of this file.
*
*****************************************************************************/


#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"

/* Disable MSI for internal FPGA build except PPC */
#if gcdFPGA_BUILD && !defined(CONFIG_PPC)
#define USE_MSI     0
#else
#define USE_MSI     1
#endif

gceSTATUS
_AdjustParam(
    IN gcsPLATFORM *Platform,
    OUT gcsMODULE_PARAMETERS *Args
    );

static struct _gcsPLATFORM_OPERATIONS default_ops =
{
    .adjustParam   = _AdjustParam,
};

#if USE_LINUX_PCIE

#define MAX_PCIE_DEVICE 4
#define MAX_PCIE_BAR    6

struct _gcsPCIEInfo
{
    gctPOINTER bar[MAX_PCIE_BAR];
    struct pci_dev *pdev;
};

struct _gcsPLATFORM_PCIE
{
    struct _gcsPLATFORM base;
    struct _gcsPCIEInfo pcieInfo[MAX_PCIE_DEVICE];
    unsigned int device_number;
};


struct _gcsPLATFORM_PCIE default_platform =
{
    .base =
    {
        .name = __FILE__,
        .ops  = &default_ops,
    },
};

#else

static struct _gcsPLATFORM default_platform =
{
    .name = __FILE__,
    .ops  = &default_ops,
};
#endif

gceSTATUS
_AdjustParam(
    IN gcsPLATFORM *Platform,
    OUT gcsMODULE_PARAMETERS *Args
    )
{
#if USE_LINUX_PCIE
    struct _gcsPLATFORM_PCIE *pcie_platform = (struct _gcsPLATFORM_PCIE *)Platform;
    struct pci_dev *pdev = pcie_platform->pcieInfo[0].pdev;
    unsigned char   irqline = pdev->irq;
    unsigned int i;
    unsigned int devIndex, coreIndex = 0;

    if (Args->irqs[gcvCORE_2D] != -1)
    {
        Args->irqs[gcvCORE_2D] = irqline;
    }
    if (Args->irqs[gcvCORE_MAJOR] != -1)
    {
        Args->irqs[gcvCORE_MAJOR] = irqline;
    }
    for (devIndex = 0; devIndex < pcie_platform->device_number; devIndex++)
    {
        struct pci_dev * pcieDev = pcie_platform->pcieInfo[devIndex].pdev;
        for (i = 0; i < gcvCORE_COUNT; i++)
        {
            if (Args->bars[i] != -1)
            {
                Args->irqs[coreIndex] = pcieDev->irq;
                Args->registerBasesMapped[coreIndex]    =
                pcie_platform->pcieInfo[devIndex].bar[i] =
                    (gctPOINTER)pci_iomap(pcieDev, Args->bars[i], Args->registerSizes[coreIndex]);
                coreIndex++;
            }
        }
    }
    Args->contiguousRequested = gcvTRUE;
#endif
    return gcvSTATUS_OK;
}


#if USE_LINUX_PCIE
static const struct pci_device_id vivpci_ids[] = {
  {
    .class = 0x000000,
    .class_mask = 0x000000,
    .vendor = 0x10ee,
    .device = 0x7012,
    .subvendor = PCI_ANY_ID,
    .subdevice = PCI_ANY_ID,
    .driver_data = 0
  }, { /* End: all zeroes */ }
};

MODULE_DEVICE_TABLE(pci, vivpci_ids);


static int gpu_sub_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    static u64 dma_mask = DMA_BIT_MASK(40);
#else
    static u64 dma_mask = DMA_40BIT_MASK;
#endif

    gcmkPRINT("PCIE DRIVER PROBED");
    if (pci_enable_device(pdev)) {
        printk(KERN_ERR "galcore: pci_enable_device() failed.\n");
    }

    if (pci_set_dma_mask(pdev, dma_mask)) {
        printk(KERN_ERR "galcore: Failed to set DMA mask.\n");
    }

    pci_set_master(pdev);

    if (pci_request_regions(pdev, "galcore")) {
        printk(KERN_ERR "galcore: Failed to get ownership of BAR region.\n");
    }

#if USE_MSI
    if (pci_enable_msi(pdev)) {
        printk(KERN_ERR "galcore: Failed to enable MSI.\n");
    }
#endif
    default_platform.pcieInfo[default_platform.device_number++].pdev = pdev;
    return 0;
}

static void gpu_sub_remove(struct pci_dev *pdev)
{
    pci_set_drvdata(pdev, NULL);
#if USE_MSI
    pci_disable_msi(pdev);
#endif
    pci_clear_master(pdev);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    return;
}

static struct pci_driver gpu_pci_subdriver = {
    .name = DEVICE_NAME,
    .id_table = vivpci_ids,
    .probe = gpu_sub_probe,
    .remove = gpu_sub_remove
};

#endif

static struct platform_device *default_dev;

int gckPLATFORM_Init(struct platform_driver *pdrv,
            struct _gcsPLATFORM **platform)
{
    int ret;
    default_dev = platform_device_alloc(pdrv->driver.name, -1);

    if (!default_dev) {
        printk(KERN_ERR "galcore: platform_device_alloc failed.\n");
        return -ENOMEM;
    }

    /* Add device */
    ret = platform_device_add(default_dev);
    if (ret) {
        printk(KERN_ERR "galcore: platform_device_add failed.\n");
        goto put_dev;
    }

    *platform = (gcsPLATFORM *)&default_platform;

#if USE_LINUX_PCIE
    ret = pci_register_driver(&gpu_pci_subdriver);
#endif

    return 0;

put_dev:
    platform_device_put(default_dev);

    return ret;
}

int gckPLATFORM_Terminate(struct _gcsPLATFORM *platform)
{
    if (default_dev) {
        platform_device_unregister(default_dev);
        default_dev = NULL;
    }

#if USE_LINUX_PCIE
    {
        unsigned int devIndex;
        struct _gcsPLATFORM_PCIE *pcie_platform = (struct _gcsPLATFORM_PCIE *)platform;
        for (devIndex = 0; devIndex < pcie_platform->device_number; devIndex++)
        {
            unsigned int i;
            for (i = 0; i < MAX_PCIE_BAR; i++)
            {
                if (pcie_platform->pcieInfo[devIndex].bar[i] != 0)
                {
                    pci_iounmap(pcie_platform->pcieInfo[devIndex].pdev, pcie_platform->pcieInfo[devIndex]. bar[i]);
                }
            }
        }

        pci_unregister_driver(&gpu_pci_subdriver);
    }
#endif

    return 0;
}
