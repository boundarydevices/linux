/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _HCI_HAL_INIT_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <hal_init.h>



#include <sdio_hal.h>

#include <sdio_ops.h>


#ifndef CONFIG_SDIO_HCI

#error "CONFIG_SDIO_HCI shall be on!\n"

#endif

#ifdef PLATFORM_LINUX
	#include<linux/mmc/sdio_func.h>
#endif

void open_dbg_port(_adapter * padapter){
		u32 val32=0;
		u8 val8=0;
		val8=read8(padapter, 0x10250003);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250003 value 0x%x\n",val8));
		val8=val8|BIT(1);
		mdelay_os(20);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250003 write value 0x%x\n",val8));
		write8(padapter, 0x10250003, val8);
		mdelay_os(20);
		val8=read8(padapter, 0x10250003);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250003 value 0x%x\n",val8));
		mdelay_os(20);
	
		val8=read8(padapter, 0x10250000);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250000 value 0x%x\n",val8));
		val8=val8&0x7F;
		mdelay_os(20);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250000 write value 0x%x\n",val8));
		write8(padapter, 0x10250000, val8);
		mdelay_os(20);
		val8=read8(padapter, 0x10250000);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250000 value 0x%x\n",val8));
		mdelay_os(20);

		val8=read8(padapter, 0x10250006);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250006 value 0x%x\n",val8));
		val8=0x30;
				mdelay_os(20);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250006 write value 0x%x\n",val8));
		write8(padapter, 0x10250006, val8);
				mdelay_os(20);
		val8=read8(padapter, 0x10250006);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x10250006 value 0x%x\n",val8));
		mdelay_os(20);
		val8=read8(padapter, 0x1025003a);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x1025003a value 0x%x\n",val8));
		val8=0x31;
				mdelay_os(20);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x1025003a write value 0x%x\n",val8));
		write8(padapter, 0x1025003a, val8);
				mdelay_os(20);
		val8=read8(padapter, 0x1025003a);
		RT_TRACE(_module_hal_init_c_,_drv_err_, ("\n 0x1025003a value 0x%x\n",val8));


	}
//power on secquence
u8 sd_hal_bus_init(_adapter * padapter)
{
	u8 val8,ret =_FAIL;
	u16 val16;
		u32 val32;
	s32 i;

	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("+sd_hal_bus_init: ret=%x\n", ret));
	//clear driver c2h , rx block number if reinitialize driver.
	padapter->dvobjpriv.c2hblknum = 0;
	padapter->dvobjpriv.rxblknum = 0;
	
#if 0
//#ifdef CONFIG_DEBUG_RTL871X
	RT_TRACE(_module_hal_init_c_, _drv_err_, ("sd_hal_bus_init: dump test========\n"));
	for (i = 0; i < 0x4f; i = i + 4) {
		RT_TRACE(_module_hal_init_c_, _drv_err_,
			 ("sd_hal_bus_init: Addr %#x=%#x\n",
			  0x10250000+i, read32(padapter, 0x10250000+i)));
	}
	RT_TRACE(_module_hal_init_c_, _drv_err_, ("sd_hal_bus_init: dump test end========\n"));
#endif
	if ((0x6911 == read16(padapter, 0x10250028)) &&
	    (0xdb8f == read16(padapter, 0x10250026)) &&
	    (0xb8a4 == read16(padapter, 0x10250008)))
{
		RT_TRACE(_module_hal_init_c_,_drv_err_,("sd_hal_bus_init: ====HW power is on====\n"));
#if 0
		write8(padapter,0x10250003,read8(padapter,0x10250003)&0x7f);
		printk("====HW power is on 0x10250003=%#02x  ===\n",read8(padapter,0x10250003));
		write8(padapter,0x10250003,read8(padapter,0x10250003)|0x80);
		printk("====HW power is on 0x10250003=%#02x  ===\n",read8(padapter,0x10250003));
#endif
		//modifid for reboot without power off

	write8(padapter,0x10250009,0x38);  //switch clock path to PON domain
		i = 0;
	do{
			if (i > 1000) break;
			i++;

			val8=read8(padapter,0x10250009);
		} while ((val8 & BIT(6)) == 0);
		if (i > 1000) {
			RT_TRACE(_module_hal_init_c_,_drv_err_,
				 ("sd_hal_bus_init: ERROR! 0x10250009=0x%02x\n", val8));
		}

		val8 = read8(padapter, 0x10250003) & 0x73;
		write8(padapter, 0x10250003, val8);

		write8(padapter, RF_CTRL, 0x0); // 0x1025001f
	
		val8 = read8(padapter, SYS_FUNC_EN+1);
		val8 |= BIT(3) | BIT(7); // FEN_DCORE | FEN_MREGEN
		write8(padapter, SYS_FUNC_EN+1, val8);

		write16(padapter, SYS_CLKR, 0xB8A0);
		RT_TRACE(_module_hal_init_c_, _drv_err_,
			 ("HW power is on 0x10250044=%#04x\n",
			  read16(padapter, 0x10250044)));
		goto exit;
	}

//4 	EFUSE_TEST	
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########EFUSE_TEST[step 1 Read value] value32=0x%x\n ",read32(padapter, EFUSE_TEST) ));
	val32=0;
	val32 = read32(padapter, EFUSE_TEST)|BIT(18);
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("######## EFUSE_TEST[step 2 Write] write with value32=0x%x\n ",val32 ));	
	write32(padapter, EFUSE_TEST, val32);
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("######## EFUSE_TEST[step 3 Read again] value32=0x%x\n ",val32 ));	

	
	val16 = read16(padapter, AFE_XTAL_CTRL)|BIT(0)|BIT(7);
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########init_pwr_clk AFE_XTAL_CTRL[step 2 write ] write with value16=0x%x\n ",val16 ));

	write16(padapter, AFE_XTAL_CTRL, val16);
	val16=read16(padapter,AFE_XTAL_CTRL);
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########init_pwr_clk AFE_XTAL_CTRL[step 3 read again] val = 0x%x\n ",val16));
	
//4	 AFE_MISC
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########init_pwr_clk AFE_MISC [step 1 read value] value8=0x%x\n ", read8(padapter, AFE_MISC)));
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########init_pwr_clk AFE_MISC [step 2 write] write with value8=0x%x\n ",(u32)(read8(padapter, AFE_MISC)|BIT(0))));
	write8(padapter, AFE_MISC,read8(padapter, AFE_MISC)|BIT(0));
	RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########init_pwr_clk AFE_MISC[step 3 read again]  value8=0x%x\n ", read8(padapter, AFE_MISC)));
	
//4 	SPS0_CTRL [7B]
RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########init_pwr_clk SPS0_CTRL[step 1 read value] val16 = 0x%x\n ",  read16(padapter,SPS0_CTRL) ));
RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,("########init_pwr_clk SPS0_CTRL[step 2 write] write with val16 = 0x%x\n ",  (u32)(read16(padapter,SPS0_CTRL)|BIT(0)|BIT(12)) ));

	write16(padapter, SPS0_CTRL, read16(padapter, SPS0_CTRL)|BIT(0)|BIT(12));
	if((read16(padapter,SPS0_CTRL)&0x0ff0) != 0x0490){
		 write16(padapter, SPS0_CTRL,0x5497);
		RT_TRACE(_module_hci_hal_init_c_,_drv_notice_,(" write SPS0_CTRL 0x5497\n")); 
	}

	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SPS0_CTRL [step 3 read again ] val16 = 0x%x\n ",  read16(padapter,SPS0_CTRL) ));


	udelay_os(1500);

//4	AFE_MISC	
RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_MISC[step 1 read value] val8 = 0x%x\n ",  read8(padapter,AFE_MISC) ));
RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_MISC[step 2 write] write with val8 = 0x%x\n ", (u32)(read8(padapter,AFE_MISC)|BIT(1)) ));
	write8(padapter, AFE_MISC, read8(padapter, AFE_MISC)|BIT(1));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_MISC[step 3 read again] val8 = 0x%x\n ",  read8(padapter,AFE_MISC) ));

//4	SPS0_CTRL
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SPS0_CTRL[step 1 read value] val8 = 0x%x\n ",  read8(padapter,SPS0_CTRL) ));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SPS0_CTRL[step 2 write] write with  val8 = 0x%x\n ", (u32)(read8(padapter,SPS0_CTRL)|BIT(1)) ));
	write8(padapter, SPS0_CTRL, read8(padapter, SPS0_CTRL)|BIT(1));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SPS0_CTRL[step 3 read again]  val8 = 0x%x\n ",  read16(padapter,SPS0_CTRL) ));

	udelay_os(500);
	
		//-- Enable digital core power
//4	LDOV12D_CTRL		

	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk LDOV12D_CTRL[step 2 write] write with val8 = 0x%x\n ", (u32) (read8(padapter,LDOV12D_CTRL)|BIT(0))));
	
	write8(padapter, LDOV12D_CTRL, read8(padapter, LDOV12D_CTRL)|BIT(0));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk LDOV12D_CTRL[step 3 read again] val8 = 0x%x\n ",  read8(padapter,LDOV12D_CTRL) ));

//4	LDOA15_CTRL
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk LDOA15_CTRL[step 1 read value] val8 = 0x%x\n ",  read8(padapter,LDOA15_CTRL) ));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk LDOA15_CTRL[step 2 write] write with  val8 = 0x%x\n ", (u32)(read8(padapter,LDOA15_CTRL)|BIT(0)) ));

	write8(padapter, LDOA15_CTRL, read8(padapter, LDOA15_CTRL)|BIT(0));
	udelay_os(1000);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk LDOA15_CTRL[step 3 read again] val8 =0x%x\n ",  read8(padapter,LDOA15_CTRL) ));

	
		//-- Switch the power cut
//4	SYS_ISO_CTRL
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 1 read value] val16 = 0x%x\n ",  read16(padapter,SYS_ISO_CTRL) ));

	val16 = read16(padapter, SYS_ISO_CTRL)|BIT(3);	
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 2 write] write with  val16 = 0x%x\n ",  val16 ));
	write16(padapter, SYS_ISO_CTRL, val16);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 3 read again] val16 = 0x%x\n ",  read16(padapter,SYS_ISO_CTRL) ));

//4	SYS_FUNC_EN
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_FUNC_EN[step 1 read value] val16 = 0x%x\n ",  read16(padapter,SYS_FUNC_EN) ));
	val16 = (read16(padapter, SYS_FUNC_EN)|BIT(13));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_FUNC_EN[step 2 write] write with val16 = 0x%x\n ",  val16 ));
	write16(padapter, SYS_FUNC_EN, val16);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_FUNC_EN[step 3 read again] val16 = 0x%x\n ",  read16(padapter,SYS_FUNC_EN) ));


//4	SYS_ISO_CTRL
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 1 read value] val16 = 0x%x\n ",  read16(padapter,SYS_ISO_CTRL) ));
	val16 = read16(padapter, SYS_ISO_CTRL)&0x77FF;	
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 2 write] write with val16 = 0x%x\n ",  val16 ));
	write16(padapter, SYS_ISO_CTRL, val16);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 3 read again] val = 0x%x\n ",  read16(padapter,SYS_ISO_CTRL) ));

//4	AFE_XTAL_CTRL
	//enable Xtal and AFE BG
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_XTAL_CTRL[step 1 read value] val16 = 0x%x\n ",  read16(padapter,AFE_XTAL_CTRL) ));
	
	val16 = (read16(padapter, AFE_XTAL_CTRL)&0xFBFF);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_XTAL_CTRL[step 2 write] write with  val = 0x%x\n ",  val16 ));
	write16(padapter, AFE_XTAL_CTRL, val16);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_XTAL_CTRL[step read again] val = 0x%x\n ",  read16(padapter,AFE_XTAL_CTRL) ));

//4	AFE_MISC	
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_MISC val [step 1 read value]  val8= 0x%x\n ",  read8(padapter,AFE_MISC) ));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_MISC val [step 2 write] write with val8= 0x%x\n ", (u32) (read8(padapter,AFE_MISC)|BIT(3)) ));
	write8(padapter, AFE_MISC, read8(padapter, AFE_MISC)|BIT(3));
	udelay_os(100);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_MISC val8 = 0x%x\n ",  read8(padapter,AFE_MISC) ));

	//enable AFE and PLL
//4	AFE_PLL_CTRL
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_PLL_CTRL[step 1 read value] val32 = 0x%x\n ",  read32(padapter,AFE_PLL_CTRL) ));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_PLL_CTRL[step 2 write] write with  val32 = 0x%x\n ", (u32)(read32(padapter,AFE_PLL_CTRL)|BIT(0))));
	
	write32(padapter, AFE_PLL_CTRL, read32(padapter, AFE_PLL_CTRL)|BIT(0));
	udelay_os(500);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_PLL_CTRL[step 3 read again] val32 = 0x%x\n ",  read32(padapter,AFE_PLL_CTRL) ));


	//Configure Clock Setting
	//a. CPU clock select: using default
	//b. SYS_CLK clock select

	//restore required clock path
	//restore required clock path
//4	AFE_PLL_CTRL	
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_PLL_CTRL[step 1 read value] val32 = 0x%x\n ",  read32(padapter,AFE_PLL_CTRL) ));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_PLL_CTRL[step 2 write] wirte with val32 = 0x%x\n ", (u32) (read32(padapter,AFE_PLL_CTRL)|BIT(4)|BIT(8)) ));
	write32(padapter, AFE_PLL_CTRL, read32(padapter, AFE_PLL_CTRL)|BIT(4)|BIT(8));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk AFE_PLL_CTRL [step 3 read again ]val32 = 0x%x\n ",  read32(padapter,AFE_PLL_CTRL) ));

//4	SYS_ISO_CTRL
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 1 read value ] val16 = 0x%x\n ",  read16(padapter,SYS_ISO_CTRL) ));
	val16 = read16(padapter, SYS_ISO_CTRL)&0xFFEE;	
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 2 write] write with val16= 0x%x\n ",  val16 ));
	write16(padapter, SYS_ISO_CTRL, val16);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_ISO_CTRL[step 3 read again] val16 = 0x%x\n ",  read16(padapter,SYS_ISO_CTRL) ));

	

	{

		val16=read16(padapter, SYS_CLKR);

		RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("\n read SYS_CLKR value=0x%.4x\n",val16));
		val16=val16&0xfffb;
		write16(padapter, SYS_CLKR, val16);
		val16=read16(padapter, SYS_CLKR);
		RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("\n read SYS_CLKR value=0x%.4x\n",val16));

		RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("\n read SYS_CLKR[1] value =0x%.4x\n", read16(padapter, SYS_CLKR)));
		RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("\n read SYS_CLKR[2] write with value = 0x%.4x\n",(u32)(read16(padapter, SYS_CLKR)|BIT(11)|BIT(12))));

		write16(padapter, SYS_CLKR, read16(padapter, SYS_CLKR)|BIT(11)|BIT(12));
		RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("\n write SYS_CLKR value=0x%.4x\n", read16(padapter, SYS_CLKR)));


		val16=read16(padapter, SYS_CLKR);
		RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("\n read SYS_CLKR value=0x%.4x\n",val16));
		val16=val16&0xfff9;
		write16(padapter, SYS_CLKR, val16);
		val16=read16(padapter, SYS_CLKR);
		RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("\n read SYS_CLKR value=0x%.4x\n",val16));


	}
//	write16(padapter, SYS_CLKR, read16(padapter, SYS_CLKR)|SYS_CLK_EN|MAC_CLK_EN);




	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_FUNC_EN[step 1 read value] val16 = 0x%x\n ",  read16(padapter,SYS_FUNC_EN)));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_FUNC_EN[step 2 write] write with val16 = 0x%x\n ", (u32) (read16(padapter,SYS_FUNC_EN)|FEN_DCORE|BIT(15))));

	write16(padapter, SYS_FUNC_EN, read16(padapter, SYS_FUNC_EN)|FEN_DCORE|BIT(15));
//	value16 = Read16(SYS_CLKR)&MSK16(SWHW_SEL_SHT);

	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_CLKR val = %x\n ",  read16(padapter,SYS_CLKR) ));
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_FUNC_EN val = %x\n ",  read16(padapter,SYS_FUNC_EN)));

	write16(padapter, SYS_ISO_CTRL, (read16(padapter, SYS_ISO_CTRL)&0xF9FF) );
//	value16 = Read16(SYS_CLKR)&MSK16(SWHW_SEL_SHT);

exit:
	{
//	u8			val8;
	
//	val8  =  read8(padapter, SDIO_TX_CTRL);
//	write8(padapter, SDIO_TX_CTRL, val8|0xfc);


	write8(padapter, SYS_CLKR+1, 0xB8);
	val8  =  read8(padapter, SYS_CLKR+1);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk SYS_CLKR+1  val8 = %x\n ",  val8 ));

	
	}
	write16(padapter,TXPAUSE, 0x0FFF);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk TXPAUSE val = 0x%x[0x0FFF]\n ",  read16(padapter, TXPAUSE)));
	write16(padapter, MACID, 0x5678);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk MACID val = 0x%x[0x5678]\n ",  read16(padapter, MACID)));
	write16(padapter, CR, (u16)0x3FFF);
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("########init_pwr_clk CR val = 0x%x[0x3FFF]\n ",  read16(padapter, CR)));
	
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("######## sd_hal_bus_init <=== \n "));
#if 0
	
//3 For debuging  
open_dbg_port(padapter);

#endif

	write8(padapter,SDIO_HRPWM,0);
	ret=_SUCCESS;

#if 0
//#ifdef CONFIG_DEBUG_RTL871X
	RT_TRACE(_module_hal_init_c_, _drv_err_, ("dump test========\n"));
	for (i = 0; i < 0x4f; i = i+4) {
		RT_TRACE(_module_hal_init_c_, _drv_err_,
			 ("Addr %#x=%#08x\n",
			  0x10250000+i, read32(padapter, 0x10250000+i)));
	}
	RT_TRACE(_module_hal_init_c_, _drv_err_, ("dump test end========\n"));
#endif
	RT_TRACE(_module_hci_hal_init_c_,_drv_err_,("-sd_hal_bus_init: ret=%d\n",ret));

	return ret;
}
 u8 sd_hal_bus_deinit(_adapter * padapter)
 {

	return _SUCCESS;
 }




