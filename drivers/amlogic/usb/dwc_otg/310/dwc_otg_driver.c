/*
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/dwc_otg_driver.c $
 * $Revision: #94 $
 * $Date: 2012/12/21 $
 * $Change: 2131568 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */

/** @file
 * The dwc_otg_driver module provides the initialization and cleanup entry
 * points for the DWC_otg driver. This module will be dynamically installed
 * after Linux is booted using the insmod command. When the module is
 * installed, the dwc_otg_driver_init function is called. When the module is
 * removed (using rmmod), the dwc_otg_driver_cleanup function is called.
 *
 * This module also defines a data structure for the dwc_otg_driver, which is
 * used in conjunction with the standard ARM lm_device structure. These
 * structures allow the OTG driver to comply with the standard Linux driver
 * model in which devices and drivers are registered with a bus driver. This
 * has the benefit that Linux can expose attributes of the driver and device
 * in its special sysfs file system. Users can then read or write files in
 * this file system to perform diagnostics on the driver components or the
 * device.
 */

#include "dwc_otg_os_dep.h"
#include "dwc_os.h"
#include "dwc_otg_dbg.h"
#include "dwc_otg_driver.h"
#include "dwc_otg_attr.h"
#include "dwc_otg_core_if.h"
#include "dwc_otg_cil.h"
#include "dwc_otg_pcd_if.h"
#include "dwc_otg_hcd_if.h"
#include "dwc_otg_pcd.h"

#include <linux/of_platform.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/amlogic/usbtype.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/workqueue.h>
#include <linux/amlogic/cpu_version.h>
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif

#define DWC_DRIVER_VERSION	"3.10a 12-MAY-2014"
#define DWC_DRIVER_DESC		"HS OTG USB Controller driver"

static const char dwc_driver_name[] = "dwc_otg";
extern int pcd_init(struct platform_device *pdev);
extern int hcd_init(struct platform_device *pdev);
extern int pcd_remove(struct platform_device *pdev);
extern void hcd_remove(struct platform_device *pdev);

extern void dwc_otg_adp_start(dwc_otg_core_if_t *core_if, uint8_t is_host);

dwc_otg_device_t *g_dwc_otg_device[2];
EXPORT_SYMBOL_GPL(g_dwc_otg_device);

static struct platform_driver dwc_otg_driver;
extern uint32_t g_dbg_lvl;

struct device *usbdev;

/*-------------------------------------------------------------------------*/
/* Encapsulate the module parameter settings */

struct dwc_otg_driver_module_params {
	int32_t opt;
	int32_t otg_cap;
	int32_t dma_enable;
	int32_t dma_desc_enable;
	int32_t dma_burst_size;
	int32_t speed;
	int32_t host_support_fs_ls_low_power;
	int32_t host_ls_low_power_phy_clk;
	int32_t enable_dynamic_fifo;
	int32_t data_fifo_size;
	int32_t dev_rx_fifo_size;
	int32_t dev_nperio_tx_fifo_size;
	uint32_t dev_perio_tx_fifo_size[MAX_PERIO_FIFOS];
	int32_t host_rx_fifo_size;
	int32_t host_nperio_tx_fifo_size;
	int32_t host_perio_tx_fifo_size;
	int32_t max_transfer_size;
	int32_t max_packet_count;
	int32_t host_channels;
	int32_t dev_endpoints;
	int32_t phy_type;
	int32_t phy_utmi_width;
	int32_t phy_ulpi_ddr;
	int32_t phy_ulpi_ext_vbus;
	int32_t i2c_enable;
	int32_t ulpi_fs_ls;
	int32_t ts_dline;
	int32_t en_multiple_tx_fifo;
	uint32_t dev_tx_fifo_size[MAX_TX_FIFOS];
	uint32_t thr_ctl;
	uint32_t tx_thr_length;
	uint32_t rx_thr_length;
	int32_t pti_enable;
	int32_t mpi_enable;
	int32_t lpm_enable;
	int32_t besl_enable;
	int32_t baseline_besl;
	int32_t deep_besl;
	int32_t ic_usb_cap;
	int32_t ahb_thr_ratio;
	int32_t power_down;
	int32_t reload_ctl;
	int32_t dev_out_nak;
	int32_t cont_on_bna;
	int32_t ahb_single;
	int32_t otg_ver;
	int32_t adp_enable;
	int32_t host_only;
};

static struct dwc_otg_driver_module_params dwc_otg_module_params = {
	.opt = -1,
	.otg_cap = DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE,
	.dma_enable = -1,
	.dma_desc_enable = 0,
	.dma_burst_size = -1,
	.speed = -1,
	.host_support_fs_ls_low_power = -1,
	.host_ls_low_power_phy_clk = -1,
	.enable_dynamic_fifo = 1,
	.data_fifo_size = 1024, /* will be overrided by platform setting */
	.dev_endpoints = -1,
	.en_multiple_tx_fifo = -1,
	.dev_rx_fifo_size = 256,
	.dev_nperio_tx_fifo_size = 256,
	.dev_tx_fifo_size = {
			     /* dev_tx_fifo_size */
			     256,
			     256,
			     128,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1
			     /* 15 */
			     },
	.dev_perio_tx_fifo_size = {
				   /* dev_perio_tx_fifo_size_1 */
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1
				   /* 15 */
				   },
	.host_rx_fifo_size = 512, /* will be overrided by platform setting */
	.host_nperio_tx_fifo_size = 500,
	.host_perio_tx_fifo_size = 500,
	.max_transfer_size = -1,
	.max_packet_count = -1,
	.host_channels = 16,
	.phy_type = -1,
	.phy_utmi_width = -1,
	.phy_ulpi_ddr = -1,
	.phy_ulpi_ext_vbus = -1,
	.i2c_enable = -1,
	.ulpi_fs_ls = -1,
	.ts_dline = -1,
	.thr_ctl = -1,
	.tx_thr_length = -1,
	.rx_thr_length = -1,
	.pti_enable = -1,
	.mpi_enable = -1,
	.lpm_enable = -1,
	.besl_enable = -1,
	.baseline_besl = -1,
	.deep_besl = -1,
	.ic_usb_cap = -1,
	.ahb_thr_ratio = -1,
	.power_down = -1,
	.reload_ctl = 1,
	.dev_out_nak = -1,
	.cont_on_bna = -1,
	.ahb_single = 1,
	.otg_ver = -1,
	.adp_enable = -1,
};

bool force_device_mode;
module_param_named(otg_device, force_device_mode,
		bool, S_IRUGO | S_IWUSR);

static char otg_mode_string[2] = "0";
static int __init force_otg_mode(char *s)
{
	if (s != NULL)
		sprintf(otg_mode_string, "%s", s);
	if (strcmp(otg_mode_string, "0") == 0)
		force_device_mode = 0;
	else
		force_device_mode = 1;
	return 0;
}
__setup("otg_device=", force_otg_mode);

static u64 dwc2_dmamask = DMA_BIT_MASK(32);

/**
  *  Index-name refer to lm.h usb_dma_config_e
  */
static const char *dma_config_name[] = {
	"BURST_DEFAULT",
	"BURST_SINGLE",
	"BURST_INCR",
	"BURST_INCR4",
	"BURST_INCR8",
	"BURST_INCR16",
	"DISABLE",
};

/**
 * This function is called during module intialization
 * to pass module parameters to the DWC_OTG CORE.
 */
static int set_parameters(dwc_otg_core_if_t *core_if)
{
	int retval = 0;
	int i;

	if (dwc_otg_module_params.otg_cap != -1)
		retval +=
		    dwc_otg_set_param_otg_cap(core_if,
					      dwc_otg_module_params.otg_cap);

	if (dwc_otg_module_params.dma_enable != -1)
		retval +=
		    dwc_otg_set_param_dma_enable(core_if,
						 dwc_otg_module_params.
						 dma_enable);

	if (dwc_otg_module_params.dma_desc_enable != -1)
		retval +=
		    dwc_otg_set_param_dma_desc_enable(core_if,
						      dwc_otg_module_params.
						      dma_desc_enable);

	if (dwc_otg_module_params.opt != -1)
		retval +=
		    dwc_otg_set_param_opt(core_if, dwc_otg_module_params.opt);

	if (dwc_otg_module_params.dma_burst_size != -1)
		retval +=
		    dwc_otg_set_param_dma_burst_size(core_if,
						     dwc_otg_module_params.
						     dma_burst_size);

	if (dwc_otg_module_params.host_support_fs_ls_low_power != -1)
		retval +=
		    dwc_otg_set_param_host_support_fs_ls_low_power(core_if,
								   dwc_otg_module_params.
								   host_support_fs_ls_low_power);

	if (dwc_otg_module_params.enable_dynamic_fifo != -1)
		retval +=
		    dwc_otg_set_param_enable_dynamic_fifo(core_if,
							  dwc_otg_module_params.
							  enable_dynamic_fifo);

	if (dwc_otg_module_params.data_fifo_size != -1)
		retval +=
		    dwc_otg_set_param_data_fifo_size(core_if,
						     dwc_otg_module_params.
						     data_fifo_size);

	if (dwc_otg_module_params.dev_rx_fifo_size != -1)
		retval +=
		    dwc_otg_set_param_dev_rx_fifo_size(core_if,
						       dwc_otg_module_params.
						       dev_rx_fifo_size);

	if (dwc_otg_module_params.dev_nperio_tx_fifo_size != -1)
		retval +=
		    dwc_otg_set_param_dev_nperio_tx_fifo_size(core_if,
							      dwc_otg_module_params.
							      dev_nperio_tx_fifo_size);

	if (dwc_otg_module_params.host_rx_fifo_size != -1)
		retval +=
		    dwc_otg_set_param_host_rx_fifo_size(core_if,
							dwc_otg_module_params.host_rx_fifo_size);

	if (dwc_otg_module_params.host_nperio_tx_fifo_size != -1)
		retval +=
		    dwc_otg_set_param_host_nperio_tx_fifo_size(core_if,
							       dwc_otg_module_params.
							       host_nperio_tx_fifo_size);

	if (dwc_otg_module_params.host_perio_tx_fifo_size != -1)
		retval +=
		    dwc_otg_set_param_host_perio_tx_fifo_size(core_if,
							      dwc_otg_module_params.
							      host_perio_tx_fifo_size);

	if (dwc_otg_module_params.max_transfer_size != -1)
		retval +=
		    dwc_otg_set_param_max_transfer_size(core_if,
							dwc_otg_module_params.
							max_transfer_size);

	if (dwc_otg_module_params.max_packet_count != -1)
		retval +=
		    dwc_otg_set_param_max_packet_count(core_if,
						       dwc_otg_module_params.
						       max_packet_count);

	if (dwc_otg_module_params.host_channels != -1)
		retval +=
		    dwc_otg_set_param_host_channels(core_if,
						    dwc_otg_module_params.
						    host_channels);

	if (dwc_otg_module_params.dev_endpoints != -1)
		retval +=
		    dwc_otg_set_param_dev_endpoints(core_if,
						    dwc_otg_module_params.
						    dev_endpoints);

	if (dwc_otg_module_params.phy_type != -1)
		retval +=
		    dwc_otg_set_param_phy_type(core_if,
					       dwc_otg_module_params.phy_type);

	if (dwc_otg_module_params.speed != -1)
		retval +=
		    dwc_otg_set_param_speed(core_if,
					    dwc_otg_module_params.speed);

	if (dwc_otg_module_params.host_ls_low_power_phy_clk != -1)
		retval +=
		    dwc_otg_set_param_host_ls_low_power_phy_clk(core_if,
								dwc_otg_module_params.
								host_ls_low_power_phy_clk);

	if (dwc_otg_module_params.phy_ulpi_ddr != -1)
		retval +=
		    dwc_otg_set_param_phy_ulpi_ddr(core_if,
						   dwc_otg_module_params.
						   phy_ulpi_ddr);

	if (dwc_otg_module_params.phy_ulpi_ext_vbus != -1)
		retval +=
		    dwc_otg_set_param_phy_ulpi_ext_vbus(core_if,
							dwc_otg_module_params.
							phy_ulpi_ext_vbus);

	if (dwc_otg_module_params.phy_utmi_width != -1)
		retval +=
		    dwc_otg_set_param_phy_utmi_width(core_if,
						     dwc_otg_module_params.
						     phy_utmi_width);

	if (dwc_otg_module_params.ulpi_fs_ls != -1)
		retval +=
		    dwc_otg_set_param_ulpi_fs_ls(core_if,
						 dwc_otg_module_params.ulpi_fs_ls);

	if (dwc_otg_module_params.ts_dline != -1)
		retval +=
		    dwc_otg_set_param_ts_dline(core_if,
					       dwc_otg_module_params.ts_dline);

	if (dwc_otg_module_params.i2c_enable != -1)
		retval +=
		    dwc_otg_set_param_i2c_enable(core_if,
						 dwc_otg_module_params.
						 i2c_enable);

	if (dwc_otg_module_params.en_multiple_tx_fifo != -1)
		retval +=
		    dwc_otg_set_param_en_multiple_tx_fifo(core_if,
							  dwc_otg_module_params.
							  en_multiple_tx_fifo);

	if (!dwc_otg_module_params.host_only) {
		for (i = 0; i < 15; i++) {
			if (dwc_otg_module_params.dev_perio_tx_fifo_size[i] != -1)
				retval +=
				    dwc_otg_set_param_dev_perio_tx_fifo_size(core_if,
									     dwc_otg_module_params.
									     dev_perio_tx_fifo_size
									     [i], i);
		}

		for (i = 0; i < 15; i++) {
			if (dwc_otg_module_params.dev_tx_fifo_size[i] != -1)
				retval += dwc_otg_set_param_dev_tx_fifo_size(core_if,
									     dwc_otg_module_params.
									     dev_tx_fifo_size
									     [i], i);
		}
	}
	if (dwc_otg_module_params.thr_ctl != -1)
		retval +=
		    dwc_otg_set_param_thr_ctl(core_if,
					      dwc_otg_module_params.thr_ctl);

	if (dwc_otg_module_params.mpi_enable != -1)
		retval +=
		    dwc_otg_set_param_mpi_enable(core_if,
						 dwc_otg_module_params.
						 mpi_enable);

	if (dwc_otg_module_params.pti_enable != -1)
		retval +=
		    dwc_otg_set_param_pti_enable(core_if,
						 dwc_otg_module_params.
						 pti_enable);

	if (dwc_otg_module_params.lpm_enable != -1)
		retval +=
		    dwc_otg_set_param_lpm_enable(core_if,
						 dwc_otg_module_params.
						 lpm_enable);

	if (dwc_otg_module_params.besl_enable != -1)
		retval +=
		    dwc_otg_set_param_besl_enable(core_if,
						 dwc_otg_module_params.
						 besl_enable);

	if (dwc_otg_module_params.baseline_besl != -1)
		retval +=
		    dwc_otg_set_param_baseline_besl(core_if,
						 dwc_otg_module_params.
						 baseline_besl);

	if (dwc_otg_module_params.deep_besl != -1)
		retval +=
		    dwc_otg_set_param_deep_besl(core_if,
						 dwc_otg_module_params.
						 deep_besl);

	if (dwc_otg_module_params.ic_usb_cap != -1)
		retval +=
		    dwc_otg_set_param_ic_usb_cap(core_if,
						 dwc_otg_module_params.
						 ic_usb_cap);

	if (dwc_otg_module_params.tx_thr_length != -1)
		retval +=
		    dwc_otg_set_param_tx_thr_length(core_if,
						    dwc_otg_module_params.tx_thr_length);

	if (dwc_otg_module_params.rx_thr_length != -1)
		retval +=
		    dwc_otg_set_param_rx_thr_length(core_if,
						    dwc_otg_module_params.
						    rx_thr_length);

	if (dwc_otg_module_params.ahb_thr_ratio != -1)
		retval +=
		    dwc_otg_set_param_ahb_thr_ratio(core_if,
						    dwc_otg_module_params.ahb_thr_ratio);

	if (dwc_otg_module_params.power_down != -1)
		retval +=
		    dwc_otg_set_param_power_down(core_if,
						 dwc_otg_module_params.power_down);

	if (dwc_otg_module_params.reload_ctl != -1)
		retval +=
		    dwc_otg_set_param_reload_ctl(core_if,
						 dwc_otg_module_params.reload_ctl);

	if (dwc_otg_module_params.dev_out_nak != -1)
		retval +=
			dwc_otg_set_param_dev_out_nak(core_if,
			dwc_otg_module_params.dev_out_nak);

	if (dwc_otg_module_params.cont_on_bna != -1)
		retval +=
			dwc_otg_set_param_cont_on_bna(core_if,
			dwc_otg_module_params.cont_on_bna);

	if (dwc_otg_module_params.ahb_single != -1)
		retval +=
			dwc_otg_set_param_ahb_single(core_if,
			dwc_otg_module_params.ahb_single);

	if (dwc_otg_module_params.otg_ver != -1)
		retval +=
		    dwc_otg_set_param_otg_ver(core_if,
					      dwc_otg_module_params.otg_ver);

	if (dwc_otg_module_params.adp_enable != -1)
		retval +=
		    dwc_otg_set_param_adp_enable(core_if,
						 dwc_otg_module_params.
						 adp_enable);

	return retval;
}

/***************************************************
			notifier
****************************************************/
static BLOCKING_NOTIFIER_HEAD(dwc_otg_power_notifier_list);

int dwc_otg_power_register_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_register(&dwc_otg_power_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(dwc_otg_power_register_notifier);

int dwc_otg_power_unregister_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_unregister(&dwc_otg_power_notifier_list, nb);
	return ret;
}
EXPORT_SYMBOL(dwc_otg_power_unregister_notifier);

void dwc_otg_power_notifier_call(char is_power_on)
{
	blocking_notifier_call_chain(&dwc_otg_power_notifier_list, is_power_on, NULL);
}

static BLOCKING_NOTIFIER_HEAD(dwc_otg_charger_detect_notifier_list);

int dwc_otg_charger_detect_register_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_register(&dwc_otg_charger_detect_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(dwc_otg_charger_detect_register_notifier);

int dwc_otg_charger_detect_unregister_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_unregister(&dwc_otg_charger_detect_notifier_list, nb);
	return ret;
}
EXPORT_SYMBOL(dwc_otg_charger_detect_unregister_notifier);

void dwc_otg_charger_detect_notifier_call(int bc_mode)
{
	blocking_notifier_call_chain(&dwc_otg_charger_detect_notifier_list, bc_mode, NULL);
}

static void amlogic_device_detect_work(struct work_struct *work)
{
	dwc_otg_device_t *dwc_otg_device =
		container_of(work, dwc_otg_device_t, work.work);
	int ret;

	if (USB_OTG == dwc_otg_device->core_if->controller_type) {
		if (dwc_otg_device->core_if->phy_interface == 1)
			ret = device_status((unsigned long)dwc_otg_device->
					core_if->usb_peri_reg);
		else
			ret = device_status_v2((unsigned long)dwc_otg_device->
					core_if->usb_peri_reg);
		if (!ret) {
			DWC_PRINTF("usb device plug out, stop pcd!!!\n");
			if (dwc_otg_device->pcd->core_if->pcd_cb->stop)
				dwc_otg_device->pcd->core_if->
					pcd_cb->stop(dwc_otg_device->pcd);
		} else {
			schedule_delayed_work(&dwc_otg_device->work,
				msecs_to_jiffies(100));
		}
	}

	return;
}

#define FORCE_ID_CLEAR	-1
#define FORCE_ID_HOST	0
#define FORCE_ID_SLAVE	1
#define FORCE_ID_ERROR	2
static void dwc_otg_set_force_id(dwc_otg_core_if_t *core_if, int mode)
{
	gusbcfg_data_t gusbcfg_data;

	gusbcfg_data.d32 = DWC_READ_REG32(&core_if->core_global_regs->gusbcfg);
	switch (mode) {
	case FORCE_ID_CLEAR:
		gusbcfg_data.b.force_host_mode = 0;
		gusbcfg_data.b.force_dev_mode = 0;
		break;
	case FORCE_ID_HOST:
		gusbcfg_data.b.force_host_mode = 1;
		gusbcfg_data.b.force_dev_mode = 0;
		break;
	case FORCE_ID_SLAVE:
		gusbcfg_data.b.force_host_mode = 0;
		gusbcfg_data.b.force_dev_mode = 1;
		break;
	default:
		DWC_ERROR("error id mode\n");
		return;
		break;
	}
	DWC_WRITE_REG32(&core_if->core_global_regs->gusbcfg, gusbcfg_data.d32);
	return;
}

#define VBUS_POWER_GPIO_OWNER  "DWC_OTG"
void set_usb_vbus_power(struct gpio_desc *usb_gd, int pin, char is_power_on)
{
	if (is_power_on) {
		/*notify pmu off vbus first*/
	    dwc_otg_power_notifier_call(is_power_on);
		DWC_DEBUG("set usb port power on (board gpio %d)!\n", pin);
		if (pin != -2)
			/*set vbus on by gpio*/
			gpiod_direction_output(usb_gd, is_power_on);
	} else {
		DWC_DEBUG("set usb port power off (board gpio %d)!\n", pin);
		if (pin != -2)
			/*set vbus off by gpio first*/
			gpiod_direction_output(usb_gd, is_power_on);
		/*notify pmu on vbus*/
		dwc_otg_power_notifier_call(is_power_on);
	}
}

static void dwc_otg_id_change_timer_handler(void *parg)
{

	dwc_otg_device_t *otg_dev = (dwc_otg_device_t *)parg;
	usb_peri_reg_t *phy_peri = (usb_peri_reg_t *)otg_dev->core_if->usb_peri_reg;
	usb_adp_bc_data_t adp_bc;
	unsigned long flags;

	local_irq_save(flags);

	adp_bc.d32 = phy_peri->adp_bc;
	if (adp_bc.b.iddig)
		dwc_otg_set_force_id(otg_dev->core_if, FORCE_ID_SLAVE);
	else
		dwc_otg_set_force_id(otg_dev->core_if, FORCE_ID_HOST);

	DWC_TIMER_SCHEDULE(otg_dev->id_change_timer, 100);

	local_irq_restore(flags);
	return;
}
/**
 * This function is the top level interrupt handler for the Common
 * (Device and host modes) interrupts.
 */
static irqreturn_t dwc_otg_common_irq(int irq, void *dev)
{
	int32_t retval = IRQ_NONE;

	retval = dwc_otg_handle_common_intr(dev);
	if (retval != 0)
		S3C2410X_CLEAR_EINTPEND();

	return IRQ_RETVAL(retval);
}

/**
 * This function is called when a lm_device is unregistered with the
 * dwc_otg_driver. This happens, for example, when the rmmod command is
 * executed. The device may or may not be electrically present. If it is
 * present, the driver stops device processing. Any resources used on behalf
 * of this device are freed.
 *
 * @param _dev
 */
static int dwc_otg_driver_remove(struct platform_device *pdev)
{
	dwc_otg_device_t *otg_dev = g_dwc_otg_device[pdev->id];
	int irq = 0;

	DWC_DEBUGPL(DBG_ANY, "%s(%p)\n", __func__, pdev);

	if (!otg_dev) {
		/* Memory allocation for the dwc_otg_device failed. */
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev NULL!\n", __func__);
		return -EINVAL;
	}
#ifndef DWC_DEVICE_ONLY
	if (otg_dev->hcd) {
		hcd_remove(pdev);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->hcd NULL!\n", __func__);
		return -EINVAL;
	}
#endif

#ifndef DWC_HOST_ONLY
	if (otg_dev->pcd) {
		pcd_remove(pdev);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->pcd NULL!\n", __func__);
		return -EINVAL;
	}
#endif
	/*
	 * Free the IRQ
	 */
	if (otg_dev->common_irq_installed) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0)
			return -ENODEV;
		free_irq(irq, otg_dev);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: There is no installed irq!\n", __func__);
		return -EINVAL;
	}

	if (otg_dev->core_if) {
		if (otg_dev->core_if->vbus_power_pin != -2)
			gpiod_put(otg_dev->core_if->usb_gpio_desc);
		dwc_otg_cil_remove(otg_dev->core_if);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->core_if NULL!\n", __func__);
		return -EINVAL;
	}

	if (otg_dev->id_change_timer)
		DWC_TIMER_FREE(otg_dev->id_change_timer);

	/*
	 * Remove the device attributes
	 */
	dwc_otg_attr_remove(pdev);

	/*
	 * Return the memory.
	 */
	if (otg_dev->os_dep.base) {
		iounmap(otg_dev->os_dep.base);
	}
	DWC_FREE(otg_dev);

	/*
	 * Clear the drvdata pointer.
	 */
	platform_set_drvdata(pdev, 0);

	return 0;
}

static void dwc_otg_driver_shutdown(struct platform_device *pdev)
{
#if 0
	dwc_otg_device_t *otg_dev = g_dwc_otg_device[pdev->id];
	int irq = 0;
	const char *s_clock_name = NULL;
	const char *cpu_type = NULL;
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	DWC_DEBUGPL(DBG_ANY, "%s(%p)\n", __func__, pdev);

	/*
	* Remove the device attributes
	*/
	dwc_otg_attr_remove(pdev);

	if (hcd)
		if (hcd->driver->shutdown)
			hcd->driver->shutdown(hcd);

	if (!otg_dev) {
		/* Memory allocation for the dwc_otg_device failed. */
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev NULL!\n", __func__);
		return;
	}
#ifndef DWC_DEVICE_ONLY
	if (otg_dev->hcd)
		hcd_remove(pdev);
	else
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->hcd NULL!\n", __func__);

#endif

#ifndef DWC_HOST_ONLY
	if (otg_dev->pcd)
		pcd_remove(pdev);
	else
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->pcd NULL!\n", __func__);

#endif
	/*
	* Free the IRQ
	*/
	if (otg_dev->common_irq_installed) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0)
			return;
		free_irq(irq, otg_dev);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: no installed irq!\n", __func__);
		return;
	}

	if (otg_dev->core_if) {
		if (otg_dev->core_if->vbus_power_pin != -2)
			gpiod_put(otg_dev->core_if->usb_gpio_desc);
		dwc_otg_cil_remove(otg_dev->core_if);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->core_if NULL!\n", __func__);
		return;
	}

	if (otg_dev->id_change_timer)
		DWC_TIMER_FREE(otg_dev->id_change_timer);

	s_clock_name = of_get_property(pdev->dev.of_node, "clock-src", NULL);
	cpu_type = of_get_property(pdev->dev.of_node, "cpu-type", NULL);

	clk_suspend_usb(pdev, s_clock_name,
			(unsigned long)(otg_dev->
				core_if->usb_peri_reg), cpu_type);

	/*
	* Return the memory.
	*/
	if (otg_dev->os_dep.base)
		iounmap(otg_dev->os_dep.base);

	DWC_FREE(otg_dev);
	otg_dev = NULL;

	/*
	* Clear the drvdata pointer.
	*/
	platform_set_drvdata(pdev, 0);
#endif
	return;
}
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
extern int get_pcd_ums_state(dwc_otg_pcd_t *pcd);
static void usb_early_suspend(struct early_suspend *h)
{
	int is_mount = 0;
	dwc_otg_device_t *dwc_otg_device;
	dwc_otg_device = (dwc_otg_device_t *)h->param;
	is_mount = get_pcd_ums_state(dwc_otg_device->pcd);
	DWC_DEBUG("DWC_OTG: going early suspend! is_mount=%d\n", is_mount);
	if (dwc_otg_device->core_if->controller_type == 0) {
	if (dwc_otg_is_device_mode(dwc_otg_device->core_if) && !is_mount)
		DWC_MODIFY_REG32(&dwc_otg_device->core_if->dev_if->dev_global_regs->dctl, 0, 2);
	}
}
static void usb_early_resume(struct early_suspend *h)
{
	dwc_otg_device_t *dwc_otg_device;
	DWC_DEBUG("DWC_OTG: going early resume\n");
	dwc_otg_device = (dwc_otg_device_t *)h->param;
	if (dwc_otg_device->core_if->controller_type == 0) {
	if (dwc_otg_is_device_mode(dwc_otg_device->core_if))
		DWC_MODIFY_REG32(&dwc_otg_device->core_if->dev_if->dev_global_regs->dctl, 2, 0);
	}
}
#endif
static const struct of_device_id dwc_otg_dt_match[] = {
	{	.compatible	= "amlogic, dwc2",
	},
	{	.compatible	= "amlogic,dwc2",
	},
	{},
};

/**
 * This function is called when an lm_device is bound to a
 * dwc_otg_driver. It creates the driver components required to
 * control the device (CIL, HCD, and PCD) and it initializes the
 * device. The driver components are stored in a dwc_otg_device
 * structure. A reference to the dwc_otg_device is saved in the
 * lm_device. This allows the driver to access the dwc_otg_device
 * structure on subsequent calls to driver methods for this device.
 *
 * @param _dev Bus device
 */
static int dwc_otg_driver_probe(struct platform_device *pdev)
{
	int retval = 0;
	int port_index = 0;
	int port_type = USB_PORT_TYPE_OTG;
	int id_mode = USB_PHY_ID_MODE_HW;
	int port_speed = USB_PORT_SPEED_DEFAULT;
	int port_config = 0;
	int dma_config = USB_DMA_BURST_DEFAULT;
	int gpio_work_mask = 1;
	int gpio_vbus_power_pin = -1;
	int charger_detect = 0;
	int non_normal_usb_charger_detect_delay = 0;
	int host_only_core = 0;
	int pmu_apply_power = 0;
	int	irq = 0;
	void __iomem *phy_reg_addr = NULL;
	void __iomem *ctrl_reg_addr = NULL;
	struct resource	*res = NULL;
	unsigned int p_phy_reg_addr = 0;
	unsigned int p_ctrl_reg_addr = 0;
	unsigned int phy_reg_addr_size = 0;
	unsigned int phy_interface = 1;
	const char *s_clock_name = NULL;
	const char *cpu_type = NULL;
	const char *gpio_name = NULL;
	const void *prop;
	dwc_otg_device_t *dwc_otg_device;
	struct gpio_desc *usb_gd = NULL;
	struct dwc_otg_driver_module_params *pcore_para;
	static int dcount;
	int controller_type = USB_NORMAL;

	dev_dbg(&pdev->dev, "dwc_otg_driver_probe(%p)\n", pdev);

	if (dcount == 0) {
		dcount++;
		usbdev = (struct device *)&pdev->dev;
	}

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct device_node	*of_node = pdev->dev.of_node;
		match = of_match_node(dwc_otg_dt_match, of_node);
		if (match) {
			s_clock_name = of_get_property(of_node, "clock-src", NULL);
			cpu_type = of_get_property(of_node, "cpu-type", NULL);
			prop = of_get_property(of_node, "port-id", NULL);
			if (prop) {
				port_index = of_read_ulong(prop, 1);
				pdev->id = port_index;
			}
			prop = of_get_property(of_node,
				"controller-type", NULL);
			if (prop)
				controller_type = of_read_ulong(prop, 1);
			prop = of_get_property(of_node, "port-type", NULL);
			if (prop)
				port_type = of_read_ulong(prop, 1);
			prop = of_get_property(of_node, "port-speed", NULL);
			if (prop)
				port_speed = of_read_ulong(prop, 1);
			prop = of_get_property(of_node, "port-config", NULL);
			if (prop)
				port_config = of_read_ulong(prop, 1);
			prop = of_get_property(of_node, "port-dma", NULL);
			if (prop)
				dma_config = of_read_ulong(prop, 1);
			prop = of_get_property(of_node, "port-id-mode", NULL);
			if (prop)
				id_mode = of_read_ulong(prop, 1);

			prop = of_get_property(of_node, "charger_detect", NULL);
			if (prop)
				charger_detect = of_read_ulong(prop, 1);

			prop = of_get_property(of_node, "non_normal_usb_charger_detect_delay", NULL);
			if (prop)
				non_normal_usb_charger_detect_delay = of_read_ulong(prop, 1);

			gpio_name = of_get_property(of_node, "gpio-vbus-power", NULL);
			if (gpio_name) {
				if (strcmp(gpio_name, "PMU") == 0) {
					gpio_vbus_power_pin = -2;
				} else {
					gpio_vbus_power_pin = 1;
					usb_gd = gpiod_get_index(&pdev->dev,
								 NULL, 0, GPIOD_OUT_LOW);
					if (IS_ERR(usb_gd))
						return -1;
				}
				prop = of_get_property(of_node, "gpio-work-mask", NULL);
				if (prop)
					gpio_work_mask = of_read_ulong(prop, 1);
			}

			prop = of_get_property(of_node, "host-only-core", NULL);
			if (prop)
				host_only_core = of_read_ulong(prop, 1);

			prop = of_get_property(of_node, "pmu-apply-power", NULL);
			if (prop)
				pmu_apply_power = of_read_ulong(prop, 1);

			retval = of_property_read_u32(of_node, "phy-reg", &p_phy_reg_addr);
			if (retval < 0)
				return -EINVAL;

			retval = of_property_read_u32(of_node, "phy-reg-size", &phy_reg_addr_size);
			if (retval < 0)
				return -EINVAL;

			phy_reg_addr = devm_ioremap_nocache(&(pdev->dev), (resource_size_t)p_phy_reg_addr, (unsigned long)phy_reg_addr_size);
			if (!phy_reg_addr)
					return -ENOMEM;

			retval = of_property_read_u32(of_node, "usb-fifo", &dwc_otg_module_params.data_fifo_size);
			if (retval < 0)
				return -EINVAL;

			prop = of_get_property(of_node, "phy-interface", NULL);
			if (prop)
				phy_interface = of_read_ulong(prop, 1);

			if (is_meson_g12b_cpu()) {
				if (!is_meson_rev_a())
					phy_interface = 2;
			}

			dwc_otg_module_params.host_rx_fifo_size = dwc_otg_module_params.data_fifo_size / 2;
			DWC_PRINTF("dwc_otg: %s: type: %d speed: %d, ",
				s_clock_name, port_type, port_speed);
			DWC_PRINTF("config: %d, dma: %d, id: %d, ",
				port_config, dma_config, id_mode);
			DWC_PRINTF("phy: %x, ctrl: %x\n",
				p_phy_reg_addr, p_ctrl_reg_addr);


		} else {
			DWC_PRINTF("%s NOT match\n", __func__);
			return -ENODEV;
		}
	}

	if (controller_type == USB_HOST_ONLY && !force_device_mode) {
		DWC_PRINTF("%s host only, not probe usb_otg!!!\n", __func__);
		return -ENODEV;
	}

	dwc_otg_device = DWC_ALLOC(sizeof(dwc_otg_device_t));

	if (!dwc_otg_device) {
		dev_err(&pdev->dev, "kmalloc of dwc_otg_device failed\n");
		return -ENOMEM;
	}

	memset(dwc_otg_device, 0, sizeof(*dwc_otg_device));
	dwc_otg_device->os_dep.reg_offset = 0xFFFFFFFF;

	/*
	* Map the DWC_otg Core memory into virtual address space.
	*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory resource\n");
		DWC_FREE(dwc_otg_device);
		return -ENODEV;
	}

	ctrl_reg_addr = devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
	if (!ctrl_reg_addr) {
		dev_err(&pdev->dev, "ioremap failed\n");
		DWC_FREE(dwc_otg_device);
		return -ENOMEM;
	}

	dwc_otg_device->os_dep.base = (void *)ctrl_reg_addr;

	if (!dwc_otg_device->os_dep.base) {
		dev_err(&pdev->dev, "ioremap() failed\n");
		DWC_FREE(dwc_otg_device);
		return -ENOMEM;
	}
	dev_dbg(&pdev->dev, "base=0x%08lx\n",
		(unsigned long)dwc_otg_device->os_dep.base);

	/*
	 * Initialize driver data to point to the global DWC_otg
	 * Device structure.
	 */
	g_dwc_otg_device[pdev->id] = dwc_otg_device;

	dwc_otg_device->os_dep.pldev = pdev;
	dwc_otg_device->gen_dev = &pdev->dev;

	dwc_otg_device->dev_name = dev_name(dwc_otg_device->gen_dev);

	pcore_para = &dwc_otg_module_params;

	if (force_device_mode && (port_index == 0))
		port_type = USB_PORT_TYPE_SLAVE;

	if (port_type == USB_PORT_TYPE_HOST)
		pcore_para->host_only = 1;
	else
		pcore_para->host_only = 0;

	dev_dbg(&pdev->dev, "dwc_otg_device=0x%p\n", dwc_otg_device);

	if (clk_enable_usb(pdev, s_clock_name,
		(unsigned long)phy_reg_addr, cpu_type, controller_type)) {
		dev_err(&pdev->dev, "Set dwc_otg PHY clock failed!\n");
		DWC_FREE(dwc_otg_device);
		return -ENODEV;
	}

	dwc_otg_device->core_if = dwc_otg_cil_init(dwc_otg_device->os_dep.base,
		pcore_para->host_only);
	if (!dwc_otg_device->core_if) {
		dev_err(&pdev->dev, "CIL initialization failed!\n");
		retval = -ENOMEM;
		goto fail;
	}

	dwc_otg_device->core_if->usb_peri_reg = (usb_peri_reg_t *)phy_reg_addr;
	dwc_otg_device->core_if->controller_type = controller_type;
	dwc_otg_device->core_if->phy_interface = phy_interface;
	/*
	* Attempt to ensure this device is really a DWC_otg Controller.
	* Read and verify the SNPSID register contents. The value should be
	* 0x45F42XXX or 0x45F42XXX, which corresponds to either "OT2" or "OTG3",
	* as in "OTG version 2.XX" or "OTG version 3.XX".
	*/

	if (((dwc_otg_get_gsnpsid(dwc_otg_device->core_if) & 0xFFFFF000) !=	0x4F542000) &&
		((dwc_otg_get_gsnpsid(dwc_otg_device->core_if) & 0xFFFFF000) != 0x4F543000)) {
			dev_err(&pdev->dev, "Bad value for SNPSID: 0x%08x\n",
				dwc_otg_get_gsnpsid(dwc_otg_device->core_if));
			retval = -EINVAL;
			goto fail;
	}

	dev_dbg(&pdev->dev, "DMA config: %s\n", dma_config_name[dma_config]);

	if (dma_config == USB_DMA_DISABLE) {
		pcore_para->dma_enable = 0;
		pdev->dev.coherent_dma_mask = 0;
		pdev->dev.dma_mask = 0;
	} else {
		pdev->dev.dma_mask = &dwc2_dmamask;
		pdev->dev.coherent_dma_mask = *pdev->dev.dma_mask;
		switch (dma_config) {
		case USB_DMA_BURST_INCR:
			pcore_para->dma_burst_size =
			    DWC_GAHBCFG_INT_DMA_BURST_INCR;
			break;
		case USB_DMA_BURST_INCR4:
			pcore_para->dma_burst_size =
			    DWC_GAHBCFG_INT_DMA_BURST_INCR4;
			break;
		case USB_DMA_BURST_INCR8:
			pcore_para->dma_burst_size =
			    DWC_GAHBCFG_INT_DMA_BURST_INCR8;
			break;
		case USB_DMA_BURST_INCR16:
			pcore_para->dma_burst_size =
			    DWC_GAHBCFG_INT_DMA_BURST_INCR16;
			break;
		case USB_DMA_BURST_SINGLE:
			pcore_para->dma_burst_size =
			    DWC_GAHBCFG_INT_DMA_BURST_SINGLE;
			break;
		default:
			pcore_para->dma_burst_size =
			    DWC_GAHBCFG_INT_DMA_BURST_INCR8;
			break;
		}
	}



	if (USB_NORMAL != controller_type) {
		if (dwc_otg_module_params.data_fifo_size == 728) {
			dwc_otg_module_params.data_fifo_size = -1;
			dwc_otg_module_params.host_rx_fifo_size = -1;
			dwc_otg_module_params.host_nperio_tx_fifo_size = -1;
			dwc_otg_module_params.host_perio_tx_fifo_size = -1;
			dwc_otg_module_params.host_channels = -1;
			dwc_otg_module_params.dev_rx_fifo_size = 192;
			dwc_otg_module_params.dev_nperio_tx_fifo_size = 128;
			dwc_otg_module_params.dev_tx_fifo_size[0] = 128;
			dwc_otg_module_params.dev_tx_fifo_size[1] = 128;
			dwc_otg_module_params.dev_tx_fifo_size[2] = 128;
			dwc_otg_module_params.dev_tx_fifo_size[3] = 16;
			dwc_otg_module_params.dev_tx_fifo_size[4] = 16;
		} else {
			dwc_otg_module_params.data_fifo_size = -1;
			dwc_otg_module_params.host_rx_fifo_size = -1;
			dwc_otg_module_params.host_nperio_tx_fifo_size = -1;
			dwc_otg_module_params.host_perio_tx_fifo_size = -1;
			dwc_otg_module_params.host_channels = -1;
			dwc_otg_module_params.dev_rx_fifo_size = 164;
			dwc_otg_module_params.dev_nperio_tx_fifo_size = 144;
			dwc_otg_module_params.dev_tx_fifo_size[0] = 144;
			dwc_otg_module_params.dev_tx_fifo_size[1] = -1;
			dwc_otg_module_params.dev_tx_fifo_size[2] = -1;
		}
	}
	/*
	* Validate parameter values.
	*/
	if (set_parameters(dwc_otg_device->core_if)) {
		retval = -EINVAL;
		goto fail;
	}

	/*
	* Create Device Attributes in sysfs
	*/
	dwc_otg_attr_create(pdev);

	/*
	* Disable the global interrupt until all the interrupt
	* handlers are installed.
	*/
	dwc_otg_disable_global_interrupts(dwc_otg_device->core_if);

	/*
	* Install the interrupt handler for the common interrupts before
	* enabling common interrupts in core_init below.
	*/
	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	DWC_DEBUGPL(DBG_CIL, "registering (common) handler for irq%d\n",
		    irq);
	retval = request_irq(irq, dwc_otg_common_irq,
			     IRQF_SHARED | IRQ_LEVEL, "dwc_otg",
			     dwc_otg_device);
	if (retval) {
		DWC_ERROR("request of irq%d failed\n", irq);
		retval = -EBUSY;
		goto fail;
	} else {
		dwc_otg_device->common_irq_installed = 1;
	}

	switch (port_type) {
	case USB_PORT_TYPE_OTG:
		id_mode = FORCE_ID_CLEAR;
		break;
	case USB_PORT_TYPE_HOST:
		id_mode = FORCE_ID_HOST;
		break;
	case USB_PORT_TYPE_SLAVE:
		id_mode = FORCE_ID_SLAVE;
		break;
	default:
		id_mode = FORCE_ID_ERROR;
		break;
	}
	dwc_otg_set_force_id(dwc_otg_device->core_if, id_mode);

	/*
	* Initialize the DWC_otg core.
	*/
	dwc_otg_core_init(dwc_otg_device->core_if);

	/*
	*   Set VBus Power CallBack
	*/
	dwc_otg_device->core_if->vbus_power_pin = gpio_vbus_power_pin;
	dwc_otg_device->core_if->vbus_power_pin_work_mask = gpio_work_mask;
	dwc_otg_device->core_if->usb_gpio_desc = usb_gd;
	dwc_otg_device->core_if->charger_detect = charger_detect;
	dwc_otg_device->core_if->non_normal_usb_charger_detect_delay = non_normal_usb_charger_detect_delay;
	if (host_only_core && pmu_apply_power)
		dwc_otg_device->core_if->swicth_int_reg = 1;

	if (port_type == USB_PORT_TYPE_HOST) {
		/*
		* Initialize the HCD
		*/
		DWC_PRINTF("dwc_otg: Working on port type = HOST\n");
		if (!dwc_otg_is_host_mode(dwc_otg_device->core_if)) {
			DWC_PRINTF
			    ("Chip mode not match! -- Want HOST mode but not.  --\n");
			goto fail;
		}
		retval = hcd_init(pdev);
		if (retval != 0) {
			DWC_ERROR("hcd_init failed\n");
			dwc_otg_device->hcd = NULL;
			goto fail;
		}
	} else if (port_type == USB_PORT_TYPE_SLAVE) {
		/*
		* Initialize the PCD
		*/
		DWC_PRINTF("dwc_otg: Working on port type = SLAVE\n");
		if (!dwc_otg_is_device_mode(dwc_otg_device->core_if)) {
			DWC_ERROR
			    ("Chip mode not match! -- Want Device mode but not.  --\n");
			goto fail;
		}
		retval = pcd_init(pdev);
		if (retval != 0) {
			DWC_ERROR("pcd_init failed\n");
			dwc_otg_device->pcd = NULL;
			goto fail;
		}
	} else if (port_type == USB_PORT_TYPE_OTG) {
		DWC_PRINTF("dwc_otg: Working on port type = OTG\n");
		DWC_PRINTF("dwc_otg: Current port type: %s\n",
		dwc_otg_is_host_mode(dwc_otg_device->core_if)?"HOST":"SLAVE");

		retval = hcd_init(pdev);
		if (retval != 0) {
			DWC_ERROR("hcd_init failed(in otg mode)\n");
			dwc_otg_device->hcd = NULL;
			goto fail;
		}

		retval = pcd_init(pdev);
		if (retval != 0) {
			DWC_ERROR("pcd_init failed(in otg mode)\n");
			dwc_otg_device->pcd = NULL;
			goto fail;
		}
		if (!dwc_otg_get_param_adp_enable(dwc_otg_device->core_if)) {
			DWC_PRINTF("dwc_otg: using timer detect");
			DWC_PRINTF("id change, %p\n",
				dwc_otg_device->core_if);
			dwc_otg_device->id_change_timer = DWC_TIMER_ALLOC("ID change timer",
				dwc_otg_id_change_timer_handler, dwc_otg_device);
			DWC_TIMER_SCHEDULE(dwc_otg_device->id_change_timer, 0);
		}
	} else {
		DWC_ERROR("can't config as right mode\n");
		goto fail;
	}

	dwc_otg_save_global_regs(dwc_otg_device->core_if);

	INIT_DELAYED_WORK(&dwc_otg_device->work, amlogic_device_detect_work);

	/*
	 * Enable the global interrupt after all the interrupt
	 * handlers are installed if there is no ADP support else
	 * perform initial actions required for Internal ADP logic.
	 */

	if (port_type == USB_PORT_TYPE_OTG) {
		if (!dwc_otg_get_param_adp_enable(dwc_otg_device->core_if))
			dwc_otg_enable_global_interrupts(dwc_otg_device->core_if);
		else
			dwc_otg_adp_start(dwc_otg_device->core_if,
								dwc_otg_is_host_mode(dwc_otg_device->core_if));
	} else {
		dwc_otg_enable_global_interrupts(dwc_otg_device->core_if);
	}
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	dwc_otg_device->usb_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	dwc_otg_device->usb_early_suspend.suspend = usb_early_suspend;
	dwc_otg_device->usb_early_suspend.resume = usb_early_resume;
	dwc_otg_device->usb_early_suspend.param = dwc_otg_device;
	register_early_suspend(&dwc_otg_device->usb_early_suspend);
#endif

#ifdef CONFIG_AMLOGIC_USB3PHY
	if (dwc_otg_device->core_if->controller_type == USB_OTG) {
		if (dwc_otg_device->core_if->phy_interface == 1)
			aml_new_usb_init();
		else
			aml_new_usb_v2_init();
	}
#endif

	return 0;

fail:
	dwc_otg_driver_remove(pdev);
	return retval;
}


#ifdef CONFIG_PM_SLEEP
static int dwc2_prepare(struct device *dev)
{
	return 0;
}

static void dwc2_complete(struct device *dev)
{
	return;
}

static int dwc2_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	const char *s_clock_name = NULL;
	const char *cpu_type = NULL;

	s_clock_name = of_get_property(pdev->dev.of_node, "clock-src", NULL);
	if (!s_clock_name)
		return 0;
	cpu_type = of_get_property(pdev->dev.of_node, "cpu-type", NULL);
	if (!cpu_type)
		return 0;
	clk_suspend_usb(pdev, s_clock_name,
			(unsigned long)(g_dwc_otg_device[pdev->id]->
				core_if->usb_peri_reg), cpu_type);

	return 0;
}

static int dwc2_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	const char *s_clock_name = NULL;
	const char *cpu_type = NULL;

	s_clock_name = of_get_property(pdev->dev.of_node, "clock-src", NULL);
	if (!s_clock_name)
		return 0;
	cpu_type = of_get_property(pdev->dev.of_node, "cpu-type", NULL);
	if (!cpu_type)
		return 0;
	clk_resume_usb(pdev, s_clock_name,
			(unsigned long)(g_dwc_otg_device[pdev->id]->
				core_if->usb_peri_reg), cpu_type);

	return 0;
}

static const struct dev_pm_ops dwc2_dev_pm_ops = {
	.prepare	= dwc2_prepare,
	.complete	= dwc2_complete,

	SET_SYSTEM_SLEEP_PM_OPS(dwc2_suspend, dwc2_resume)
};

#define DWC2_PM_OPS	(&(dwc2_dev_pm_ops))
#else
#define DWC2_PM_OPS	NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id of_dwc2_match[] = {
	{
		.compatible = "amlogic, dwc2"
	},
	{
		.compatible = "amlogic,dwc2"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_dwc2_match);
#endif

static struct platform_driver dwc_otg_driver = {
	.probe		= dwc_otg_driver_probe,
	.remove		= dwc_otg_driver_remove,
	.shutdown	= dwc_otg_driver_shutdown,
	.driver		= {
		.name	= "dwc_otg",
		.of_match_table	= of_match_ptr(of_dwc2_match),
		.pm	= DWC2_PM_OPS,
	},
};


static int __init dwc_otg_init(void)
{
	return platform_driver_register(&dwc_otg_driver);
}
late_initcall(dwc_otg_init);

MODULE_DESCRIPTION(DWC_DRIVER_DESC);
MODULE_AUTHOR("Synopsys Inc.");
MODULE_LICENSE("GPL");

module_param_named(otg_cap, dwc_otg_module_params.otg_cap, int, 0444);
MODULE_PARM_DESC(otg_cap, "OTG Capabilities 0=HNP&SRP 1=SRP Only 2=None");
module_param_named(opt, dwc_otg_module_params.opt, int, 0444);
MODULE_PARM_DESC(opt, "OPT Mode");
module_param_named(dma_enable, dwc_otg_module_params.dma_enable, int, 0444);
MODULE_PARM_DESC(dma_enable, "DMA Mode 0=Slave 1=DMA enabled");

module_param_named(dma_desc_enable, dwc_otg_module_params.dma_desc_enable, int,
		   0444);
MODULE_PARM_DESC(dma_desc_enable,
		 "DMA Desc Mode 0=Address DMA 1=DMA Descriptor enabled");

module_param_named(dma_burst_size, dwc_otg_module_params.dma_burst_size, int,
		   0444);
MODULE_PARM_DESC(dma_burst_size,
		 "DMA Burst Size 1, 4, 8, 16, 32, 64, 128, 256");
module_param_named(speed, dwc_otg_module_params.speed, int, 0444);
MODULE_PARM_DESC(speed, "Speed 0=High Speed 1=Full Speed");
module_param_named(host_support_fs_ls_low_power,
		   dwc_otg_module_params.host_support_fs_ls_low_power, int,
		   0444);
MODULE_PARM_DESC(host_support_fs_ls_low_power,
		 "Support Low Power w/FS or LS 0=Support 1=Don't Support");
module_param_named(host_ls_low_power_phy_clk,
		   dwc_otg_module_params.host_ls_low_power_phy_clk, int, 0444);
MODULE_PARM_DESC(host_ls_low_power_phy_clk,
		 "Low Speed Low Power Clock 0=48Mhz 1=6Mhz");
module_param_named(enable_dynamic_fifo,
		   dwc_otg_module_params.enable_dynamic_fifo, int, 0444);
MODULE_PARM_DESC(enable_dynamic_fifo, "0=cC Setting 1=Allow Dynamic Sizing");
module_param_named(data_fifo_size, dwc_otg_module_params.data_fifo_size, int,
		   0444);
MODULE_PARM_DESC(data_fifo_size,
		 "Total number of words in the data FIFO memory 32-32768");
module_param_named(dev_rx_fifo_size, dwc_otg_module_params.dev_rx_fifo_size,
		   int, 0444);
MODULE_PARM_DESC(dev_rx_fifo_size, "Number of words in the Rx FIFO 16-32768");
module_param_named(dev_nperio_tx_fifo_size,
		   dwc_otg_module_params.dev_nperio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(dev_nperio_tx_fifo_size,
		 "Number of words in the non-periodic Tx FIFO 16-32768");
module_param_named(dev_perio_tx_fifo_size_1,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[0], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_1,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_2,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[1], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_2,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_3,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[2], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_3,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_4,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[3], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_4,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_5,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[4], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_5,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_6,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[5], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_6,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_7,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[6], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_7,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_8,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[7], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_8,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_9,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[8], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_9,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_10,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[9], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_10,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_11,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[10], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_11,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_12,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[11], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_12,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_13,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[12], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_13,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_14,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[13], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_14,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_15,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[14], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_15,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(host_rx_fifo_size, dwc_otg_module_params.host_rx_fifo_size,
		   int, 0444);
MODULE_PARM_DESC(host_rx_fifo_size, "Number of words in the Rx FIFO 16-32768");
module_param_named(host_nperio_tx_fifo_size,
		   dwc_otg_module_params.host_nperio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(host_nperio_tx_fifo_size,
		 "Number of words in the non-periodic Tx FIFO 16-32768");
module_param_named(host_perio_tx_fifo_size,
		   dwc_otg_module_params.host_perio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(host_perio_tx_fifo_size,
		 "Number of words in the host periodic Tx FIFO 16-32768");
module_param_named(max_transfer_size, dwc_otg_module_params.max_transfer_size,
		   int, 0444);
/** @todo Set the max to 512K, modify checks */
MODULE_PARM_DESC(max_transfer_size,
		 "The maximum transfer size supported in bytes 2047-65535");
module_param_named(max_packet_count, dwc_otg_module_params.max_packet_count,
		   int, 0444);
MODULE_PARM_DESC(max_packet_count,
		 "The maximum number of packets in a transfer 15-511");
module_param_named(host_channels, dwc_otg_module_params.host_channels, int,
		   0444);
MODULE_PARM_DESC(host_channels,
		 "The number of host channel registers to use 1-16");
module_param_named(dev_endpoints, dwc_otg_module_params.dev_endpoints, int,
		   0444);
MODULE_PARM_DESC(dev_endpoints,
		 "The number of endpoints in addition to EP0 available for device mode 1-15");
module_param_named(phy_type, dwc_otg_module_params.phy_type, int, 0444);
MODULE_PARM_DESC(phy_type, "0=Reserved 1=UTMI+ 2=ULPI");
module_param_named(phy_utmi_width, dwc_otg_module_params.phy_utmi_width, int,
		   0444);
MODULE_PARM_DESC(phy_utmi_width, "Specifies the UTMI+ Data Width 8 or 16 bits");
module_param_named(phy_ulpi_ddr, dwc_otg_module_params.phy_ulpi_ddr, int, 0444);
MODULE_PARM_DESC(phy_ulpi_ddr,
		 "ULPI at double or single data rate 0=Single 1=Double");
module_param_named(phy_ulpi_ext_vbus, dwc_otg_module_params.phy_ulpi_ext_vbus,
		   int, 0444);
MODULE_PARM_DESC(phy_ulpi_ext_vbus,
		 "ULPI PHY using internal or external vbus 0=Internal");
module_param_named(i2c_enable, dwc_otg_module_params.i2c_enable, int, 0444);
MODULE_PARM_DESC(i2c_enable, "FS PHY Interface");
module_param_named(ulpi_fs_ls, dwc_otg_module_params.ulpi_fs_ls, int, 0444);
MODULE_PARM_DESC(ulpi_fs_ls, "ULPI PHY FS/LS mode only");
module_param_named(ts_dline, dwc_otg_module_params.ts_dline, int, 0444);
MODULE_PARM_DESC(ts_dline, "Term select Dline pulsing for all PHYs");
module_param_named(debug, g_dbg_lvl, int, 0444);
MODULE_PARM_DESC(debug, "");

module_param_named(en_multiple_tx_fifo,
		   dwc_otg_module_params.en_multiple_tx_fifo, int, 0444);
MODULE_PARM_DESC(en_multiple_tx_fifo,
		 "Dedicated Non Periodic Tx FIFOs 0=disabled 1=enabled");
module_param_named(dev_tx_fifo_size_1,
		   dwc_otg_module_params.dev_tx_fifo_size[0], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_1, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_2,
		   dwc_otg_module_params.dev_tx_fifo_size[1], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_2, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_3,
		   dwc_otg_module_params.dev_tx_fifo_size[2], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_3, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_4,
		   dwc_otg_module_params.dev_tx_fifo_size[3], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_4, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_5,
		   dwc_otg_module_params.dev_tx_fifo_size[4], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_5, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_6,
		   dwc_otg_module_params.dev_tx_fifo_size[5], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_6, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_7,
		   dwc_otg_module_params.dev_tx_fifo_size[6], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_7, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_8,
		   dwc_otg_module_params.dev_tx_fifo_size[7], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_8, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_9,
		   dwc_otg_module_params.dev_tx_fifo_size[8], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_9, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_10,
		   dwc_otg_module_params.dev_tx_fifo_size[9], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_10, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_11,
		   dwc_otg_module_params.dev_tx_fifo_size[10], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_11, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_12,
		   dwc_otg_module_params.dev_tx_fifo_size[11], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_12, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_13,
		   dwc_otg_module_params.dev_tx_fifo_size[12], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_13, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_14,
		   dwc_otg_module_params.dev_tx_fifo_size[13], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_14, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_15,
		   dwc_otg_module_params.dev_tx_fifo_size[14], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_15, "Number of words in the Tx FIFO 4-768");

module_param_named(thr_ctl, dwc_otg_module_params.thr_ctl, int, 0444);
MODULE_PARM_DESC(thr_ctl,
		 "Thresholding enable flag bit 0 - non ISO Tx thr., 1 - ISO Tx thr., 2 - Rx thr.- bit 0=disabled 1=enabled");
module_param_named(tx_thr_length, dwc_otg_module_params.tx_thr_length, int,
		   0444);
MODULE_PARM_DESC(tx_thr_length, "Tx Threshold length in 32 bit DWORDs");
module_param_named(rx_thr_length, dwc_otg_module_params.rx_thr_length, int,
		   0444);
MODULE_PARM_DESC(rx_thr_length, "Rx Threshold length in 32 bit DWORDs");

module_param_named(pti_enable, dwc_otg_module_params.pti_enable, int, 0444);
module_param_named(mpi_enable, dwc_otg_module_params.mpi_enable, int, 0444);
module_param_named(lpm_enable, dwc_otg_module_params.lpm_enable, int, 0444);
MODULE_PARM_DESC(lpm_enable, "LPM Enable 0=LPM Disabled 1=LPM Enabled");

module_param_named(besl_enable, dwc_otg_module_params.besl_enable, int, 0444);
MODULE_PARM_DESC(besl_enable, "BESL Enable 0=BESL Disabled 1=BESL Enabled");
module_param_named(baseline_besl, dwc_otg_module_params.baseline_besl, int, 0444);
MODULE_PARM_DESC(baseline_besl, "Set the baseline besl value");
module_param_named(deep_besl, dwc_otg_module_params.deep_besl, int, 0444);
MODULE_PARM_DESC(deep_besl, "Set the deep besl value");

module_param_named(ic_usb_cap, dwc_otg_module_params.ic_usb_cap, int, 0444);
MODULE_PARM_DESC(ic_usb_cap,
		 "IC_USB Capability 0=IC_USB Disabled 1=IC_USB Enabled");
module_param_named(ahb_thr_ratio, dwc_otg_module_params.ahb_thr_ratio, int,
		   0444);
MODULE_PARM_DESC(ahb_thr_ratio, "AHB Threshold Ratio");
module_param_named(power_down, dwc_otg_module_params.power_down, int, 0444);
MODULE_PARM_DESC(power_down, "Power Down Mode");
module_param_named(reload_ctl, dwc_otg_module_params.reload_ctl, int, 0444);
MODULE_PARM_DESC(reload_ctl, "HFIR Reload Control");
module_param_named(dev_out_nak, dwc_otg_module_params.dev_out_nak, int, 0444);
MODULE_PARM_DESC(dev_out_nak, "Enable Device OUT NAK");
module_param_named(cont_on_bna, dwc_otg_module_params.cont_on_bna, int, 0444);
MODULE_PARM_DESC(cont_on_bna, "Enable Enable Continue on BNA");
module_param_named(ahb_single, dwc_otg_module_params.ahb_single, int, 0444);
MODULE_PARM_DESC(ahb_single, "Enable AHB Single Support");
module_param_named(adp_enable, dwc_otg_module_params.adp_enable, int, 0444);
MODULE_PARM_DESC(adp_enable, "ADP Enable 0=ADP Disabled 1=ADP Enabled");
module_param_named(otg_ver, dwc_otg_module_params.otg_ver, int, 0444);
MODULE_PARM_DESC(otg_ver, "OTG revision supported 0=OTG 1.3 1=OTG 2.0");

/** @page "Module Parameters"
 *
 * The following parameters may be specified when starting the module.
 * These parameters define how the DWC_otg controller should be
 * configured. Parameter values are passed to the CIL initialization
 * function dwc_otg_cil_init
 *
 * Example: <code>modprobe dwc_otg speed=1 otg_cap=1</code>
 *

 <table>
 <tr><td>Parameter Name</td><td>Meaning</td></tr>

 <tr>
 <td>otg_cap</td>
 <td>Specifies the OTG capabilities. The driver will automatically detect the
 value for this parameter if none is specified.
 - 0: HNP and SRP capable (default, if available)
 - 1: SRP Only capable
 - 2: No HNP/SRP capable
 </td></tr>

 <tr>
 <td>dma_enable</td>
 <td>Specifies whether to use slave or DMA mode for accessing the data FIFOs.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Slave
 - 1: DMA (default, if available)
 </td></tr>

 <tr>
 <td>dma_burst_size</td>
 <td>The DMA Burst size (applicable only for External DMA Mode).
 - Values: 1, 4, 8 16, 32, 64, 128, 256 (default 32)
 </td></tr>

 <tr>
 <td>speed</td>
 <td>Specifies the maximum speed of operation in host and device mode. The
 actual speed depends on the speed of the attached device and the value of
 phy_type.
 - 0: High Speed (default)
 - 1: Full Speed
 </td></tr>

 <tr>
 <td>host_support_fs_ls_low_power</td>
 <td>Specifies whether low power mode is supported when attached to a Full
 Speed or Low Speed device in host mode.
 - 0: Don't support low power mode (default)
 - 1: Support low power mode
 </td></tr>

 <tr>
 <td>host_ls_low_power_phy_clk</td>
 <td>Specifies the PHY clock rate in low power mode when connected to a Low
 Speed device in host mode. This parameter is applicable only if
 HOST_SUPPORT_FS_LS_LOW_POWER is enabled.
 - 0: 48 MHz (default)
 - 1: 6 MHz
 </td></tr>

 <tr>
 <td>enable_dynamic_fifo</td>
 <td> Specifies whether FIFOs may be resized by the driver software.
 - 0: Use cC FIFO size parameters
 - 1: Allow dynamic FIFO sizing (default)
 </td></tr>

 <tr>
 <td>data_fifo_size</td>
 <td>Total number of 4-byte words in the data FIFO memory. This memory
 includes the Rx FIFO, non-periodic Tx FIFO, and periodic Tx FIFOs.
 - Values: 32 to 32768 (default 8192)

 Note: The total FIFO memory depth in the FPGA configuration is 8192.
 </td></tr>

 <tr>
 <td>dev_rx_fifo_size</td>
 <td>Number of 4-byte words in the Rx FIFO in device mode when dynamic
 FIFO sizing is enabled.
 - Values: 16 to 32768 (default 1064)
 </td></tr>

 <tr>
 <td>dev_nperio_tx_fifo_size</td>
 <td>Number of 4-byte words in the non-periodic Tx FIFO in device mode when
 dynamic FIFO sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>dev_perio_tx_fifo_size_n (n = 1 to 15)</td>
 <td>Number of 4-byte words in each of the periodic Tx FIFOs in device mode
 when dynamic FIFO sizing is enabled.
 - Values: 4 to 768 (default 256)
 </td></tr>

 <tr>
 <td>host_rx_fifo_size</td>
 <td>Number of 4-byte words in the Rx FIFO in host mode when dynamic FIFO
 sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>host_nperio_tx_fifo_size</td>
 <td>Number of 4-byte words in the non-periodic Tx FIFO in host mode when
 dynamic FIFO sizing is enabled in the core.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>host_perio_tx_fifo_size</td>
 <td>Number of 4-byte words in the host periodic Tx FIFO when dynamic FIFO
 sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>max_transfer_size</td>
 <td>The maximum transfer size supported in bytes.
 - Values: 2047 to 65,535 (default 65,535)
 </td></tr>

 <tr>
 <td>max_packet_count</td>
 <td>The maximum number of packets in a transfer.
 - Values: 15 to 511 (default 511)
 </td></tr>

 <tr>
 <td>host_channels</td>
 <td>The number of host channel registers to use.
 - Values: 1 to 16 (default 12)

 Note: The FPGA configuration supports a maximum of 12 host channels.
 </td></tr>

 <tr>
 <td>dev_endpoints</td>
 <td>The number of endpoints in addition to EP0 available for device mode
 operations.
 - Values: 1 to 15 (default 6 IN and OUT)

 Note: The FPGA configuration supports a maximum of 6 IN and OUT endpoints in
 addition to EP0.
 </td></tr>

 <tr>
 <td>phy_type</td>
 <td>Specifies the type of PHY interface to use. By default, the driver will
 automatically detect the phy_type.
 - 0: Full Speed
 - 1: UTMI+ (default, if available)
 - 2: ULPI
 </td></tr>

 <tr>
 <td>phy_utmi_width</td>
 <td>Specifies the UTMI+ Data Width. This parameter is applicable for a
 phy_type of UTMI+. Also, this parameter is applicable only if the
 OTG_HSPHY_WIDTH cC parameter was set to "8 and 16 bits", meaning that the
 core has been configured to work at either data path width.
 - Values: 8 or 16 bits (default 16)
 </td></tr>

 <tr>
 <td>phy_ulpi_ddr</td>
 <td>Specifies whether the ULPI operates at double or single data rate. This
 parameter is only applicable if phy_type is ULPI.
 - 0: single data rate ULPI interface with 8 bit wide data bus (default)
 - 1: double data rate ULPI interface with 4 bit wide data bus
 </td></tr>

 <tr>
 <td>i2c_enable</td>
 <td>Specifies whether to use the I2C interface for full speed PHY. This
 parameter is only applicable if PHY_TYPE is FS.
 - 0: Disabled (default)
 - 1: Enabled
 </td></tr>

 <tr>
 <td>ulpi_fs_ls</td>
 <td>Specifies whether to use ULPI FS/LS mode only.
 - 0: Disabled (default)
 - 1: Enabled
 </td></tr>

 <tr>
 <td>ts_dline</td>
 <td>Specifies whether term select D-Line pulsing for all PHYs is enabled.
 - 0: Disabled (default)
 - 1: Enabled
 </td></tr>

 <tr>
 <td>en_multiple_tx_fifo</td>
 <td>Specifies whether dedicatedto tx fifos are enabled for non periodic IN EPs.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Disabled
 - 1: Enabled (default, if available)
 </td></tr>

 <tr>
 <td>dev_tx_fifo_size_n (n = 1 to 15)</td>
 <td>Number of 4-byte words in each of the Tx FIFOs in device mode
 when dynamic FIFO sizing is enabled.
 - Values: 4 to 768 (default 256)
 </td></tr>

 <tr>
 <td>tx_thr_length</td>
 <td>Transmit Threshold length in 32 bit double words
 - Values: 8 to 128 (default 64)
 </td></tr>

 <tr>
 <td>rx_thr_length</td>
 <td>Receive Threshold length in 32 bit double words
 - Values: 8 to 128 (default 64)
 </td></tr>

<tr>
 <td>thr_ctl</td>
 <td>Specifies whether to enable Thresholding for Device mode. Bits 0, 1, 2 of
 this parmater specifies if thresholding is enabled for non-Iso Tx, Iso Tx and
 Rx transfers accordingly.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - Values: 0 to 7 (default 0)
 Bit values indicate:
 - 0: Thresholding disabled
 - 1: Thresholding enabled
 </td></tr>

<tr>
 <td>dma_desc_enable</td>
 <td>Specifies whether to enable Descriptor DMA mode.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Descriptor DMA disabled
 - 1: Descriptor DMA (default, if available)
 </td></tr>

<tr>
 <td>mpi_enable</td>
 <td>Specifies whether to enable MPI enhancement mode.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: MPI disabled (default)
 - 1: MPI enable
 </td></tr>

<tr>
 <td>pti_enable</td>
 <td>Specifies whether to enable PTI enhancement support.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: PTI disabled (default)
 - 1: PTI enable
 </td></tr>

<tr>
 <td>lpm_enable</td>
 <td>Specifies whether to enable LPM support.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: LPM disabled
 - 1: LPM enable (default, if available)
 </td></tr>

 <tr>
 <td>besl_enable</td>
 <td>Specifies whether to enable LPM Errata support.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: LPM Errata disabled (default)
 - 1: LPM Errata enable
 </td></tr>

  <tr>
 <td>baseline_besl</td>
 <td>Specifies the baseline besl value.
 - Values: 0 to 15 (default 0)
 </td></tr>

  <tr>
 <td>deep_besl</td>
 <td>Specifies the deep besl value.
 - Values: 0 to 15 (default 15)
 </td></tr>

<tr>
 <td>ic_usb_cap</td>
 <td>Specifies whether to enable IC_USB capability.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: IC_USB disabled (default, if available)
 - 1: IC_USB enable
 </td></tr>

<tr>
 <td>ahb_thr_ratio</td>
 <td>Specifies AHB Threshold ratio.
 - Values: 0 to 3 (default 0)
 </td></tr>

<tr>
 <td>power_down</td>
 <td>Specifies Power Down(Hibernation) Mode.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Power Down disabled (default)
 - 2: Power Down enabled
 </td></tr>

 <tr>
 <td>reload_ctl</td>
 <td>Specifies whether dynamic reloading of the HFIR register is allowed during
 run time. The driver will automatically detect the value for this parameter if
 none is specified. In case the HFIR value is reloaded when HFIR.RldCtrl == 1'b0
 the core might misbehave.
 - 0: Reload Control disabled (default)
 - 1: Reload Control enabled
 </td></tr>

 <tr>
 <td>dev_out_nak</td>
 <td>Specifies whether  Device OUT NAK enhancement enabled or no.
 The driver will automatically detect the value for this parameter if
 none is specified. This parameter is valid only when OTG_EN_DESC_DMA == 1¡¯b1.
 - 0: The core does not set NAK after Bulk OUT transfer complete (default)
 - 1: The core sets NAK after Bulk OUT transfer complete
 </td></tr>

 <tr>
 <td>cont_on_bna</td>
 <td>Specifies whether Enable Continue on BNA enabled or no.
 After receiving BNA interrupt the core disables the endpoint,when the
 endpoint is re-enabled by the application the
 - 0: Core starts processing from the DOEPDMA descriptor (default)
 - 1: Core starts processing from the descriptor which received the BNA.
 This parameter is valid only when OTG_EN_DESC_DMA == 1¡¯b1.
 </td></tr>

 <tr>
 <td>ahb_single</td>
 <td>This bit when programmed supports SINGLE transfers for remainder data
 in a transfer for DMA mode of operation.
 - 0: The remainder data will be sent using INCR burst size (default)
 - 1: The remainder data will be sent using SINGLE burst size.
 </td></tr>

<tr>
 <td>adp_enable</td>
 <td>Specifies whether ADP feature is enabled.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: ADP feature disabled (default)
 - 1: ADP feature enabled
 </td></tr>

  <tr>
 <td>otg_ver</td>
 <td>Specifies whether OTG is performing as USB OTG Revision 2.0 or Revision 1.3
 USB OTG device.
 - 0: OTG 2.0 support disabled (default)
 - 1: OTG 2.0 support enabled
 </td></tr>

*/
