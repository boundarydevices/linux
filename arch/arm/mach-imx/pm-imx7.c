/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/suspend.h>
#include <linux/genalloc.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <asm/fncpy.h>
#include <asm/mach/map.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/tlb.h>

#include "common.h"
#include "hardware.h"

#define MX7_SUSPEND_OCRAM_SIZE		0x1000
#define MX7_MAX_DDRC_IO_NUM		33
#define MX7_MAX_DDRC_NUM		34

#define MX7_SUSPEND_IRAM_ADDR_OFFSET	0

unsigned long imx7_iram_tlb_base_addr;
unsigned long imx7_iram_tlb_phys_addr;

static void __iomem *ccm_base;
static void __iomem *suspend_ocram_base;
static void (*imx7_suspend_in_ocram_fn)(void __iomem *ocram_vbase);
/*
 * suspend ocram space layout:
 * ======================== high address ======================
 *                              .
 *                              .
 *                              .
 *                              ^
 *                              ^
 *                              ^
 *                      imx7_suspend code
 *              PM_INFO structure(imx7_cpu_pm_info)
 * ======================== low address =======================
 */

struct imx7_pm_base {
	phys_addr_t pbase;
	void __iomem *vbase;
};

struct imx7_pm_socdata {
	u32 ddr_type;
	const char *ddrc_compat;
	const char *src_compat;
	const char *iomuxc_compat;
	const char *gpc_compat;
	const u32 ddrc_io_num;
	const u32 *ddrc_io_offset;
	const u32 ddrc_num;
	const u32 *ddrc_offset;
};

static const u32 imx7d_ddrc_io_offset[] __initconst = {
};

static const u32 imx7d_ddrc_offset[] __initconst = {
};

static const struct imx7_pm_socdata imx7d_pm_data __initconst = {
	.ddrc_compat = "fsl,imx7d-ddrc",
	.src_compat = "fsl,imx7d-src",
	.iomuxc_compat = "fsl,imx7d-iomuxc",
	.gpc_compat = "fsl,imx7d-gpc",
	.ddrc_io_num = ARRAY_SIZE(imx7d_ddrc_io_offset),
	.ddrc_io_offset = imx7d_ddrc_io_offset,
	.ddrc_num = 0,
	.ddrc_offset = NULL,
};

/*
 * This structure is for passing necessary data for low level ocram
 * suspend code(arch/arm/mach-imx/suspend-imx7.S), if this struct
 * definition is changed, the offset definition in
 * arch/arm/mach-imx/suspend-imx7.S must be also changed accordingly,
 * otherwise, the suspend to ocram function will be broken!
 */
struct imx7_cpu_pm_info {
	phys_addr_t pbase; /* The physical address of pm_info. */
	phys_addr_t resume_addr; /* The physical resume address for asm code */
	u32 ddr_type;
	u32 pm_info_size; /* Size of pm_info. */
	struct imx7_pm_base ddrc_base;
	struct imx7_pm_base src_base;
	struct imx7_pm_base iomuxc_base;
	struct imx7_pm_base ccm_base;
	struct imx7_pm_base gpc_base;
	struct imx7_pm_base l2_base;
	struct imx7_pm_base anatop_base;
	u32 ttbr1; /* Store TTBR1 */
	u32 ddrc_io_num; /* Number of DDRC IOs which need saved/restored. */
	u32 ddrc_io_val[MX7_MAX_DDRC_IO_NUM][2]; /* To save offset and value */
	u32 ddrc_num; /* Number of DDRC registers which need saved/restored. */
	u32 ddrc_val[MX7_MAX_DDRC_NUM][2]; /* To save offset and value */
} __aligned(8);

static struct map_desc imx7_pm_io_desc[] __initdata = {
	imx_map_entry(MX7D, AIPS1, MT_DEVICE),
	imx_map_entry(MX7D, AIPS2, MT_DEVICE),
	imx_map_entry(MX7D, AIPS3, MT_DEVICE),
};

static const char * const low_power_ocram_match[] __initconst = {
	"fsl,lpm-sram",
	NULL
};

static int imx7_suspend_finish(unsigned long val)
{
	if (!imx7_suspend_in_ocram_fn) {
		cpu_do_idle();
	} else {
		/*
		 * call low level suspend function in ocram,
		 * as we need to float DDR IO.
		 */
		local_flush_tlb_all();
		imx7_suspend_in_ocram_fn(suspend_ocram_base);
	}

	return 0;
}

static int imx7_pm_enter(suspend_state_t state)
{

	if (!imx7_iram_tlb_base_addr) {
		pr_warn("No IRAM/OCRAM memory allocated for suspend/resume \
			 code. Please ensure device tree has an entry for \
			 fsl,lpm-sram.\n");
		return -EINVAL;
	}

	switch (state) {
	case PM_SUSPEND_STANDBY:

		/* Zzz ... */
		cpu_do_idle();

		break;
	case PM_SUSPEND_MEM:
		imx_anatop_pre_suspend();
		imx_gpcv2_pre_suspend(true);

		/* Zzz ... */
		cpu_suspend(0, imx7_suspend_finish);

		imx_anatop_post_resume();
		imx_gpcv2_post_resume();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int imx7_pm_valid(suspend_state_t state)
{
	return state == PM_SUSPEND_STANDBY || state == PM_SUSPEND_MEM;
}

static const struct platform_suspend_ops imx7_pm_ops = {
	.enter = imx7_pm_enter,
	.valid = imx7_pm_valid,
};

void __init imx7_pm_set_ccm_base(void __iomem *base)
{
	ccm_base = base;
}

static struct map_desc iram_tlb_io_desc __initdata = {
	/* .virtual and .pfn are run-time assigned */
	.length     = SZ_1M,
	.type       = MT_MEMORY_RWX_NONCACHED,
};

static int __init imx7_dt_find_lpsram(unsigned long node, const char *uname,
				      int depth, void *data)
{
	unsigned long lpram_addr;
	__be32 *prop;

	if (of_flat_dt_match(node, low_power_ocram_match)) {
		prop = of_get_flat_dt_prop(node, "reg", NULL);
		if (!prop)
			return -EINVAL;

		lpram_addr = be32_to_cpup(prop);

		/* We need to create a 1M page table entry. */
		iram_tlb_io_desc.virtual = IMX_IO_P2V(lpram_addr & 0xFFF00000);
		iram_tlb_io_desc.pfn = __phys_to_pfn(lpram_addr & 0xFFF00000);
		imx7_iram_tlb_phys_addr = lpram_addr;
		imx7_iram_tlb_base_addr = IMX_IO_P2V(lpram_addr);
		iotable_init(&iram_tlb_io_desc, 1);
	}

	return 0;
}

void __init imx7_pm_map_io(void)
{
	unsigned long i, j;

	iotable_init(imx7_pm_io_desc, ARRAY_SIZE(imx7_pm_io_desc));
	/*
	 * Get the address of IRAM or OCRAM to be used by the low
	 * power code from the device tree.
	 */
	WARN_ON(of_scan_flat_dt(imx7_dt_find_lpsram, NULL));

	/* Return if no IRAM space is allocated for suspend/resume code. */
	if (!imx7_iram_tlb_base_addr) {
		pr_warn("No valid ocram avaiable for suspend/resume!\n");
		return;
	}

	/* Set all entries to 0. */
	memset((void *)imx7_iram_tlb_base_addr, 0, MX7_IRAM_TLB_SIZE);

	/*
	 * Make sure the IRAM virtual address has a mapping in the IRAM
	 * page table.
	 *
	 * Only use the top 12 bits [31-20] when storing the physical
	 * address in the page table as only these bits are required
	 * for 1M mapping.
	 */
	j = ((imx7_iram_tlb_base_addr >> 20) << 2) / 4;
	*((unsigned long *)imx7_iram_tlb_base_addr + j) =
		(imx7_iram_tlb_phys_addr & 0xFFF00000) | TT_ATTRIB_NON_CACHEABLE_1M;

	/*
	 * Make sure the AIPS1 virtual address has a mapping in the
	 * IRAM page table.
	 */
	for (i = 0; i < 4; i++) {
		j = ((IMX_IO_P2V(MX7D_AIPS1_BASE_ADDR + i * 0x100000) >> 20) << 2) / 4;
		*((unsigned long *)imx7_iram_tlb_base_addr + j) =
			((MX7D_AIPS1_BASE_ADDR + i * 0x100000) & 0xFFF00000) |
			TT_ATTRIB_NON_CACHEABLE_1M;
	}

	/*
	 * Make sure the AIPS2 virtual address has a mapping in the
	 * IRAM page table.
	 */
	for (i = 0; i < 4; i++) {
		j = ((IMX_IO_P2V(MX7D_AIPS2_BASE_ADDR + i * 0x100000) >> 20) << 2) / 4;
		*((unsigned long *)imx7_iram_tlb_base_addr + j) =
			((MX7D_AIPS2_BASE_ADDR + i * 0x100000) & 0xFFF00000) |
			TT_ATTRIB_NON_CACHEABLE_1M;
	}

	/*
	 * Make sure the AIPS3 virtual address has a mapping
	 * in the IRAM page table.
	 */
	for (i = 0; i < 4; i++) {
		j = ((IMX_IO_P2V(MX7D_AIPS3_BASE_ADDR + i * 0x100000) >> 20) << 2) / 4;
		*((unsigned long *)imx7_iram_tlb_base_addr + j) =
			((MX7D_AIPS3_BASE_ADDR + i * 0x100000) & 0xFFF00000) |
			TT_ATTRIB_NON_CACHEABLE_1M;
	}
}

static int __init imx7_suspend_init(const struct imx7_pm_socdata *socdata)
{
	struct device_node *node;
	struct imx7_cpu_pm_info *pm_info;
	int i, ret = 0;
	const u32 *ddrc_io_offset_array;
	const u32 *ddrc_offset_array;
	unsigned long iram_paddr;

	suspend_set_ops(&imx7_pm_ops);

	if (!socdata) {
		pr_warn("%s: invalid argument!\n", __func__);
		return -EINVAL;
	}

	/*
	 * 16KB is allocated for IRAM TLB, but only up 8k is for kernel TLB,
	 * The lower 8K is not used, so use the lower 8K for IRAM code and
	 * pm_info.
	 *
	 */
	iram_paddr = imx7_iram_tlb_phys_addr + MX7_SUSPEND_IRAM_ADDR_OFFSET;

	/* Make sure iram_paddr is 8 byte aligned. */
	if ((uintptr_t)(iram_paddr) & (FNCPY_ALIGN - 1))
		iram_paddr += FNCPY_ALIGN - iram_paddr % (FNCPY_ALIGN);

	/* Get the virtual address of the suspend code. */
	suspend_ocram_base = (void *)IMX_IO_P2V(iram_paddr);

	pm_info = suspend_ocram_base;
	/* pbase points to iram_paddr. */
	pm_info->pbase = iram_paddr;
	pm_info->resume_addr = virt_to_phys(ca7_cpu_resume);
	pm_info->pm_info_size = sizeof(*pm_info);

	/*
	 * ccm physical address is not used by asm code currently,
	 * so get ccm virtual address directly, as we already have
	 * it from ccm driver.
	 */
	pm_info->ccm_base.pbase = MX7D_CCM_BASE_ADDR;
	pm_info->ccm_base.vbase = (void __iomem *)
				   IMX_IO_P2V(MX7D_CCM_BASE_ADDR);

	pm_info->ddrc_base.pbase = MX7D_DDRC_BASE_ADDR;
	pm_info->ddrc_base.vbase = (void __iomem *)
				    IMX_IO_P2V(MX7D_DDRC_BASE_ADDR);

	pm_info->src_base.pbase = MX7D_SRC_BASE_ADDR;
	pm_info->src_base.vbase = (void __iomem *)
				   IMX_IO_P2V(MX7D_SRC_BASE_ADDR);

	pm_info->iomuxc_base.pbase = MX7D_IOMUXC_BASE_ADDR;
	pm_info->iomuxc_base.vbase = (void __iomem *)
				      IMX_IO_P2V(MX7D_IOMUXC_BASE_ADDR);

	pm_info->gpc_base.pbase = MX7D_GPC_BASE_ADDR;
	pm_info->gpc_base.vbase = (void __iomem *)
				   IMX_IO_P2V(MX7D_GPC_BASE_ADDR);

	pm_info->anatop_base.pbase = MX7D_ANATOP_BASE_ADDR;
	pm_info->anatop_base.vbase = (void __iomem *)
				  IMX_IO_P2V(MX7D_ANATOP_BASE_ADDR);

	pm_info->ddrc_io_num = socdata->ddrc_io_num;
	ddrc_io_offset_array = socdata->ddrc_io_offset;
	pm_info->ddrc_num = socdata->ddrc_num;
	ddrc_offset_array = socdata->ddrc_offset;

	/* initialize DDRC IO settings */
	for (i = 0; i < pm_info->ddrc_io_num; i++) {
		pm_info->ddrc_io_val[i][0] =
			ddrc_io_offset_array[i];
		pm_info->ddrc_io_val[i][1] =
			readl_relaxed(pm_info->iomuxc_base.vbase +
			ddrc_io_offset_array[i]);
	}
	/* initialize DDRC settings */
	for (i = 0; i < pm_info->ddrc_num; i++) {
		pm_info->ddrc_val[i][0] =
			ddrc_offset_array[i];
		pm_info->ddrc_val[i][1] =
			readl_relaxed(pm_info->ddrc_base.vbase +
			ddrc_offset_array[i]);
	}

	imx7_suspend_in_ocram_fn = fncpy(
		suspend_ocram_base + sizeof(*pm_info),
		&imx7_suspend,
		MX7_SUSPEND_OCRAM_SIZE - sizeof(*pm_info));

	goto put_node;

put_node:
	of_node_put(node);

	return ret;
}

static void __init imx7_pm_common_init(const struct imx7_pm_socdata
					*socdata)
{
	int ret;

	if (IS_ENABLED(CONFIG_SUSPEND)) {
		ret = imx7_suspend_init(socdata);
		if (ret)
			pr_warn("%s: No DDR LPM support with suspend %d!\n",
				__func__, ret);
	}
}

void __init imx7d_pm_init(void)
{
	imx7_pm_common_init(&imx7d_pm_data);
}
