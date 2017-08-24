/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef SII_RXAPIDEFS
#define SII_RXAPIDEFS

#define ON 1    /* Turn ON */
#define OFF 0   /* Turn OFF */


#define SII_USE_SYS_TIMER_MEASUREMENT 0
/*#define SiI_RX_DSD_Uses_I2S_SPDIF_Buses */

#define WRN_BUF_SIZE 4
#define ERR_BUF_SIZE 4

/* SiI Device Id */
enum SiIDeviceId {   /* real device id, defined by registers' maps */

	SiI9993 = 0x9942,
	SiI9031 = 0x9A53,
	SiI9021 = 0x9A52,
	SiI9011 = 0x9AA3,
	SiI9051 = 0x9051,
	SiI9023 = 0x9023,
	SiI9033 = 0x9033,
	SiI9125 = 0x9125,
	SiI9133 = 0x9133,
	SiI9135 = 0x9135

};

enum SiI_RX_DeviceId { /* small size id will be easy for processing */

	RX_SiI9993,
	RX_SiI9031,
	RX_SiI9021,
	RX_SiI9011,
	RX_SiI9051,
	RX_SiI9023,
	RX_SiI9033,
	RX_SiI9133,
	RX_SiIIP11,
	RX_SiI9135,
	RX_SiI9125,
	RX_Unknown = 0xFF
};
/* Input Color Space */

enum SiI_RX_InputColorSpace {
	SiI_RX_ICP_RGB,
	SiI_RX_ICP_YCbCr422,
	SiI_RX_ICP_YCbCr444
};

/* Color Depth */

enum SiI_RX_ColorDepth {
	SiI_RX_CD_24BPP = 0x00,
	SiI_RX_CD_30BPP = 0x01,
	SiI_RX_CD_36BPP = 0x02
};
/* bVideoPathSelect */
enum SiI_RX_Path {

	SiI_RX_P_RGB,
	SiI_RX_P_YCbCr444,
	SiI_RX_P_RGB_2PixClk,            /* SiI9011 */
	SiI_RX_P_YCbCr444_2PixClk,       /* SiI9011 */
	SiI_RX_P_RGB_48B,                /* SiI9021/31 */
	SiI_RX_P_YCbCr444_48B,           /* SiI9021/31 */
	/* This mode is used with digital output only */
	SiI_RX_P_YCbCr422,
	/* YCbCr 422 muxed 8 bit, used with digital output only */
	SiI_RX_P_YCbCr422_MUX8B,
	/* YCbCr 422 muxed 10 bit, used with digital output only */
	SiI_RX_P_YCbCr422_MUX10B,
	SiI_RX_P_YCbCr422_16B,
	SiI_RX_P_YCbCr422_20B,
	SiI_RX_P_YCbCr422_16B_2PixClk,   /* SiI9011 */
	SiI_RX_P_YCbCr422_20B_2PixClk,   /* SiI9011 */
	SiI_RX_P_Mask = 0x3F
};

enum SiI_RX_Path_Switch {

	/* For video modes with pixel replication rate Output wont be divided */
	SiI_RX_P_S_PassReplicatedPixels = 0x80,
};

/*----------------------------------------------------------------------------*/
enum SiI_RX_VidRes {
	SiI_ResNoInfo = 0xFF,
	SiI_ResNotDetected = 0xFE,
	SiI_CE_Res = 0x80,
	SiI_CE_orNextRes = 0x40
};
/*----------------------------------------------------------------------------*/
/* bSyncSelect */
enum SiI_RX_SyncSel {

	SiI_RX_SS_SeparateSync,
	SiI_RX_SS_CompOnH,            /* composite sync on HSync */
	SiI_RX_SS_CompOnV,            /* composite sync on VSync */
	SiI_RX_SS_EmbSync,            /* used with digital output only */
	SiI_RX_AVC_SOGY   /* sync on Green or Y, used with analog output only */

};
/*----------------------------------------------------------------------------*/
/* bSyncCtrl. */
enum SiI_RX_SyncCtrl {

	SiI_RX_SC_NoInv = 0x00,
	SiI_RX_SC_InvH = 0x10,             /* invert HSync polarity */
	SiI_RX_SC_InvV = 0x20              /* invert VSync polarity */

};
/*----------------------------------------------------------------------------*/
/* bAnalogCtrl. */
enum SiI_RX_AnalogVideoCtrl {

	SiI_RX_AVC_Digital_Output,
	SiI_RX_AVC_NoPedestal,
	SiI_RX_AVC_Pedestal                 /* with pedestal */
};
/*----------------------------------------------------------------------------*/
/*  bVideoOutSel */
enum SiI_RX_OutputSel {

	SiI_RX_OS_Analog  = 0x10,
	SiI_RX_OS_Digital = 0x20

};
/*----------------------------------------------------------------------------*/
enum SiI_RX_AVI_State {

	SiI_RX_NoAVI,
	SiI_RX_GotAVI

};
/*----------------------------------------------------------------------------*/
enum SiI_RX_Colorimetry {

	SiI_RX_ColorimetryNoInfo,
	SiI_RX_ITU_601,
	SiI_RX_ITU_709
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_AVMuteCtrl {

	SiI_RX_AVMuteCtrl_MuteAudio = 0x01,
	SiI_RX_AVMuteCtrl_MuteVideo = 0x10
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_GlobalStatus {
	SiI_RX_Global_HotPlugDetect = 0x01,
	SiI_RX_Global_VMute = 0x02,  /* Applicable in both HDMI and DVI modes */
	SiI_RX_GlobalHDMI_Detected = 0x04,
	SiI_RX_GlobalHDMI_AMute = 0x08,  /* Applicable in HDMI only */
	/* VG On this event upper layer should disable
	 *multi-channel digital output
	 */
	SiI_RX_GlobalHDMI_ACP = 0x10,
	SiI_RX_GlobalHDMI_NoAVIPacket = 0x20,
	SiI_RX_GlobalHDMI_Reserved    = 0x40,
	SiI_RX_Global_OldHotPlugDetect = 0x80
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_NewInfo {
	SiI_RX_NewInfo_AVI      = 0x01,
	SiI_RX_NewInfo_SPD      = 0x02,
	SiI_RX_NewInfo_AUD      = 0x04,
	SiI_RX_NewInfo_Reserved = 0x08,
	SiI_RX_NewInfo_ACP      = 0x10,
	SiI_RX_NewInfo_ISRC1    = 0x20,
	SiI_RX_NewInfo_ISRC2    = 0x40,
	SiI_RX_NewInfo_UNREQ    = 0x80
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_VideoState {

	SiI_RX_VS_PowerDown,
	SiI_RX_VS_SyncInChange,
	SiI_RX_VS_SyncStable,
	SiI_RX_VS_SyncOutOfRange,
	SiI_RX_VS_ReqVMDetection,
	SiI_RX_VS_VMDetected,
	SiI_RX_VS_VMNotDetected,
	SiI_RX_VS_ReqVideoOn,
	SiI_RX_VS_VideoOn,
	SiI_RX_VS_ReqVidInChange,
	/* it's need to accomplish some job (like Soft Audio Mute */
	SiI_RX_VS_ReqGlobalPowerDown,
	SiI_RX_VS_GlobalPowerDown,
	SiI_RX_VS_ReqPowerDown,
	SiI_RX_VS_HoldVideoMute

};
/*----------------------------------------------------------------------------*/
enum SiI_RX_Delays {
	SiI_RX_DelayAudio_WaitDACWakingUp = 20

};


/*----------------------------------------------------------------------------*/
enum SiI_AudioChannelStatus4 {

	/* 4 least significant bits */

	_22Fs = 0x04,
	_44Fs = 0x00,
	_88Fs = 0x08,
	_176Fs = 0x0C,
	_24Fs = 0x06,
	_48Fs = 0x02,
	_96Fs = 0x0A,
	_192Fs = 0x0E,
	_32Fs = 0x03,
	_768Fs = 0x09, /* YMA 8/17/06 added for HBR */
	FsNotIndicated = 0x01,

	/* 4 most significant bits */

	AccuracyLevel2 = 0x00,
	AccuracyLevel1 = 0x20,
	AccuracyLevel3 = 0x10,

	SiI_Mask_AudioChannelStatus4 = 0x3F
};
/*----------------------------------------------------------------------------*/
enum SiI_AudioChannelStatus5 {
	/* 4 least significant bits */

	AudioLengthNotIndicated = 0x00,

	AudioLength_max20_16bit = 0x02,
	AudioLength_max20_18bit = 0x04,
	AudioLength_max20_19bit = 0x08,
	AudioLength_max20_20bit = 0x0A,
	AudioLength_max20_17bit = 0x0C,

	AudioLength_max24_20bit = 0x03,
	AudioLength_max24_22bit = 0x05,
	AudioLength_max24_23bit = 0x09,
	AudioLength_max24_24bit = 0x0B,
	AudioLength_max24_21bit = 0x0D,

	AudioLength_max24bit = 0x01

};
/*----------------------------------------------------------------------------*/
enum SiI_MClk {
	MClock_128Fs = 0x00, /* Fm = 128* Fs */
	MClock_256Fs = 0x01, /* Fm = 256 * Fs (default) */
	MClock_384Fs = 0x02, /* Fm = 384 * Fs */
	MClock_512Fs = 0x03, /* Fm = 512 * Fs */

	SelectMClock = 0x03
};


/*----------------------------------------------------------------------------*/
enum SiI_ChipType {
	/*YMA removed since actually no bit for it, changed to #define
	 *    SiI_RX_DSD_Uses_I2S_SPDIF_Buses = 0x20,
	 */
	SiI_RX_InvertOutputVidClock = 0x40,
	SiI_RX_FPGA = 0x80

};
/*----------------------------------------------------------------------------*/
enum SiI_RX_AudioState {

	SiI_RX_AS_AudioOff,
	SiI_RX_AS_ReqAudio,
	SiI_RX_AS_StartCheckCTS_Stable,
	SiI_RX_AS_WaitAudioPPLLock,
	SiI_RX_AS_WaitAudioDACReady,
	SiI_RX_AS_AudioOnWaitLockStatus,
	SiI_RX_AS_AudioOn,
	SiI_RX_AS_ReqAudioOff,
	SiI_RX_AS_HoldAudioMute,
	SiI_RX_AS_WaitVideoOn,
	SiI_RX_AS_ReqHoldAudioMute /* YMA fix by added audio mute API */

};
/*----------------------------------------------------------------------------*/
enum SiI_RX_I2SBusFormat {

	SiI_RX_AOut_I2S_1BitShift = 0x0000, /* 1st. bit shift (Philips spec.) */
	SiI_RX_AOut_I2S_1BitNoShift = 0x0001,   /* no shift */
	SiI_RX_AOut_I2S_MSB1St = 0x0000,        /* MSb 1st */
	SiI_RX_AOut_I2S_LSB1St = 0x0002,            /* LSb 1st */
	SiI_RX_AOut_I2S_LJ = 0x0000,                    /* Left justified */
	SiI_RX_AOut_I2S_RJ = 0x0004,                    /* Right justified */
	SiI_RX_AOut_I2S_LeftPolWSL = 0x0000,    /* Left polarity when WS low */
	SiI_RX_AOut_I2S_LeftPolWSH = 0x0008,    /* Left polarity when WS high */
	SiI_RX_AOut_I2S_MSBSgEx = 0x0000,   /* MSb signed extended */
	SiI_RX_AOut_I2S_MSBNoSgEx = 0x00010,    /* MSb not signed extended */
	SiI_RX_AOut_I2S_WS32Bit = 0x0000,            /* WordSize 32 bits */
	SiI_RX_AOut_I2S_WS16Bit = 0x0020,            /* WordSize 16 bits */
	SiI_RX_AOut_I2S_SEPos = 0x0000,              /* Sample edge positive */
	SiI_RX_AOut_I2S_SENeg = 0x0040,              /* Sample edge negative */
	SiI_RX_AOut_I2S_PassAllData = 0x0000,        /* Pass all data */
	SiI_RX_AOut_I2S_ValData = 0x0080,            /* Pass valid data only */
	SiI_RX_AOut_I2S_PassAnyFrm = 0x0000,    /* Pass any format from SPDIF */
	SiI_RX_AOut_I2S_PassPCMOnly = 0x0100,        /* Pass PCM format only */
	SiI_RX_AOut_I2S_24BVI2S = 0x0000,            /* 24 bits send via I2S */
	SiI_RX_AOut_I2S_28BVI2S = 0x0200,            /* 28 bits send via I2S */

	SiI_RX_AOut_I2S_I2SDefault = 0x0140
};

enum SiI_RX_DSDBusFormat {
	SiI_RX_AOut_DSD_WS32Bit = 0x00,             /* WordSize 32 bits */
	SiI_RX_AOut_DSD_WS16Bit = 0x01,             /* WordSize 16 bits */
	SiI_RX_AOut_DSD_SEPos   = 0x00,             /* Sample edge positive */
	SiI_RX_AOut_DSD_SENeg   = 0x02,             /* Sample edge negative */
};

enum SiI_RX_HBRABusFormat {
	SiI_RX_AOut_HBRA_WS32Bit = 0x00,             /* WordSize 32 bits */
	SiI_RX_AOut_HBRA_WS16Bit = 0x10,             /* WordSize 16 bits */
	SiI_RX_AOut_HBRA_SEPos   = 0x00,             /* Sample edge positive */
	SiI_RX_AOut_HBRA_SENeg   = 0x20,             /* Sample edge negative */
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_AudioFIFOMap {

	SiI_RX_AOut_Map_F0_D0 = 0x00,
	SiI_RX_AOut_Map_F1_D0 = 0x01,
	SiI_RX_AOut_Map_F2_D0 = 0x02,
	SiI_RX_AOut_Map_F3_D0 = 0x03,

	SiI_RX_AOut_Map_F0_D1 = 0x00,
	SiI_RX_AOut_Map_F1_D1 = 0x04,
	SiI_RX_AOut_Map_F2_D1 = 0x08,
	SiI_RX_AOut_Map_F3_D1 = 0x0C,

	SiI_RX_AOut_Map_F0_D2 = 0x00,
	SiI_RX_AOut_Map_F1_D2 = 0x10,
	SiI_RX_AOut_Map_F2_D2 = 0x20,
	SiI_RX_AOut_Map_F3_D2 = 0x30,

	SiI_RX_AOut_Map_F0_D3 = 0x00,
	SiI_RX_AOut_Map_F1_D3 = 0x40,
	SiI_RX_AOut_Map_F2_D3 = 0x80,
	SiI_RX_AOut_Map_F3_D3 = 0xC0,

	SiI_RX_AOut_FIFOMap_Default = 0xE4
};

/*----------------------------------------------------------------------------*/
enum SiI_RX_AudioOutputSelect {

	SiI_RX_AOut_SmoothHWMute = 0x01,
	SiI_RX_AOut_SPDIF = 0x02,
	SiI_RX_AOut_I2S = 0x04,
	SiI_RX_AOut_Res = 0x08,

	SiI_RX_AOut_SD0_En = 0x100,      /* VG Added */
	SiI_RX_AOut_SD1_En = 0x200,
	SiI_RX_AOut_SD2_En = 0x400,
	SiI_RX_AOut_SD3_En = 0x800,

	SiI_RX_AOut_Default = (SiI_RX_AOut_SmoothHWMute | SiI_RX_AOut_SPDIF |
		SiI_RX_AOut_I2S | SiI_RX_AOut_SD0_En | SiI_RX_AOut_SD1_En |
		SiI_RX_AOut_SD2_En | SiI_RX_AOut_SD3_En)
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_AudioRepresentation {

	SiI_RX_AudioRepr_PCM,
	SiI_RX_AudioRepr_Compressed,
	SiI_RX_AudioRepr_DSD,
	SiI_RX_AudioRepr_HBR

	/* to be extended in future */
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_NumberAudioChannels {

	SiI_RX_NumAudCh_Unknown,
	SiI_RX_NumAudCh_2,
	SiI_RX_NumAudCh_3,
	SiI_RX_NumAudCh_4,
	SiI_RX_NumAudCh_5,
	SiI_RX_NumAudCh_6,
	SiI_RX_NumAudCh_7,
	SiI_RX_NumAudCh_8,
	SiI_RX_NumAudCh_UnknownMulti
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_TMDSState {

	SiI_RX_TMDS_NoInfo,
	SiI_RX_TMDS_DVI,
	SiI_RX_TMDS_HDMI
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_HDCPStatus {
	SiI_RX_HDCP_NotDecrypted,
	SiI_RX_HDCP_Decrypted
};

/*----------------------------------------------------------------------------*/
enum SiI_RX_TasksProperty {

	SiI_RX_Tasks_DebugOnly,
	SiI_RX_Tasks_All
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_PowerState {
	SiI_RX_Power_Off,
	SiI_RX_Power_On
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_VideoInputChannels {
	SiI_RX_VInCh1 = 0x10,
	SiI_RX_VInCh2 = 0x20,
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_AVI {
	SiI_RX_AVI_Defaults = 0,
	SiI_RX_AVI_IF = 0x80,
	SiI_RX_AVI_ColSpMask = 0x60,
	SiI_RX_AVI_ColSp_RGB = 0x00,
	SiI_RX_AVI_ColSp_YCbCr444 = 0x20,
	SiI_RX_AVI_ColSp_YCbCr422 = 0x40,
	SiI_RX_AVI_ColorimetryMask = 0x18,
	SiI_RX_AVI_Colorimetry_NoInfo = 0x00,
	SiI_RX_AVI_Colorimetry_BT601 = 0x08,
	SiI_RX_AVI_Colorimetry_BT709 = 0x10,
	SiI_RX_AVI_PixReplMask = 0x06,
	SiI_RX_AVI_PixRepl_NoRepl = 0x00,
	SiI_RX_AVI_PixRepl_x2 = 0x02,
	SiI_RX_AVI_PixRepl_x3 = 0x04,
	SiI_RX_AVI_PixRepl_x4 = 0x06
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_HDMIStatus {
	SiI_RX_HDMI_HP = 0x01,
	SiI_RX_HDMI_Enabled = 0x02,
	SiI_RX_HDMI_AMute = 0x04,
	SiI_RX_HDMI_VMute = 0x08,
	SiI_RX_HDMI_ACP = 0x10
};
/*------------------------------------------------------------------------------
 * Error or Warning Messages
 *----------------------------------------------------------------------------
 */
enum SiI_ErrWrnCode {

	SiI_EWC_OK, /* no errors and warning */
	SiI_WC_ChipNoCap,  /* W001: no support because of chip is not capable */
	SiI_WC_FrmNoCap,   /* W002: firmware has no such capability */
	SiI_WC_PCBNoCap,   /* W003: PCB has no such capability */
	/* W004: Incorrect combination of input parameters */
	SiI_WC_IncorrectCombPrm,
	SiI_WC_Colorimetry,      /* W005: Incorrect parameter of Colorimetry */
	SiI_WC_InfoFramePacketUnknownId,
	SiI_WC_CTS_Irregular,

	/* IIC Errors */
	SiI_EC_FirstIICErr = 0x81,

	SiI_EC_NoAckIIC = SiI_EC_FirstIICErr, /* E001: no Ack from SiI Device */
	SiI_EC_LowLevelIIC = 0x82,   /* E002: low level SDA or/SCL of IIC bus */
	SiI_EC_OtherIIC = 0x83,      /* E003: other errors of IIC bus */

	SiI_EC_NoAckMasterIIC = 0x84, /* E004: No Ack from Master DDC */
	SiI_EC_OtherMasterIIC = 0x85, /* E005: other errors of Master IIC bus */
	SiI_EC_LastIICErr = SiI_EC_OtherMasterIIC,

	/* CPU Errors */

	SiI_EC_NoEnoughMem = 0x86,      /* E006: not enough memory */

	/* Program Execution error */
	SiI_EC_DivZeroAttemp = 0x87,          /* E007: Div. by Zero */

	/* Video Errors */
	/* E008: Wrong parameter of Input Color Space */
	SiI_EC_InColorSpace = 0x88,

	/* Info Frame/Packet */

	SiI_EC_InfoFrameWrongLength = 0x96,
	SiI_EC_InfoFrameWrongCheckSum = 0x97,

	/* Audio Errors */

	SiI_EC_MasterClockInDiv = 0x98,
	SiI_EC_MasterClockOutDiv = 0x99,

	/* Sys timer allert */
	SiI_EC_LateCallAPIEngine = 0x9A,

	SiI_EC_Mask = 0x80

};

enum SiI_AudioErrCode {
	SiI_EC_NoAudioErrors,
	SiI_EC_NoAudioPackets = 0x89,   /* E009 No Audio packets */
	SiI_EC_CTS_OutOfRange = 0x8A,   /* E010 CTS out of range */
	/* E011 CTS Dropped (packets coming too fast) */
	SiI_EC_CTS_Dropped = 0x8B,
	/* E012 CTS Reused  (packets coming too slow) */
	SiI_EC_CTS_Reuseed = 0x8C,
	SiI_EC_NoCTS_Packets = 0x8D,    /* E013 No CTS Packets */
	SiI_EC_CTS_Changed  = 0x8E,     /* E014 CTS Changed */
	SiI_EC_ECCErrors = 0x8D,        /* E015 ECC Errors */
	SiI_EC_AudioLinkErrors  = 0x8F, /* E016 Audio Link Errors */
	SiI_EC_PLL_Unlock  = 0x90,      /* E017 PLL Unlock */
	SiI_EC_FIFO_UnderRun = 0x92,    /* E018 FIFO Under Run */
	SiI_EC_FIFO_OverRun = 0x93,     /* E019 FIFO Over Run */
	SiI_EC_FIFO_ResetFailure = 0x94,
	SiI_EC_FIFO_UnderRunStuck = 0x96,
	SiI_EC_HDCP_StuckInterrupt = 0x97,

	SiI_EC_UnsupportedColorDepth = 0x99,
	SiI_EC_ColorDepthError = 0x9A
};

/* RX API ChangeEvents  after exe SiI_RX_DoTasks() */

enum SiI_RX_API_ChangeEvents {
	SiI_RX_API_VideoSM_Changed    = 0x01,
	SiI_RX_API_AudioSM_Changed    = 0x02,
	SiI_RX_API_InfoPacket_Changed = 0x04,
	SiI_RX_API_HDCPStatus_Changed = 0x08,
	SiI_RX_API_GlobalStatus_Changed = 0x10
};
/*----------------------------------------------------------------------------*/
enum SiI_RX_VidOutputsPowerDown {

	SiI_RX_VidOutPD_NoPD,
	SiI_RX_VidOutPD_Analog,
	SiI_RX_VidOutPD_AnalogAndDigital,
	SiI_RX_VidOutPD_Digital

};


enum  IgnoreIntr {

	qcIgnoreNoAVI = 0x01,
	qcIgnoreAAC  = 0x02,
	qcIgnoreHDCPFail = 0x04

};

#endif

