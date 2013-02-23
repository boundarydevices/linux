/*
 * Driver for Techwell TW6864/68 based DVR cards
 *
 * (c) 2009-10 liran <jlee@techwellinc.com.cn> [Techwell China]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/videodev2.h>
#include <linux/delay.h>
#include <asm/div64.h>

#include "tw686x.h"
#include "tw686x-reg.h"
#include "tw686x-device.h"

int  tw686x_dev_init( struct tw686x_chip *chip )
{
	u32 tmp;
	int   i = 0;
	u32 page_table_p[TW6864_MAX_NUM_SG_DMA] = {
		TW6869_R_DMA_PAGE_TABLE0, TW6869_R_DMA_PAGE_TABLE2, TW6869_R_DMA_PAGE_TABLE4, TW6869_R_DMA_PAGE_TABLE6,
		TW6869_R_DMA_PAGE_TABLE8, TW6869_R_DMA_PAGE_TABLEA, TW6869_R_DMA_PAGE_TABLEC, TW6869_R_DMA_PAGE_TABLEE
	};
	u32 page_table_b[TW6864_MAX_NUM_SG_DMA] = {
		TW6869_R_DMA_PAGE_TABLE1, TW6869_R_DMA_PAGE_TABLE3, TW6869_R_DMA_PAGE_TABLE5, TW6869_R_DMA_PAGE_TABLE7,
		TW6869_R_DMA_PAGE_TABLE9, TW6869_R_DMA_PAGE_TABLEB, TW6869_R_DMA_PAGE_TABLED, TW6869_R_DMA_PAGE_TABLEF
	};

	dprintk(DPRT_LEVEL0, chip, "%s()\n", __func__);

	if( pci_read_config_dword( chip->pci, 0x04, &tmp) == 0) {
		tmp |= 7;
		pci_write_config_dword( chip->pci, 0x04, tmp);
	}

	if( pci_read_config_dword( chip->pci, 0x78, &tmp) == 0) {
		tmp = (0x8<<5) | (tmp & 0xFFFFFE1F);		//8=128B, 9=256B, A=512B, B=1024B
		pci_write_config_dword( chip->pci, 0x78, tmp);
	}

	//Software Reset
	tw_write(TW6864_R_SYS_SOFT_RST, 0x01);
	tw_write(TW6864_R_SYS_SOFT_RST, 0x0F);

	for(i=0;i<TW6864_MAX_NUM_SG_DMA;i++) {
        //S&G, Common Buffer DMA memory setup
        tw_write(page_table_p[i], 0x00000000);
        tw_write(page_table_b[i], 0x00000000);
	}

	for(tmp=0; tmp<TW6864_MAX_NUM_DATA_DMA; tmp++) {
		tw_write(TW6864_R_CH8to15_CONFIG_REG_P(tmp),0);
		tw_write(TW6864_R_CH8to15_CONFIG_REG_B(tmp),0);
	}

	if( pci_read_config_dword(chip->pci, 0x04, &tmp) == 0) {
        dprintk(DPRT_LEVEL0,  chip, "enable INTx\n");
		tmp &= 0xFFFFFBFF;							// Enable INTx
		pci_write_config_dword(chip->pci, 0x04, tmp);
	}

	if( pci_read_config_dword(chip->pci, 0x50, &tmp) == 0) {	//CFG_MSI_CAP = 0x50
		tmp &= 0xFFFEFFFF;							//Disable MSI
		pci_write_config_dword( chip->pci, 0x50, tmp );
	}

	if( pci_read_config_dword(chip->pci, 0xAC, &tmp) == 0) {	//CFG_MSIX_CAP = 0xAC
		tmp &= 0x7FFFFFFF;							//Disable MSI-X
		pci_write_config_dword(chip->pci, 0xAC, tmp);
	}

	tw_write(TW6864_R_AVSRST,0x3F);             //Reset Internal video decoders
	tw_write(TW6864_R_DMA_CMD, 0);			     //Reset all DMA channels
	tw_write(TW6864_R_DMA_CHANNEL_ENABLE, 0);	 //Disable all DMA channels
	tw_write(TW6864_R_DMA_INT_REF,  0x4c4b6/2);//0x4c4b6);	 //Set INT interval for 2.0ms   CPU占用率100%时，这个值设大了可能会造成DMA timeout

	tmp = 0x00FF0004; //(TW6864_MAXSWITCH>1) ? 0x00000004 : 0x00FF0004;
	tw_write(TW6864_R_DMA_CONFIG,  tmp);	    //enable vsync reset dma_operator fifo,INTx, MSI disabled,
												//little endian, bad/vsync protection on

	tw_write(TW6864_R_DMA_CHANNEL_TIMEOUT,  0x3FFFFFFF);//0x14FF0FF0);//0x140c8b08);

	tmp  = 0x15DC;								//512*(2*125)/(27*720/858)
	tw_write(TW6864_R_PHASE_REF_CONFIG,  tmp);

	tmp = 1;
	tmp |= TW6864_R_VIDEO_CTRL1_STANDARDALL;	//pal, 4 D1 TDM format, 108MHz
	tw_write(TW6864_R_VIDEO_CTRL1,  tmp);

    //set gpio mode
	tmp = tw_read(TW6869_R_PIN_CFG_CTRL);
	tmp&= ~0x03;
	tmp|= 0x02;
	tw_write(TW6869_R_PIN_CFG_CTRL,tmp);

	tmp = 0x40a0;
	tw_write(TW6864_R_CSR_REG,tmp);

	tmp = TW6864_R_VIDEO_CTRL2_GENRSTALL | (TW6864_VIDEO_GEN_PATTERNS <<8) | TW6864_R_VIDEO_CTRL2_GENALL;
	tw_write(TW6864_R_VIDEO_CTRL2,  tmp);

	tmp = (TW6864_AUDIO_DMA_LENGTH << 19) |
		  (TW6864_AUDIO_SAMP_RATE_INT(TW6864_AUDIO_SAMPLE_RATE)<<5) |
		  (TW6864_AUDIO_GEN_PATTERN << 4) |
		  (TW6864_AUDIO_GEN_MIX_SEL <<1 ) |
		   TW6864_AUDIO_GEN;
	tw_write(TW6864_R_AUDIO_CTRL1,  tmp);

	tmp = (u32)TW6864_AUDIO_SAMP_RATE_EXT(TW6864_AUDIO_SAMPLE_RATE);
	tw_write(TW6864_R_AUDIO_CTRL2,  tmp);

	tmp = 0x40;
	if(TW6864_AUDIO_SAMPLE_BIT == 8) {
		tmp |= BIT(8);
	}
	tw_write(TW6864_R_AUDIO_CTRL3, tmp);

	//Show Blue background if no signal
	tw_write(TW6864_R_MISC_CTRLII1, 0xE7);

	tmp = 0x40;
	tw_write(TW6864_R_AUDIO_GAIN_0, tmp);
	tw_write(TW6864_R_AUDIO_GAIN_1, tmp);
	tw_write(TW6864_R_AUDIO_GAIN_2, tmp);
	tw_write(TW6864_R_AUDIO_GAIN_3, tmp);
	tw_write(TW6869_R_AUDIO_GAIN_4, tmp);
	tw_write(TW6869_R_AUDIO_GAIN_5, tmp);
	tw_write(TW6869_R_AUDIO_GAIN_6, tmp);
	tw_write(TW6869_R_AUDIO_GAIN_7, tmp);

	tw_write(TW6864_R_VERTICAL_CTRL,0x00); //0x26 will cause ch0 and ch1 have dma_error.
	tw_write(TW6864_R_LOOP_CTRL,    0xa5);
	tw_write(TW6864_R_GPIO_REG,	    0xFFFF0000);
 	tw_write(TW6864_R_HFLT1,		0x00);//0xBB);		//NOTE: 这个会影响CIF的清晰度，针对CIF和4CIF需分别设置
 	tw_write(TW6864_R_HFLT2,		0x00);//0xBB);

    return 0;
}

void tw686x_dev_uninit( struct tw686x_chip *chip )
{
	int   i = 0;
	u32 page_table_p[TW6864_MAX_NUM_SG_DMA] = {
		TW6869_R_DMA_PAGE_TABLE0, TW6869_R_DMA_PAGE_TABLE2, TW6869_R_DMA_PAGE_TABLE4, TW6869_R_DMA_PAGE_TABLE6,
		TW6869_R_DMA_PAGE_TABLE8, TW6869_R_DMA_PAGE_TABLEA, TW6869_R_DMA_PAGE_TABLEC, TW6869_R_DMA_PAGE_TABLEE
	};
	u32 page_table_b[TW6864_MAX_NUM_SG_DMA] = {
		TW6869_R_DMA_PAGE_TABLE1, TW6869_R_DMA_PAGE_TABLE3, TW6869_R_DMA_PAGE_TABLE5, TW6869_R_DMA_PAGE_TABLE7,
		TW6869_R_DMA_PAGE_TABLE9, TW6869_R_DMA_PAGE_TABLEB, TW6869_R_DMA_PAGE_TABLED, TW6869_R_DMA_PAGE_TABLEF
	};

	dprintk(DPRT_LEVEL0, chip, "%s()\n", __func__);

	tw_write(TW6864_R_DMA_CMD, 0);			     //Reset all DMA channels
	tw_write(TW6864_R_DMA_CHANNEL_ENABLE, 0);	 //Disable all DMA channels

	for(i=0; i<TW6864_MAX_NUM_SG_DMA; i++) {
		tw_write(TW6864_R_VC_CTRL_REG(i), 0x00000000);
		tw_write(page_table_p[i], 0x00000000);
		tw_write(page_table_b[i], 0x00000000);
	}
}

void tw686x_dev_setdma( struct tw686x_chip *chip, u32 dma_cmd)
{
	u32  dma_en = dma_cmd;

	if(dma_cmd != 0) {
		dma_en |= (1<<31);
	}

	tw_write(TW6864_R_DMA_CHANNEL_ENABLE, dma_en);
	//tw_write(TW6864_R_DMA_CMD, dma_cmd);
}

int tw686x_dev_set_mux(struct tw686x_vdev *dev, unsigned int input)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;
	u32 uConfig = tw_read(TW6864_R_CH0to7_CONFIG_REG(ch));

	dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, (int)input);

	uConfig &= ~(3 << 30);
	uConfig |= (input&3) << 30;
	tw_write(TW6864_R_CH0to7_CONFIG_REG(ch), uConfig);

	return 0;
}

void tw686x_dev_set_standard(struct tw686x_vdev *dev, v4l2_std_id value, bool b_auto)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;
	u32 tmp = tw_read(TW6864_R_VIDEO_CTRL1);

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	tmp &= ~TW6864_R_VIDEO_CTRL1_STANDARD(ch);
	if(IS_PAL(value)) {
		tmp |= TW6864_R_VIDEO_CTRL1_STANDARD(ch);		//pal, 4 D1 TDM format, 108MHz
	}

	tw_write(TW6864_R_VIDEO_CTRL1,  tmp);

	if(ch < 4) {
		if(b_auto) {
			tmp = 0x07;
		}
		else {
			tmp = tw_read(TW6864_R_CHANNEL(TW6864_R_STANDARD_0, ch));
			tmp&= 0xF8;
			tmp|= IS_PAL(value) ? 1 : 0;
		}
		tw_write(TW6864_R_CHANNEL(TW6864_R_STANDARD_0, ch), tmp);
	}
	else {
		ch -= 4;
		if(b_auto) {
			tmp = 0x07;
		}
		else {
			tmp = tw_read(TW6864_R_CHANNEL(TW6869_R_STANDARD_4, ch));
			tmp&= 0xF8;
			tmp|= IS_PAL(value) ? 1 : 0;
		}
		tw_write(TW6864_R_CHANNEL(TW6869_R_STANDARD_4, ch), tmp);
	}
}

static u32 tw686x_dev_map_pixelformat(u32 fourcc)
{
	switch(fourcc) {
	case V4L2_PIX_FMT_YUYV :
		fourcc = (TW6864_VIDEO_FORMAT_YUYV & 7);
		break;
	case V4L2_PIX_FMT_UYVY :
		fourcc = (TW6864_VIDEO_FORMAT_UYVY & 7);
		break;
	case V4L2_PIX_FMT_YVU420 :
		fourcc = (TW6864_VIDEO_FORMAT_YUV420 & 7);
		break;
	case V4L2_PIX_FMT_YUV411P :
		fourcc = (TW6864_VIDEO_FORMAT_Y411 & 7);
		break;
	case V4L2_PIX_FMT_Y41P :
		fourcc = (TW6864_VIDEO_FORMAT_Y41P & 7);
		break;
	case V4L2_PIX_FMT_RGB565:
		fourcc = (TW6864_VIDEO_FORMAT_RGB565 & 7);
		break;
	case V4L2_PIX_FMT_RGB555:
		fourcc = (TW6864_VIDEO_FORMAT_RGB555 & 7);
		break;
	default:
		fourcc = (TW6864_VIDEO_FORMAT_YUYV & 7);
	}

	return fourcc;
}

void tw686x_dev_set_pixelformat(struct tw686x_vdev *dev, u32 fourcc)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;
	u32 uConfig = tw_read(TW6864_R_CH0to7_CONFIG_REG(ch));

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	uConfig &= ~(7 << 20);
	uConfig |= tw686x_dev_map_pixelformat(fourcc) << 20;
	tw_write(TW6864_R_CH0to7_CONFIG_REG(ch), uConfig);
}

void tw686x_dev_set_size(struct tw686x_vdev *dev, int width, int height)
{
    struct tw686x_chip *chip = dev->chip;
    int   ch = dev->channel_id;
	u32   tmp, tmp1, tmp2;
	int   maxWidth = TW686X_REMOVE_BLACKSTRIPE ? 720 : 704;
	bool  isPal = IS_PAL(dev->tvnorm);

	dvprintk(DPRT_LEVEL0, dev, "%s(%d, %d)\n", __func__, width, height);

	//SH_Scaler
	//setup for Black stripe remover
	tmp1 = width-12;   //EndPos
	tmp2 = 4;		   //StartPos
	tmp = (tmp1 - tmp2)*(1<<16)/width;
	tmp = (tmp2 & 0x1F) | ((tmp1 & 0x3FF) << 5) | (tmp <<15);
	tmp |= TW686X_REMOVE_BLACKSTRIPE;
	tw_write(TW6864_R_SHSCALER_REG(ch), tmp);

	tmp = (u32)TW686X_VIDEO_SIZE(width,height);
	tmp1 = tmp2 = tmp;
	tw_write(TW6864_R_VIDEO_SIZE_REGA, tmp);
	tw_write(TW6864_R_VIDEO_SIZE_REG(ch), tmp); //for Rev.B or later only

	//H/V Scale
	tmp1 &= 0x7FF;
	tmp2 = (tmp2>>16)&0x1FF;	 //V
	tmp1 = (maxWidth*256)/tmp1;  //H
	tmp2 = ((isPal?288:240)*256)/tmp2;
	tmp = tmp1 & 0xFF;

	if(ch < 4) {
		tw_write(TW6864_R_CHANNEL(TW6864_R_HSCALE_LO_0,ch),  tmp);

		tmp = tmp2 & 0xFF;
		tw_write(TW6864_R_CHANNEL(TW6864_R_VSCALE_LO_0,ch),  tmp);

		tmp = (((tmp2 >> 8)& 0xF) << 4) | ( (tmp1>>8) & 0xF );
		tw_write(TW6864_R_CHANNEL(TW6864_R_VHSCALE_HI_0,ch),  tmp);

		if(tmp1 > 1) {
			if(width==352)		{ tmp = 0x01; }
			else if(width==176)	{ tmp = 0x02; }
			else				{ tmp = 0x00; }
		}
		else					{ tmp = 0x0B; }
		tmp1 = tw_read(TW6864_R_HFLT1 + (ch/2)*4);
		tmp1&= ~(0x0F << ((ch%2)*4));
		tmp1|= ((tmp&0x0F) << ((ch%2)*4));
		tw_write(TW6864_R_HFLT1 + (ch/2)*4, tmp1);

		if(isPal) {
			tw_write(TW6864_R_CHANNEL(TW6864_R_VDELAY_0,ch), 0x17);
			tw_write(TW6864_R_CHANNEL(TW6864_R_HDELAY_0,ch), TW686X_REMOVE_BLACKSTRIPE?0x0A:0x00E);
		}
		else {
			tw_write(TW6864_R_CHANNEL(TW6864_R_VDELAY_0,ch), 0x14);
			tw_write(TW6864_R_CHANNEL(TW6864_R_HDELAY_0,ch), 0x0E);//TW6864_REMOVE_BLACKSTRIPE?0x0A:0x00C);
		}
		tw_write(TW6864_R_CHANNEL(TW6864_R_F2_CNT_0,ch), 0x00);
	}
	else {
		ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_HSCALE_LO_4,ch),tmp);

		tmp = tmp2 & 0xFF;
		tw_write(TW6864_R_CHANNEL(TW6869_R_VSCALE_LO_4,ch),tmp);

		tmp = (((tmp2 >> 8)& 0xF) << 4) | ( (tmp1>>8) & 0xF );
		tw_write(TW6864_R_CHANNEL(TW6869_R_VHSCALE_HI_4,ch),tmp);

		if(tmp1 > 1) {
			if(width==352)		{ tmp = 0x01; }
			else if(width==176)	{ tmp = 0x02; }
			else				{ tmp = 0x00; }
		}
		else					{ tmp = 0x0B; }
		tmp1 = tw_read(TW6869_R_HFLT1 + (ch/2)*4);
		tmp1&= ~(0x0F << ((ch%2)*4));
		tmp1|= ((tmp&0x0F) << ((ch%2)*4));
		tw_write(TW6869_R_HFLT1 + (ch/2)*4, tmp1);

		if(isPal) {
			tw_write(TW6864_R_CHANNEL(TW6869_R_VDELAY_4,ch), 0x17);
			tw_write(TW6864_R_CHANNEL(TW6869_R_HDELAY_4,ch), TW686X_REMOVE_BLACKSTRIPE?0x0A:0x0E);
		}
		else {
			tw_write(TW6864_R_CHANNEL(TW6869_R_VDELAY_4,ch), 0x14);
			tw_write(TW6864_R_CHANNEL(TW6869_R_HDELAY_4,ch), 0x0E);//TW6864_REMOVE_BLACKSTRIPE?0x0A:0x00C);
		}
		tw_write(TW6864_R_CHANNEL(TW6869_R_F2_CNT_4,ch), 0x00);
	}
}

void tw686x_dev_set_size2(struct tw686x_vdev *dev, int width, int height)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;
	u32 tmp, tmp1, tmp2;
	int maxWidth = TW686X_REMOVE_BLACKSTRIPE ? 720 : 704;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d, %d)\n", __func__, width, height);

	tmp = (u32)TW686X_VIDEO_SIZE_F2(width,height);
	tmp1 = tmp2 = tmp;
	tw_write(TW6864_R_VIDEO_SIZE_REG_F2(ch), tmp); //for Rev.B or later only

    //H/V Scale
    tmp1&= 0x7FF;
    tmp2 = (tmp2>>16)&0x1FF;	 //V
    tmp1 = (maxWidth*256)/tmp1;  //H
    tmp2 = ((IS_PAL(dev->tvnorm)?288:240)*256)/tmp2;
    tmp  = tmp1 & 0xFF;

	if(ch < 4) {
		tw_write(TW6864_R_CHANNEL(TW6864_R_HSCALE_LO_F2_0,ch),  tmp);

		tmp = tmp2 & 0xFF;
		tw_write(TW6864_R_CHANNEL(TW6864_R_VSCALE_LO_F2_0,ch),  tmp);

		tmp = (((tmp2 >> 8)& 0xF) << 4) | ( (tmp1>>8) & 0xF );
		tw_write(TW6864_R_CHANNEL(TW6864_R_VHSCALE_HI_F2_0,ch),  tmp);

		if(IS_PAL(dev->tvnorm)) {
			tw_write(TW6864_R_CHANNEL(TW6864_R_VDELAY_F2_0,ch), 0x17);
			tw_write(TW6864_R_CHANNEL(TW6864_R_HDELAY_F2_0,ch), TW686X_REMOVE_BLACKSTRIPE?0x0A:0x00E);
		}
		else {
			tw_write(TW6864_R_CHANNEL(TW6864_R_VDELAY_F2_0,ch), 0x14);
			tw_write(TW6864_R_CHANNEL(TW6864_R_HDELAY_F2_0,ch), 0x0E);//TW686X_REMOVE_BLACKSTRIPE?0x0A:0x00C);
		}

		tmp = tw_read(TW6864_R_CHANNEL(TW6864_R_CROPPING_0,ch));
		tw_write(TW6864_R_CHANNEL(TW6864_R_CROPPING_F2_0,ch), tmp);

		tmp = tw_read(TW6864_R_CHANNEL(TW6864_R_HACTIVE_0,ch));
		tw_write(TW6864_R_CHANNEL(TW6864_R_HACTIVE_F2_0,ch), tmp);

		tmp = tw_read(TW6864_R_CHANNEL(TW6864_R_VACTIVE_0,ch));
		tw_write(TW6864_R_CHANNEL(TW6864_R_VACTIVE_F2_0,ch), tmp);

		tw_write(TW6864_R_CHANNEL(TW6864_R_F2_CNT_0,ch), 0x1);
	}
	else
	{
		ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_HSCALE_LO_F2_4,ch),  tmp);

		tmp = tmp2 & 0xFF;
		tw_write(TW6864_R_CHANNEL(TW6869_R_VSCALE_LO_F2_4,ch),  tmp);

		tmp = (((tmp2 >> 8)& 0xF) << 4) | ( (tmp1>>8) & 0xF );
		tw_write(TW6864_R_CHANNEL(TW6869_R_VHSCALE_HI_F2_4,ch),  tmp);

		if(IS_PAL(dev->tvnorm)) {
			tw_write(TW6864_R_CHANNEL(TW6869_R_VDELAY_F2_4,ch), 0x17);
			tw_write(TW6864_R_CHANNEL(TW6869_R_HDELAY_F2_4,ch), TW686X_REMOVE_BLACKSTRIPE?0x0A:0x00E);
		}
		else {
			tw_write(TW6864_R_CHANNEL(TW6869_R_VDELAY_F2_4,ch), 0x14);
			tw_write(TW6864_R_CHANNEL(TW6869_R_HDELAY_F2_4,ch), 0x0E);//TW686X_REMOVE_BLACKSTRIPE?0x0A:0x00C);
		}

		tmp = tw_read(TW6864_R_CHANNEL(TW6869_R_CROPPING_4,ch));
		tw_write(TW6864_R_CHANNEL(TW6869_R_CROPPING_F2_4,ch), tmp);

		tmp = tw_read(TW6864_R_CHANNEL(TW6869_R_HACTIVE_4,ch));
		tw_write(TW6864_R_CHANNEL(TW6869_R_HACTIVE_F2_4,ch), tmp);

		tmp = tw_read(TW6864_R_CHANNEL(TW6869_R_VACTIVE_4,ch));
		tw_write(TW6864_R_CHANNEL(TW6869_R_VACTIVE_F2_4,ch), tmp);

		tw_write(TW6864_R_CHANNEL(TW6869_R_F2_CNT_4,ch), 0x1);
	}

	tmp  = tw_read(TW6864_R_VIDEO_SIZE_REG(ch));
	tmp |= (1 << 30);
	tw_write(TW6864_R_VIDEO_SIZE_REG(ch), tmp);
}

void tw686x_dev_set_vdelay(struct tw686x_vdev *dev, int value)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if(ch < 4) {
		tw_write(TW6864_R_CHANNEL(TW6864_R_VDELAY_0,ch), (u32)value);
		tw_write(TW6864_R_CHANNEL(TW6864_R_VDELAY_F2_0,ch), (u32)value);
	}
	else {
	    ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_VDELAY_4,ch), (u32)value);
		tw_write(TW6864_R_CHANNEL(TW6869_R_VDELAY_F2_4,ch), (u32)value);
	}
}

void tw686x_dev_set_bright(struct tw686x_vdev *dev, int val)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;

	//if(1) { val = 0x00; }

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if(ch<4) {
		tw_write(TW6864_R_BRIGHT(ch), val);
	}
	else {
		ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_BRIGHT_4,ch), val);
	}
}

void tw686x_dev_set_contrast(struct tw686x_vdev *dev, int val)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;

	//if(1) { val = 0x64; }

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if(ch<4) {
		tw_write(TW6864_R_CONTRAST(ch),  val);
	}
	else {
		ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_CONTRAST_4,ch), val);
	}
}

void tw686x_dev_set_hue(struct tw686x_vdev *dev, int val)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;

	//if(1) { val = 0; }

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if(ch<4) {
		tw_write(TW6864_R_HUE(ch),  val);
	}
	else {
		ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_HUE_4,ch), val);
	}
}

void tw686x_dev_set_saturation(struct tw686x_vdev *dev, int val)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;

	//if(1) { val = 0x80; }

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if(ch<4) {
		tw_write(TW6864_R_SAT_U(ch),  val);
		tw_write(TW6864_R_SAT_V(ch),  val);
	}
	else {
		ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_SAT_U_4,ch), val);
		tw_write(TW6864_R_CHANNEL(TW6869_R_SAT_V_4,ch), val);
	}
}

void tw686x_dev_set_sharpness(struct tw686x_vdev *dev, int val)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;

	val &= 0x0F;
	val |= 0x10;//0xA0;

	//if(1) { val = 0x11; }

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if(ch<4) {
		tw_write(TW6864_R_SHARPNESS(ch),  val);
	}
	else {
		ch -= 4;
		tw_write(TW6864_R_CHANNEL(TW6869_R_SHARPNESS_4,ch), val);
	}
}

u32 tw686x_dev_get_decoderstatus(struct tw686x_vdev *dev)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;

	if(ch < 4) {
		return tw_read(TW6864_R_CHANNEL(TW6864_R_VSTATUS_0, ch));
	}

	return tw_read(TW6864_R_CHANNEL(TW6869_R_VSTATUS_4, ch-4));
}

int tw686x_dev_get_signal(struct tw686x_vdev *dev)
{
    u32 vstatus = tw686x_dev_get_decoderstatus( dev );

	//dvprintk(DPRT_LEVEL0, dev, "%s() vstatus=%x\n", __func__, vstatus);

    return (!(vstatus&BIT(7)) && (vstatus&BIT(6)));
}

u32 tw686x_dev_map_fieldrate(int nFieldRate, bool bNTSC)
{
	u32 uRate = 0;

	if(!bNTSC) {
		switch(nFieldRate) {
		case 0:
			uRate = 0;
			break;
		case 1:
		case 2:
			uRate = 0x00000001;
			break;
		case 3:
			uRate = 0x00004001;//0x10001000;
			break;
		case 4:
		case 5:
			uRate = 0x00104001;//0x10010010;
			break;
		case 6:
		case 7:
			uRate = 0x00404041;//0x10101010;
			break;
		case 8:
		case 9:
			uRate = 0x01041041;//0x11011010;
			break;
		case 10:
			uRate = 0x01104411;//0x11101110;
			break;
		case 11:
		case 12:
			uRate = 0x01111111;//0x11111110;
			break;
		case 13:
		case 14:
			uRate = 0x04444445;//0x11111111;
			break;
		case 15:
			uRate = 0x04511445;//0x15111111;
			break;
		case 16:
		case 17:
			uRate = 0x05145145;//0x15111511;
			break;
		case 18:
		case 19:
			uRate = 0x05151515;//0x15151511;
			break;
		case 20:
			uRate = 0x05515455;//0x15151515;
			break;
		case 21:
		case 22:
			uRate = 0x05551555;//0x15515515;
			break;
		case 23:
		case 24:
			uRate = 0x05555555;//0x15551555;
			break;
		default:
			uRate = 0x15555555;
			break;
		}
	}
	else {
		switch(nFieldRate) {
		case 0:
			uRate = 0;
			break;
		case 1:
		case 2:
		case 3:
			uRate = 0x00000001;
			break;
		case 4:
		case 5:
			uRate = 0x00004001;//0x10001000;
			break;
		case 6:
		case 7:
			uRate = 0x00104001;//0x10010010;
			break;
		case 8:
		case 9:
			uRate = 0x00404041;//0x10101010;
			break;
		case 10:
		case 11:
			uRate = 0x01041041;//0x11011010;
			break;
		case 12:
		case 13:
			uRate = 0x01104411;//0x11101110;
			break;
		case 14:
		case 15:
			uRate = 0x01111111;//0x11111110;
			break;
		case 16:
		case 17:
			uRate = 0x04444445;//0x11111111;
			break;
		case 18:
		case 19:
			uRate = 0x04511445;//0x15111111;
			break;
		case 20:
		case 21:
			uRate = 0x05145145;//0x15111511;
			break;
		case 22:
		case 23:
			uRate = 0x05151515;//0x15151511;
			break;
		case 24:
		case 25:
			uRate = 0x05515455;//0x15151515;
			break;
		case 26:
		case 27:
			uRate = 0x05551555;//0x15551555;
			break;
		case 28:
		case 29:
			uRate = 0x05555555;//0x15551555;
			break;
		default:
			uRate = 0x15555555;
			break;
		}
	}

	return uRate;
}

u32 tw686x_dev_map_framerate(int nFieldRate1, int nFieldRate2, bool bNTSC, bool b6864)
{
	u32 uRate = 0;

	if(nFieldRate1 == 0) {
		uRate = tw686x_dev_map_fieldrate(nFieldRate2, bNTSC);
	}
	else if(nFieldRate2 == 0) {
		uRate = (tw686x_dev_map_fieldrate(nFieldRate1, bNTSC) << 1);
	}
	else {
		if(b6864) {
			uRate = bNTSC ? 30 : 25;
			if(nFieldRate1>(int)uRate) nFieldRate1 = (int)uRate;
			if(nFieldRate2>(int)uRate) nFieldRate2 = (int)uRate;
			if(uRate==nFieldRate1) {
				uRate = tw686x_dev_map_fieldrate(nFieldRate1, bNTSC) |
						(tw686x_dev_map_fieldrate(nFieldRate2, bNTSC) << 1);
			}
			else {
				uRate = (tw686x_dev_map_fieldrate(nFieldRate1, bNTSC)<<1) |
						(tw686x_dev_map_fieldrate(nFieldRate2, bNTSC)<<2);
			}
		}
		else {
			uRate = tw686x_dev_map_fieldrate(nFieldRate1, bNTSC) |
					(tw686x_dev_map_fieldrate(nFieldRate2, bNTSC) << 1);
		}
	}
	if(uRate > 0) {
		uRate |= 0x80000000;
	}

	return uRate;
}

void tw686x_dev_set_framerate(struct tw686x_vdev *dev, int val, int field)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;
    u32 uRate = 0;

    if(field == V4L2_FIELD_TOP) {
        uRate = tw686x_dev_map_framerate(val, 0, !IS_PAL(dev->tvnorm), false);
    }
    else if(field == V4L2_FIELD_BOTTOM) {
        uRate = tw686x_dev_map_framerate(0,val, !IS_PAL(dev->tvnorm), false);
    }
    else {
        uRate = tw686x_dev_map_framerate(val, val, !IS_PAL(dev->tvnorm), true);
    }

	tw_write(TW6864_R_DROP_FIELD_REG(ch), uRate); //0x95555555 for even only, 0xd5555555 for odd only
}

//----------------------------------------------
// 运行或者停止DMA program
//----------------------------------------------
void tw686x_dev_run(struct tw686x_vdev *dev, bool b_run)
{
    struct tw686x_chip *chip = dev->chip;
    int  ch   = dev->channel_id;
	u32  tmp  = tw_read(TW6864_R_DMA_CMD);
	u32  tmp1 = tw_read(TW6864_R_DMA_CHANNEL_ENABLE);

	dvprintk(DPRT_LEVEL0, dev, "%s(%d %d)\n", __func__, ch, b_run);

	if(b_run) {
        tmp |= (1<<31);
        tmp |= (1<<ch);
        tmp1|= (1<<ch);
	}
	else {
		if(!(tmp&(1<<ch)) && !(tmp1&(1<<ch))) {
			return;
		}
		tmp &= ~(1<<ch);
		tmp1&= ~(1<<ch);
		if( tmp1 == 0 ) {
		    tmp = 0;
		}
	}

	tw_write(TW6864_R_DMA_CHANNEL_ENABLE, tmp1);
	tw_write(TW6864_R_DMA_CMD, tmp);
}

void tw686x_dev_set_dma(struct tw686x_vdev *dev, int width, int height, int linewidth)
{
	struct tw686x_dmadesc * dma_desc;
    struct tw686x_chip *chip = dev->chip;
    int   ch = dev->channel_id;
	bool  is4CIF = IS_PAL(dev->tvnorm) ? (height>288) : (height>240);
	int   nHeight = !is4CIF ? height : height/2;
	u32   uTemp, uConfig;
	int   linepitch = linewidth;
	int   idx_begin = ch*TW686X_DMA_DESC_UNIT;
	int	  idx_end   = (ch+1)*TW686X_DMA_DESC_UNIT-1;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d) is4CIF=%d\n", __func__, ch, is4CIF);

	/*
	 * 0 -- scatter gather, 2 -- frame block mode, 3 -- field block mode
	 */
	uTemp = (!IS_TW686X_DMA_MODE_BLOCK) ? 0 : (!is4CIF ? 3 : 2);

	uConfig  = tw_read( TW6864_R_PHASE_REF_CONFIG );
	uConfig &= ~(3<<(16+(ch<<1)));
	uConfig |= (uTemp<<(16+(ch<<1)));
	tw_write( TW6864_R_PHASE_REF_CONFIG, uConfig );

	uConfig = tw_read(TW6864_R_CH0to7_CONFIG_REG(ch));
	uConfig = (uConfig & ~0x1FF)		| (idx_begin&0x1FF);			//m_StartIdx
	uConfig = (uConfig & ~(0x3FF<<10))	| ((idx_end&0x3FF)<<10);		//m_EndIdx
	uConfig = (uConfig & ~(1<<23))		| ((false&1)<<23);				//m_bHorizontalDecimate
	uConfig = (uConfig & ~(1<<24))		| ((false&1)<<24);				//m_bVerticalDecimate
	uConfig = (uConfig & ~(3<<25))		| ((0&3)<<25);					//m_nDropChannelNum
	uConfig = (uConfig & ~(1<<27))		| ((1&1)<<27);					//m_bDropMasterOrSlave	//((((ch&1)?FALSE:TRUE)&1)<<27)
	uConfig = (uConfig & ~(1<<28))		| ((false&1)<<28);				//m_bDropField			//((bDropField&1)<<28)
	uConfig = (uConfig & ~(1<<29))		| ((false&1)<<29);				//m_bDropOddOrEven
	//uConfig = (uConfig & ~(3<<30))	| ((0&3)<<30);					//m_nCurVideoChannelNum -- mux

	tw_write(TW6864_R_CH0to7_CONFIG_REG(ch), uConfig);
	tw_write(TW6864_R_DROP_FIELD_REG(ch), 0);					        //0x95555555 for even only, 0xd5555555 for odd only

    if( !IS_TW686X_DMA_MODE_BLOCK ) {
#define DMA_STATUS_HOST_NOT_AVAIABLE    0
        dma_desc = TW686X_DMA_DESC(dev, P);
        for( uTemp=0; uTemp<TW686X_DMA_DESC_UNIT; uTemp++ ) {
            dma_desc[uTemp].ctrl = (DMA_STATUS_HOST_NOT_AVAIABLE&3)<<30;
        }
        dma_desc = TW686X_DMA_DESC(dev, B);
        for(uTemp=0; uTemp<TW686X_DMA_DESC_UNIT; uTemp++) {
            dma_desc[uTemp].ctrl = (DMA_STATUS_HOST_NOT_AVAIABLE&3)<<30;
        }
    }
    else {
        tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_P_0, ch), 0 );
        tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_B_0, ch), 0 );
        tw_write( TW6864_R_BDMA(TW6864_R_BDMA_WHP_0, ch),
                    (linewidth&0x7FF) |						//line width
                    ((linepitch&0x7FF)<<11) |				//pitch
                    ((nHeight&0x7FF)<<22)					//height
                    );
        tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_P_F2_0, ch), 0 );
        tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_B_F2_0, ch), 0 );
        tw_write( TW6864_R_BDMA(TW6864_R_BDMA_WHP_F2_0, ch),
                    (linewidth&0x7FF) |						//line width
                    ((linepitch&0x7FF)<<11) |				//pitch
                    ((nHeight&0x7FF)<<22)					//height
                    );
    }

	//field 2
	if(!is4CIF) {
		tw686x_dev_set_size(dev, width, nHeight);
        tw686x_dev_set_size2(dev, width, nHeight);
	}
	else {
		tw686x_dev_set_size2(dev, width, nHeight);
		tw686x_dev_set_size(dev, width, nHeight);
	}
}

int tw686x_dev_set_dmabuf(struct tw686x_vdev *dev, int nbuf, struct tw686x_buf *buf)
{
    struct tw686x_chip *chip = dev->chip;
    int ch = dev->channel_id;
    dma_addr_t dma_addr = buf ? videobuf_to_dma_contig_isl(&buf->vb) : 0;

 	dvprintk(DPRT_LEVEL0, dev, "%s(n_buf=%d, buf=%d 0x%08x)\n", __func__, nbuf, buf->vb.i, (u32)dma_addr);

    if( V4L2_FIELD_HAS_BOTH(dev->field) ) {
        switch( nbuf ) {
        case 0:
            tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_P_0, ch), (u32)dma_addr );
            break;
        case 1:
            tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_B_0, ch), (u32)dma_addr );
            break;
        }
    }
    else {
        switch( nbuf ) {
        case 0:
            tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_P_0, ch), (u32)dma_addr );
            break;
        case 1:
            tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_P_F2_0, ch), (u32)dma_addr );
            break;
        case 2:
            tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_B_0, ch), (u32)dma_addr );
            break;
        case 3:
            tw_write( TW6864_R_BDMA(TW6864_R_BDMA_ADDR_B_F2_0, ch), (u32)dma_addr );
            break;
        }
    }

    return 0;
}

#define TW686X_DISABLE_DESCRIPTOR(desc)     \
{                                           \
	(desc)->ctrl &= 0x3FFFFFFF;             \
	(desc)->ctrl |= 0x20000000;             \
}

#define TW686X_FILL_DESCRIPTOR(desc, phy_addr, byte_len, field_start, next_start, new_frame, check_point, status)    \
{                                           \
	(desc)->addr = phy_addr;                \
	(desc)->ctrl = ((status & 0x3) << 30) | \
		((new_frame&1)<<29)				  | \
		((next_start & 0xFF) << 21)       | \
		((field_start & 0x7F) << 14)	  | \
		((check_point&1)<<13)			  | \
		(byte_len & 0x1FFF);                \
}

#define DMA_STATUS_HOST_READY			1

int tw686x_dev_set_dmadesc(struct tw686x_vdev *dev, int nbuf, struct tw686x_buf *buf)
{
	int fld_start = false;
	int i, j, bytes_to;
	int offset=0, n_desc=1;
	struct videobuf_dmabuf *dma = videobuf_to_dma(&buf->vb);
	struct tw686x_dmadesc  *desc;
	u32  phy_addr = 0;
	int fld_num = V4L2_FIELD_HAS_BOTH(buf->vb.field) + 1;
	int fld_cnt = 0;

    n_desc += (dev->pb_next==0);

 	dvprintk(DPRT_LEVEL0, dev, "%s(%d %d), %d %p %p %p, %x\n", __func__, nbuf, buf->vb.i,
             n_desc, TW686X_DMA_DESC(dev, P), TW686X_DMA_DESC(dev, B), buf->sg, buf->sg_offset);

    for( j=0; j<n_desc; j++ ) {
        if((buf->sg==NULL) || (buf->sg_bytes_pb==buf->vb.size)) {
            buf->sg = dma->sglist;
            buf->sg_offset = 0;
            buf->sg_bytes_to = buf->vb.size;
            fld_start = true;
            fld_cnt = 0;
            offset= 0;
        }
        else {
            offset = buf->sg_offset;
        }
        bytes_to= buf->sg_bytes_pb/fld_num;
        desc  = (nbuf==0) ? TW686X_DMA_DESC(dev, P) : TW686X_DMA_DESC(dev, B);
        nbuf  = (nbuf + 1) % dev->buf_needs;
        for(i=0; i<TW686X_DMA_DESC_UNIT; i++) {
            if(bytes_to == 0) {
                TW686X_DISABLE_DESCRIPTOR(&desc[i]);
            }
            else {
                while (offset && offset >= sg_dma_len(buf->sg)) {
                    offset -= sg_dma_len(buf->sg);
                    buf->sg++;
                }
                phy_addr = cpu_to_le32(sg_dma_address(buf->sg)+offset);
                if(offset && (offset<TW686X_DMA_DESC_LEN)) {
                    offset = min(TW686X_DMA_DESC_LEN-offset, bytes_to);
                    TW686X_FILL_DESCRIPTOR(&desc[i], phy_addr, offset, 0,
                            0, fld_start, 1, DMA_STATUS_HOST_READY);
                    bytes_to-= offset;
                    offset   = TW686X_DMA_DESC_LEN;
                    //dvprintk(1, dev, "%s+(%x %x)\n", (nbuf==0)?"B":"P", desc[i].ctrl, phy_addr);
                    fld_start = false;
                }
                else if(bytes_to > TW686X_DMA_DESC_LEN) {
                    TW686X_FILL_DESCRIPTOR(&desc[i], phy_addr, TW686X_DMA_DESC_LEN, 0,
                            0, fld_start, 1, DMA_STATUS_HOST_READY);
                    bytes_to-= TW686X_DMA_DESC_LEN;
                    offset  += TW686X_DMA_DESC_LEN;
                    //dvprintk(1, dev, "%s (%x %x)\n", (nbuf==0)?"B":"P", desc[i].ctrl, phy_addr);
                    fld_start = false;
                }
                else {
                    TW686X_FILL_DESCRIPTOR(&desc[i], phy_addr, bytes_to, 0,
                            0, fld_start, 1, DMA_STATUS_HOST_READY);
                    offset  += bytes_to;
                    bytes_to = 0;
                    //dvprintk(1, dev, "%s-(%x %x)\n", (nbuf==0)?"B":"P", desc[i].ctrl, phy_addr);
                    //dvprintk(1, dev, "field %d/%d %s-(%x %x)\n", fld_cnt, fld_num, (nbuf==0)?"B":"P", desc[i].ctrl, phy_addr);

                    fld_cnt++;
                    if(fld_cnt < fld_num)
                    {
                        fld_start = true;
                        bytes_to  = buf->sg_bytes_pb/fld_num;
                    }
                }
            }
        }
        buf->sg_offset = offset;
    }

    return 0;
}

//-------------------------------------------------------------------
// 允许GPIO输出
//-------------------------------------------------------------------
bool tw686x_dev_enable_gpiooutput(struct tw686x_chip *chip, bool enable, int gpio, int bits)
{
	u32 mask= ((1 << bits) - 1) << (gpio+16);
	u32 old = tw_read(TW6864_R_GPIO_REG);
	tw_write(TW6864_R_GPIO_REG, enable ? (old&~mask) : (old|mask));

	dprintk(DPRT_LEVEL0, chip, "%s()\n", __func__);

	return true;
}

//-------------------------------------------------------------------
// 输出GPIO电平
//-------------------------------------------------------------------
bool tw686x_dev_set_gpiobits(struct tw686x_chip *chip, int gpio, int bits, u32* value)
{
	u32 mask= ((1 << bits) - 1) << gpio;
	u32 old = tw_read(TW6864_R_GPIO_REG) & ~mask;
	tw_write(TW6864_R_GPIO_REG, old|((*value<<gpio) & 0xFFFF0000));

	dprintk(DPRT_LEVEL0, chip, "%s()\n", __func__);

	return true;
}

//-------------------------------------------------------------------
// 获取GPIO电平
//-------------------------------------------------------------------
bool tw686x_dev_get_gpiobits(struct tw686x_chip *chip, int gpio, int bits, u32* pValue)
{
	*pValue = (((tw_read(TW6864_R_GPIO_REG) & 0xFFFF0000) & (((1 << bits) - 1) << gpio)) >> gpio);

	dprintk(DPRT_LEVEL0, chip, "%s()\n", __func__);

	return true;
}

int tw686x_dev_set_audio(struct tw686x_chip *chip, int samplerate, int bits, int channels, int blksize)
{
  u32 tmp1, tmp2, tmp3;

  tmp1 = tw_read(TW6864_R_AUDIO_CTRL1);

  tmp1 &= 0x0000001F;
  tmp1 |= (125000000/samplerate) << 5;
  tmp1 |= blksize << 19;

  tw_write(TW6864_R_AUDIO_CTRL1,  tmp1);

  tmp2 = ((125000000 / samplerate) << 16) +
         ((125000000 % samplerate) << 16)/samplerate;

  tw_write(TW6864_R_AUDIO_CTRL2,  tmp2);

  tmp3 = tw_read(TW6864_R_AUDIO_CTRL3);
  tmp3 &= ~BIT(8);
  tmp3 |= (bits==8) ? BIT(8) : 0;
  tw_write(TW6864_R_AUDIO_CTRL3, tmp3);

  return 0;
}

int tw686x_dev_set_adma_buffer(struct tw686x_adev *dev, u32 buf_addr, int pb)
{
    struct tw686x_chip *chip = dev->chip;

    if(pb == 0) {
        tw_write(TW6864_R_CH8to15_CONFIG_REG_P(dev->channel_id), buf_addr);
    }
    else {
        tw_write(TW6864_R_CH8to15_CONFIG_REG_B(dev->channel_id), buf_addr);
    }

	//daprintk(DPRT_LEVEL0, dev, "%s(%x %d)\n", __func__, buf_addr, pb);

    return 0;
}

//----------------------------------------------
// 运行或者停止audio DMA program
//----------------------------------------------
void tw686x_dev_run_audio(struct tw686x_adev *dev, bool b_run)
{
    struct tw686x_chip *chip = dev->chip;
    int  ch   = dev->channel_id+TW686X_AUDIO_BEGIN;
	u32  tmp  = tw_read(TW6864_R_DMA_CMD);
	u32  tmp1 = tw_read(TW6864_R_DMA_CHANNEL_ENABLE);

	daprintk(DPRT_LEVEL0, dev, "%s(%d %d)\n", __func__, ch, b_run);

	if(b_run) {
        tmp |= (1<<31);
        tmp |= (1<<ch);
        tmp1|= (1<<ch);
	}
	else {
		if(!(tmp&(1<<ch)) && !(tmp1&(1<<ch))) {
			return;
		}
		tmp &= ~(1<<ch);
		tmp1&= ~(1<<ch);
		if( tmp1 == 0 ) {
		    tmp = 0;
		}
	}

	tw_write(TW6864_R_DMA_CHANNEL_ENABLE, tmp1);
	tw_write(TW6864_R_DMA_CMD, tmp);
}

//----------------------------------------------
// Set dma p/b buffer desc
//----------------------------------------------
void tw686x_dev_set_pbdesc(struct tw686x_vdev *dev)
{
    struct tw686x_chip *chip = dev->chip;
	u32 page_table_p[TW6864_MAX_NUM_SG_DMA] = {
		TW6869_R_DMA_PAGE_TABLE0, TW6869_R_DMA_PAGE_TABLE2, TW6869_R_DMA_PAGE_TABLE4, TW6869_R_DMA_PAGE_TABLE6,
		TW6869_R_DMA_PAGE_TABLE8, TW6869_R_DMA_PAGE_TABLEA, TW6869_R_DMA_PAGE_TABLEC, TW6869_R_DMA_PAGE_TABLEE
	};
	u32 page_table_b[TW6864_MAX_NUM_SG_DMA] = {
		TW6869_R_DMA_PAGE_TABLE1, TW6869_R_DMA_PAGE_TABLE3, TW6869_R_DMA_PAGE_TABLE5, TW6869_R_DMA_PAGE_TABLE7,
		TW6869_R_DMA_PAGE_TABLE9, TW6869_R_DMA_PAGE_TABLEB, TW6869_R_DMA_PAGE_TABLED, TW6869_R_DMA_PAGE_TABLEF
	};

	tw_write(page_table_p[dev->channel_id], (u32)dev->dma_desc[TW686X_DMA_DESC_P].dma);
	tw_write(page_table_b[dev->channel_id], (u32)dev->dma_desc[TW686X_DMA_DESC_B].dma);
}
