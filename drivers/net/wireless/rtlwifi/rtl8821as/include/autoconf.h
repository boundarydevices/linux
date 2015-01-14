/******************************************************************************
 *
 * Copyright(c) 2013 Realtek Corporation. All rights reserved.
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
#define AUTOCONF_INCLUDED

/***********************************************************
 *
 * Basic Config
 *
 ***********************************************************/
// ==============================
// IC, Interface and OS Config
// ==============================
#define RTL871X_MODULE_NAME "8812AS"
#define DRV_NAME "rtl8821as"
#ifndef CONFIG_RTL8821A
#define CONFIG_RTL8821A // defined in Makefile
#endif
#define CONFIG_SDIO_HCI
#define CONFIG_RTL8821A_SDIO
#define PLATFORM_LINUX

// ==============================
// Necessary, Must define
// If not defined, this driver will fail
// ==============================
#define CONFIG_XMIT_THREAD_MODE

// ==============================
// Driver(software) Config
// ==============================
#define CONFIG_EMBEDDED_FWIMG
#define CONFIG_SDIO_TX_TASKLET
//#define CONFIG_SDIO_REDUCE_TX_POLLING
#define CONFIG_RECV_REORDERING_CTRL
#define CONFIG_SKB_COPY	//for amsdu
#define CONFIG_SDIO_RX_COPY
#define CONFIG_SDIO_TX_ENABLE_AVAL_INT

// ==============================
// Hardware/Firmware Config
// Special features support by HW/FW
// ==============================
#define CONFIG_XMIT_ACK
#define CONFIG_TX_AGGREGATION
#define RTW_NOTCH_FILTER 0 /* 0:Disable, 1:Enable, */
#define CONFIG_DEAUTH_BEFORE_CONNECT

//#define SUPPORT_HW_RFOFF_DETECTED
//#define CONFIG_SW_LED


// ==============================
// Supported functions
// (Interact with outside)
// ==============================
#define CONFIG_80211N_HT
#define CONFIG_80211AC_VHT
#define CONFIG_IOCTL_CFG80211
#define CONFIG_NEW_SIGNAL_STAT_PROCESS
#define CONFIG_80211D
#define CONFIG_DFS
#define CONFIG_AP_MODE
#define CONFIG_P2P
#define CONFIG_CONCURRENT_MODE

#define CONFIG_LAYER2_ROAMING
#define CONFIG_LAYER2_ROAMING_RESUME

// ==============================
// Issue Patch
// ==============================
// add patch compile flage here

// ==============================
// Other options
// ==============================
//#define CONFIG_MAC_LOOPBACK_DRIVER


/***********************************************************
 *
 * Debug Related Config
 *
 ***********************************************************/
/*#define CONFIG_DEBUG */ /* DBG_871X, etc... */
//#define CONFIG_DEBUG_RTL871X /* RT_TRACE, RT_PRINT_DATA, _func_enter_, _func_exit_ */
/* #define CONFIG_PROC_DEBUG */
/* #define DBG_CONFIG_ERROR_DETECT */
/* #define DBG_CONFIG_ERROR_RESET */

#define DBG 0

//#define DBG_IO
//#define DBG_DELAY_OS
//#define DBG_MEM_ALLOC
//#define DBG_IOCTL

//#define DBG_TX
//#define DBG_XMIT_BUF
//#define DBG_XMIT_BUF_EXT
//#define DBG_TX_DROP_FRAME

//#define DBG_RX_DROP_FRAME
//#define DBG_RX_SEQ
//#define DBG_RX_SIGNAL_DISPLAY_PROCESSING
//#define DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED "jeff-ap"

//#define DOWNLOAD_FW_TO_TXPKT_BUF 0
//#define DBG_HAL_INIT_PROFILING


/***********************************************************
 *
 * Auto Config Section
 * Don't write any "define" directly here.
 * "define" here must depend on some conditions which defined above
 *
 ***********************************************************/
// ==============================
// Platform depenedent
// ==============================


// ==============================
// Driver(software) Config dependent
// ==============================
// add driver config dependent here

// ==============================
// Hardware/Firmware Config dependent
// ==============================
#ifdef CONFIG_POWER_SAVING
	#define CONFIG_IPS
	#define CONFIG_IPS_CHECK_IN_WD
	#define CONFIG_LPS

	#if defined(CONFIG_LPS) && defined(CONFIG_SDIO_HCI)
		#define CONFIG_LPS_LCLK
	#endif

	#ifdef CONFIG_LPS_LCLK
		//#define CONFIG_DETECT_CPWM_BY_POLLING
		//#define CONFIG_LPS_RPWM_TIMER
		#define LPS_RPWM_WAIT_MS 300
		//#define CONFIG_LPS_LCLK_WD_TIMER // Watch Dog timer in LPS LCLK
	#endif

#endif // CONFIG_POWER_SAVING

#ifdef CONFIG_XMIT_ACK
	#define CONFIG_ACTIVE_KEEP_ALIVE_CHECK
#endif

// ==============================
// Support functions dependent
// ==============================

#ifdef CONFIG_IOCTL_CFG80211
	#define RTW_USE_CFG80211_STA_EVENT /* Indecate new sta asoc through cfg80211_new_sta */
	#define CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER
//	#define CONFIG_DEBUG_CFG80211
	#define CONFIG_SET_SCAN_DENY_TIMER
	#define CONFIG_IEEE80211_BAND_5GHZ
#endif

#ifdef CONFIG_CONCURRENT_MODE
	//#ifdef CONFIG_RTL8812A
	//	#define CONFIG_TSF_RESET_OFFLOAD		// For 2 PORT TSF SYNC.
	//#endif
	//#define CONFIG_HWPORT_SWAP				//Port0->Sec , Port1 -> Pri
	#define CONFIG_RUNTIME_PORT_SWITCH
	//#define DBG_RUNTIME_PORT_SWITCH
	#define CONFIG_STA_MODE_SCAN_UNDER_AP_MODE
#endif

#ifdef CONFIG_AP_MODE
//	#define CONFIG_INTERRUPT_BASED_TXBCN // Tx Beacon when driver early interrupt occurs
	#if defined(CONFIG_CONCURRENT_MODE) && defined(CONFIG_INTERRUPT_BASED_TXBCN)
		#undef CONFIG_INTERRUPT_BASED_TXBCN
	#endif
	#ifdef CONFIG_INTERRUPT_BASED_TXBCN
//		#define CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
		#define CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
	#endif

	#define CONFIG_NATIVEAP_MLME
	#ifndef CONFIG_NATIVEAP_MLME
		#define CONFIG_HOSTAPD_MLME
	#endif

//	#define CONFIG_FIND_BEST_CHANNEL
#endif

#ifdef CONFIG_P2P
	// The CONFIG_WFD is for supporting the Wi-Fi display
	#define CONFIG_WFD

	#define CONFIG_P2P_REMOVE_GROUP_INFO

//	#define CONFIG_DBG_P2P

	#define CONFIG_P2P_PS
	#define CONFIG_P2P_IPS
	#define CONFIG_P2P_OP_CHK_SOCIAL_CH
	#define CONFIG_CFG80211_ONECHANNEL_UNDER_CONCURRENT  //replace CONFIG_P2P_CHK_INVITE_CH_LIST flag
	#define CONFIG_P2P_INVITE_IOT
#endif

//	Added by Kurt 20110511
#ifdef CONFIG_TDLS
	#define CONFIG_TDLS_DRIVER_SETUP
//	#ifndef CONFIG_WFD
//		#define CONFIG_WFD
//	#endif
//	#define CONFIG_TDLS_AUTOSETUP
	#define CONFIG_TDLS_AUTOCHECKALIVE
	#define CONFIG_TDLS_CH_SW		/* Enable "CONFIG_TDLS_CH_SW" by default, however limit it to only work in wifi logo test mode but not in normal mode currently */
#endif

#ifdef CONFIG_BT_COEXIST
	// for ODM and outsrc BT-Coex
	#define BT_30_SUPPORT 1

	#ifndef CONFIG_LPS
		#define CONFIG_LPS	// download reserved page to FW
	#endif
#else // !CONFIG_BT_COEXIST
	#define BT_30_SUPPORT 0
#endif // !CONFIG_BT_COEXIST

// ==============================
// Other options dependent
// ==============================
#ifdef CONFIG_MAC_LOOPBACK_DRIVER
	#undef CONFIG_AP_MODE
	#undef CONFIG_NATIVEAP_MLME
	#undef CONFIG_POWER_SAVING
	#undef SUPPORT_HW_RFOFF_DETECTED
#endif

#define CONFIG_TX_MCAST2UNI	1	// Support IP multicast->unicast


#ifdef CONFIG_MP_INCLUDED
	#define MP_DRIVER		1
	#define CONFIG_MP_IWPRIV_SUPPORT

	// disable unnecessary functions for MP
//	#undef CONFIG_IPS
//	#undef CONFIG_LPS
//	#undef CONFIG_LPS_LCLK
//	#undef SUPPORT_HW_RFOFF_DETECTED
#else // !CONFIG_MP_INCLUDED
	#define MP_DRIVER		0
#endif // !CONFIG_MP_INCLUDED

#ifdef CONFIG_EFUSE_CONFIG_FILE
	#define EFUSE_FILE_PATH			"/system/etc/wifi/wifi_efuse.map"
	#define MAC_ADDRESS_FILE_PATH	"/data/wifimac.txt"
#endif


/***********************************************************
 *
 * Outsource Related Config
 *
 ***********************************************************/
#define TESTCHIP_SUPPORT				0

#define RTL8192CE_SUPPORT 				0
#define RTL8192CU_SUPPORT 				0
#define RTL8192C_SUPPORT 				(RTL8192CE_SUPPORT|RTL8192CU_SUPPORT)

#define RTL8192DE_SUPPORT 				0
#define RTL8192DU_SUPPORT 				0
#define RTL8192D_SUPPORT 				(RTL8192DE_SUPPORT|RTL8192DU_SUPPORT)

#define RTL8723_FPGA_VERIFICATION		0
#define RTL8723AU_SUPPORT				0
#define RTL8723AS_SUPPORT				0
#define RTL8723AE_SUPPORT				0
#define RTL8723A_SUPPORT				(RTL8723AU_SUPPORT|RTL8723AS_SUPPORT|RTL8723AE_SUPPORT)


#define RTL8188E_SUPPORT				0
#ifdef CONFIG_RTL8812A
#define RTL8812A_SUPPORT				1
#else
#define RTL8812A_SUPPORT				0
#endif
#ifdef CONFIG_RTL8821A
#define RTL8821A_SUPPORT				1
#else
#define RTL8821A_SUPPORT				0
#endif
#define RTL8723B_SUPPORT				0
#define RTL8192E_SUPPORT				0
#define RTL8814A_SUPPORT				0
#define 	RTL8195A_SUPPORT				0

#define RATE_ADAPTIVE_SUPPORT 			0
#define POWER_TRAINING_ACTIVE			0

#ifdef CONFIG_TX_EARLY_MODE
	#define	RTL8188E_EARLY_MODE_PKT_NUM_10	0
#endif

// ==============================
// Outsource - HAL Related Config
// ==============================
#define DISABLE_BB_RF	0

