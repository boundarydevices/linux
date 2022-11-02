// SPDX-License-Identifier: ISC

/* Infineon WLAN driver: vendor specific implement
 *
 * ©2022 Cypress Semiconductor Corporation (an Infineon company)
 * or an affiliate of Cypress Semiconductor Corporation. All rights reserved.
 * This software, including source code, documentation and related materials
 * ("Software") is owned by Cypress Semiconductor Corporation or one of its
 * affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license agreement
 * accompanying the software package from which you obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software source code
 * solely for use in connection with Cypress's integrated circuit products.
 * Any reproduction, modification, translation, compilation, or representation
 * of this Software except as specified above is prohibited without
 * the expresswritten permission of Cypress.
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT,
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * Cypress reserves the right to make changes to the Software without notice.
 * Cypress does not assume any liability arising out of the application or
 * use of the Software or any product or circuit described in the Software.
 * Cypress does not authorize its products for use in any products where a malfunction
 * or failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product").
 * By including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing so
 * agrees to indemnify Cypress against all liability.
 */

#ifndef IFX_VENDOR_H
#define IFX_VENDOR_H

#include <net/netlink.h>

/* This file is a registry of identifier assignments from the Infineon
 * OUI 00:03:19 for purposes other than MAC address assignment. New identifiers
 * can be assigned through normal review process for changes to the upstream
 * hostap.git repository.
 */
#define OUI_IFX		0x000319

/* enum ifx_nl80211_vendor_subcmds - IFX nl80211 vendor command identifiers
 *
 * @IFX_VENDOR_SCMD_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_SCMD_MAX: This acts as a the tail of cmds list.
 *      Make sure it located at the end of the list.
 */
#define SCMD(_CMD)	IFX_VENDOR_SCMD_##_CMD
#define IFX_SUBCMD(_CMD, _FLAGS, _POLICY, _FN) \
	{	\
		.vendor_id = OUI_IFX,	\
		.subcmd = SCMD(_CMD)	\
	},	\
	.flags = (_FLAGS),	\
	.policy = (_POLICY),	\
	.doit = (_FN)

enum ifx_nl80211_vendor_subcmds {
	/* TODO: IFX Vendor subcmd enum IDs between 2-15 are reserved
	 * to be filled later with BRCM Vendor subcmds that are
	 * already used by IFX.
	 *
	 * @IFX_VENDOR_SCMD_DCMD: equivilent to SCMD_PRIV_STR in DHD.
	 *
	 * @IFX_VENDOR_SCMD_FRAMEBURST: align ID to DHD.
	 */
	/* Reserved 2-5 */
	SCMD(UNSPEC)		= 0,
	SCMD(DCMD)		= 1,
	SCMD(RSV2)		= 2,
	SCMD(RSV3)		= 3,
	SCMD(RSV4)		= 4,
	SCMD(RSV5)		= 5,
	SCMD(FRAMEBURST)	= 6,
	SCMD(RSV7)		= 7,
	SCMD(RSV8)		= 8,
	SCMD(RSV9)		= 9,
	SCMD(RSV10)		= 10,
	SCMD(MUEDCA_OPT_ENABLE)		= 11,
	SCMD(LDPC_CAP)		= 12,
	SCMD(AMSDU)		= 13,
	SCMD(TWT)		= 14,
	SCMD(RSV15)		= 15,
	SCMD(BSSCOLOR)		= 16,
	SCMD(MAX)		= 17
};

/* enum ifx_vendor_attr - IFX nl80211 vendor attributes
 *
 * @IFX_VENDOR_ATTR_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_MAX: This acts as a the tail of attrs list.
 *      Make sure it located at the end of the list.
 */
enum ifx_vendor_attr {
	/* TODO: IFX Vendor attr enum IDs between 0-10 are reserved
	 * to be filled later with BRCM Vendor attrs that are
	 * already used by IFX.
	 */
	IFX_VENDOR_ATTR_UNSPEC		= 0,
	/* Reserved 1-10 */
	IFX_VENDOR_ATTR_MAX		= 11
};

/* TWT define/enum/struct
 */
/* TWT cmd version*/
#define IFX_TWT_SETUP_VER	0u
#define IFX_TWT_TEARDOWN_VER	0u
/* Flow flags */
#define IFX_TWT_FLOW_FLAG_BROADCAST	BIT(0)
#define IFX_TWT_FLOW_FLAG_IMPLICIT	BIT(1)
#define IFX_TWT_FLOW_FLAG_UNANNOUNCED	BIT(2)
#define IFX_TWT_FLOW_FLAG_TRIGGER	BIT(3)
#define IFX_TWT_FLOW_FLAG_WAKE_TBTT_NEGO BIT(4)
#define IFX_TWT_FLOW_FLAG_REQUEST	BIT(5)
#define IFX_TWT_FLOW_FLAG_PROTECT	BIT(0)
#define IFX_TWT_FLOW_FLAG_RESPONDER_PM BIT(6)
#define IFX_TWT_FLOW_FLAG_UNSOLICITED	BIT(7)
/* Flow id */
#define IFX_TWT_FLOW_ID_FID	0x07u	/* flow id */
#define IFX_TWT_FLOW_ID_GID_MASK	0x70u	/* group id - broadcast TWT only */
#define IFX_TWT_FLOW_ID_GID_SHIFT 4u
#define IFX_TWT_INV_BCAST_ID	0xFFu
#define IFX_TWT_INV_FLOW_ID	0xFFu
/* auto flow_id */
#define IFX_TWT_SETUP_FLOW_ID_AUTO	0xFFu
/* auto broadcast ID */
#define IFX_TWT_SETUP_BCAST_ID_AUTO	0xFFu
/* Infinite persistence for broadcast schedule */
#define IFX_TWT_INFINITE_BTWT_PERSIST	0xFFFFFFFFu
/* Wake type */
/* TODO: not yet finalized */
/* The time specified in wake_time_h/l is the BSS TSF time. */
#define IFX_TWT_TIME_TYPE_BSS	0u
/* The time specified in wake_time_h/l is an offset of the TSF time when the iovar is processed. */
#define IFX_TWT_TIME_TYPE_OFFSET 1u
/* The target wake time is chosen internally by the FW */
#define IFX_TWT_TIME_TYPE_AUTO 2u

/* enum ifx_vendor_attr_twt - Attributes for the TWT vendor command
 *
 * @IFX_VENDOR_ATTR_TWT_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_TWT_OPER: To specify the type of TWT operation
 *	to be performed. Uses attributes defined in enum ifx_twt_oper.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAMS: Nester attributes representing the
 *	parameters configured for TWT. These parameters are defined in
 *	the enum ifx_vendor_attr_twt_param.
 *
 * @IFX_VENDOR_ATTR_TWT_MAX: This acts as a the tail of cmds list.
 *      Make sure it located at the end of the list.
 */
enum ifx_vendor_attr_twt {
	IFX_VENDOR_ATTR_TWT_UNSPEC,
	IFX_VENDOR_ATTR_TWT_OPER,
	IFX_VENDOR_ATTR_TWT_PARAMS,
	IFX_VENDOR_ATTR_TWT_MAX
};

/* enum ifx_twt_oper - TWT operation to be specified using the vendor
 * attribute IFX_VENDOR_ATTR_TWT_OPER
 *
 * @IFX_TWT_OPER_UNSPEC: Reserved value 0
 *
 * @IFX_TWT_OPER_SETUP: Setup a TWT session. Required parameters are
 *	obtained through the nested attrs under IFX_VENDOR_ATTR_TWT_PARAMS.
 *
 * @IFX_TWT_OPER_TEARDOWN: Teardown the already negotiated TWT session.
 *	Required parameters are obtained through the nested attrs under
 *	IFX_VENDOR_ATTR_TWT_PARAMS.
 *
 * @IFX_TWT_OPER_MAX: This acts as a the tail of the list.
 *      Make sure it located at the end of the list.
 */
enum ifx_twt_oper {
	IFX_TWT_OPER_UNSPEC,
	IFX_TWT_OPER_SETUP,
	IFX_TWT_OPER_TEARDOWN,
	IFX_TWT_OPER_MAX
};

/* enum ifx_vendor_attr_twt_param - TWT parameters
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_NEGO_TYPE: Specifies the type of Negotiation to be
 *	done during Setup. The four possible types are
 *	0 - Individual TWT Negotiation
 *	1 - Wake TBTT Negotiation
 *	2 - Broadcast TWT in Beacon
 *	3 - Broadcast TWT Membership Negotiation
 *
 *	The possible values are defined in the enum ifx_twt_param_nego_type
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_SETUP_CMD_TYPE: Specifies the type of TWT Setup frame
 *	when sent by the TWT Requesting STA
 *	0 - Request
 *	1 - Suggest
 *	2 - Demand
 *
 *	when sent by the TWT Responding STA.
 *	3 - Grouping
 *	4 - Accept
 *	5 - Alternate
 *	6 - Dictate
 *	7 - Reject
 *
 *	The possible values are defined in the enum ifx_twt_oper_setup_cmd_type.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_DIALOG_TOKEN: Dialog Token used by the TWT Requesting STA to
 *	identify the TWT Setup request/response transaction.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME: Target Wake Time.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME_OFFSET: Target Wake Time Offset.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION: Nominal Minimum TWT Wake Duration.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_EXPONENT: TWT Wake Interval Exponent.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_MANTISSA: TWT Wake Interval Mantissa.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_REQUESTOR: Specify this is a TWT Requesting / Responding STA.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_TRIGGER: Specify Trigger based / Non-Trigger based TWT Session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_IMPLICIT: Specify Implicit / Explicit TWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_FLOW_TYPE: Specify Un-Announced / Announced TWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_FLOW_ID: Flow ID of an iTWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_BCAST_TWT_ID: Brocast TWT ID of a bTWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_PROTECTION: Specifies whether Tx within SP is protected.
 *	Set to 1 to indicate that TXOPs within the TWT SPs shall be initiated
 *	with a NAV protection mechanism, such as (MU) RTS/CTS or CTS-to-self frame;
 *	otherwise, it shall set it to 0.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_CHANNEL: TWT channel field which is set to 0, unless
 *	the HE STA sets up a subchannel selective transmission operation.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_TWT_INFO_FRAME_DISABLED: TWT Information frame RX handing
 *	disabled / enabled.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION_UNIT: Nominal Minimum TWT Wake Duration
 *	Unit. 0 represents unit in "256 usecs" and 1 represents unit in "TUs".
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_TEARDOWN_ALL_TWT: Teardown all negotiated TWT sessions.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_MAX: This acts as a the tail of the list.
 *      Make sure it located at the end of the list.
 */
enum ifx_vendor_attr_twt_param {
	IFX_VENDOR_ATTR_TWT_PARAM_UNSPEC,
	IFX_VENDOR_ATTR_TWT_PARAM_NEGO_TYPE,
	IFX_VENDOR_ATTR_TWT_PARAM_SETUP_CMD_TYPE,
	IFX_VENDOR_ATTR_TWT_PARAM_DIALOG_TOKEN,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME_OFFSET,
	IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_EXPONENT,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_MANTISSA,
	IFX_VENDOR_ATTR_TWT_PARAM_REQUESTOR,
	IFX_VENDOR_ATTR_TWT_PARAM_TRIGGER,
	IFX_VENDOR_ATTR_TWT_PARAM_IMPLICIT,
	IFX_VENDOR_ATTR_TWT_PARAM_FLOW_TYPE,
	IFX_VENDOR_ATTR_TWT_PARAM_FLOW_ID,
	IFX_VENDOR_ATTR_TWT_PARAM_BCAST_TWT_ID,
	IFX_VENDOR_ATTR_TWT_PARAM_PROTECTION,
	IFX_VENDOR_ATTR_TWT_PARAM_CHANNEL,
	IFX_VENDOR_ATTR_TWT_PARAM_TWT_INFO_FRAME_DISABLED,
	IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION_UNIT,
	IFX_VENDOR_ATTR_TWT_PARAM_TEARDOWN_ALL_TWT,
	IFX_VENDOR_ATTR_TWT_PARAM_MAX
};

enum ifx_twt_param_nego_type {
	IFX_TWT_PARAM_NEGO_TYPE_INVALID			= -1,
	IFX_TWT_PARAM_NEGO_TYPE_ITWT			= 0,
	IFX_TWT_PARAM_NEGO_TYPE_WAKE_TBTT		= 1,
	IFX_TWT_PARAM_NEGO_TYPE_BTWT_IE_BCN		= 2,
	IFX_TWT_PARAM_NEGO_TYPE_BTWT			= 3,
	IFX_TWT_PARAM_NEGO_TYPE_MAX			= 4
};

enum ifx_twt_oper_setup_cmd_type {
	IFX_TWT_OPER_SETUP_CMD_TYPE_INVALID	= -1,
	IFX_TWT_OPER_SETUP_CMD_TYPE_REQUEST	= 0,
	IFX_TWT_OPER_SETUP_CMD_TYPE_SUGGEST	= 1,
	IFX_TWT_OPER_SETUP_CMD_TYPE_DEMAND	= 2,
	IFX_TWT_OPER_SETUP_CMD_TYPE_GROUPING	= 3,
	IFX_TWT_OPER_SETUP_CMD_TYPE_ACCEPT	= 4,
	IFX_TWT_OPER_SETUP_CMD_TYPE_ALTERNATE	= 5,
	IFX_TWT_OPER_SETUP_CMD_TYPE_DICTATE	= 6,
	IFX_TWT_OPER_SETUP_CMD_TYPE_REJECT	= 7,
	IFX_TWT_OPER_SETUP_CMD_TYPE_MAX		= 8
};

/* TWT top level command IDs */
enum {
	IFX_TWT_CMD_ENAB = 0,
	IFX_TWT_CMD_SETUP = 1,
	IFX_TWT_CMD_TEARDOWN = 2,
	IFX_TWT_CMD_INFO = 3,
	IFX_TWT_CMD_AUTOSCHED = 4,
	IFX_TWT_CMD_STATS = 5,
	IFX_TWT_CMD_EARLY_TERM_TIME = 6,
	IFX_TWT_CMD_RESP_CONFIG = 7,
	IFX_TWT_CMD_SPPS_ENAB = 8,
	IFX_TWT_CMD_FEATURES = 9,
	IFX_TWT_CMD_LAST
};

/**
 * HE top level command IDs
 */
enum {
	IFX_HE_CMD_ENAB = 0,
	IFX_HE_CMD_FEATURES = 1,
	IFX_HE_CMD_TWT_SETUP = 2,
	IFX_HE_CMD_TWT_TEARDOWN = 3,
	IFX_HE_CMD_TWT_INFO = 4,
	IFX_HE_CMD_BSSCOLOR = 5,
	IFX_HE_CMD_PARTIAL_BSSCOLOR = 6,
	IFX_HE_CMD_CAP = 7,
	IFX_HE_CMD_STAID = 8,
	IFX_HE_CMD_RTSDURTHRESH = 10,
	IFX_HE_CMD_PEDURATION = 11,
	IFX_HE_CMD_TESTBED_MODE = 12,
	IFX_HE_CMD_OMI = 13,
	IFX_HE_CMD_MAC_PAD_DUR = 14,
	IFX_HE_CMD_MUEDCA = 15,
	IFX_HE_CMD_MACCAP = 16,
	IFX_HE_CMD_PHYCAP = 17,
	IFX_HE_CMD_DISPLAY = 18,
	IFX_HE_CMD_ACTION = 19,
	IFX_HE_CMD_OFDMATX = 20,
	IFX_HE_CMD_20IN80_MODE = 21,
	IFX_HE_CMD_SMPS = 22,
	IFX_HE_CMD_PPETHRESH = 23,
	IFX_HE_CMD_HTC_OMI_EN = 24,
	IFX_HE_CMD_ERSU_EN = 25,
	IFX_HE_CMD_PREPUNCRX_EN = 26,
	IFX_HE_CMD_MIMOCAP_EN = 27,
	IFX_HE_CMD_MUEDCA_OPT = 28,
	IFX_HE_CMD_LAST
};

static const struct nla_policy
ifx_vendor_attr_twt_param_policy[IFX_VENDOR_ATTR_TWT_PARAM_MAX + 1] = {
	[IFX_VENDOR_ATTR_TWT_PARAM_UNSPEC] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_NEGO_TYPE] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_SETUP_CMD_TYPE] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_DIALOG_TOKEN] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME] = {.type = NLA_U64},
	[IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME_OFFSET] = {.type = NLA_U64},
	[IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_EXPONENT] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_MANTISSA] = {.type = NLA_U16},
	[IFX_VENDOR_ATTR_TWT_PARAM_REQUESTOR] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_TRIGGER] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_IMPLICIT] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_FLOW_TYPE] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_FLOW_ID] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_BCAST_TWT_ID] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_PROTECTION] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_CHANNEL] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_TWT_INFO_FRAME_DISABLED] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION_UNIT] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_TEARDOWN_ALL_TWT] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAM_MAX] = {.type = NLA_U8},
};

static const struct nla_policy ifx_vendor_attr_twt_policy[IFX_VENDOR_ATTR_TWT_MAX + 1] = {
	[IFX_VENDOR_ATTR_TWT_UNSPEC] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_OPER] = {.type = NLA_U8},
	[IFX_VENDOR_ATTR_TWT_PARAMS] =
		NLA_POLICY_NESTED(ifx_vendor_attr_twt_param_policy),
	[IFX_VENDOR_ATTR_TWT_MAX] = {.type = NLA_U8},
};

struct ifx_twt {
	u8 twt_oper;
	enum ifx_twt_param_nego_type negotiation_type;
	enum ifx_twt_oper_setup_cmd_type setup_cmd;
	u8 dialog_token;
	u64 twt;
	u64 twt_offset;
	u8 min_twt;
	u8 exponent;
	u16 mantissa;
	u8 requestor;
	u8 trigger;
	u8 implicit;
	u8 flow_type;
	u8 flow_id;
	u8 bcast_twt_id;
	u8 protection;
	u8 twt_channel;
	u8 twt_info_frame_disabled;
	u8 min_twt_unit;
	u8 teardown_all_twt;
};

/* NOTES:
 * ifx_twt_sdesc is used to support both broadcast TWT and individual TWT.
 * Value in bit[0:2] in 'flow_id' field is interpreted differently:
 * - flow id for individual TWT (when IFX_TWT_FLOW_FLAG_BROADCAST bit is NOT set
 *   in 'flow_flags' field)
 * - flow id as defined in Table 8-248l1 for broadcast TWT (when
 *   IFX_TWT_FLOW_FLAG_BROADCAST bit is set)
 * In latter case other bits could be used to differentiate different flows
 * in order to support multiple broadcast TWTs with the same flow id.
 */

/* TWT Setup descriptor */
struct ifx_twt_sdesc {
	/* Setup Command. */
	u8 setup_cmd;		/* See TWT_SETUP_CMD_XXXX in 802.11ah.h */
	u8 flow_flags;		/* Flow attributes. See WL_TWT_FLOW_FLAG_XXXX below */
	u8 flow_id;		/* must be between 0 and 7. Set 0xFF for auto assignment */
	u8 wake_type;	/* See WL_TWT_TIME_TYPE_XXXX below */
	u32 wake_time_h;	/* target wake time - BSS TSF (us) */
	u32 wake_time_l;
	u32 wake_dur;	/* target wake duration in unit of microseconds */
	u32 wake_int;	/* target wake interval */
	u32 btwt_persistence;	/* Broadcast TWT Persistence */
	u32 wake_int_max;	/* max wake interval(uS) for TWT */
	u8 duty_cycle_min;	/* min duty cycle for TWT(Percentage) */
	u8 pad;
	u8 bid;		/* must be between 0 and 31. Set 0xFF for auto assignment */
	u8 channel;		/* Twt channel - Not used for now */
	u8 negotiation_type;	/* Negotiation Type: See macros TWT_NEGO_TYPE_X */
	u8 frame_recomm;	/* frame recommendation for broadcast TWTs - Not used for now	 */
};

/* twt teardown descriptor */
struct ifx_twt_teardesc {
	u8 negotiation_type;
	u8 flow_id;		/* must be between 0 and 7 */
	u8 bid;		/* must be between 0 and 31 */
	u8 alltwt;		/* all twt teardown - 0 or 1 */
};

/* HE TWT Setup command */
struct ifx_twt_setup {
	/* structure control */
	u16 version;	/* structure version */
	u16 length;	/* data length (starting after this field) */
	/* peer address */
	struct ether_addr peer;	/* leave it all 0s' for AP */
	u8 pad[2];
	/* setup descriptor */
	struct ifx_twt_sdesc desc;
};

/* HE TWT Teardown command */
struct ifx_twt_teardown {
	/* structure control */
	u16 version;	/* structure version */
	u16 length;	/* data length (starting after this field) */
	/* peer address */
	struct ether_addr peer;	/* leave it all 0s' for AP */
	struct ifx_twt_teardesc teardesc;	/* Teardown descriptor */
};

int ifx_cfg80211_vndr_cmds_twt(struct wiphy *wiphy,
			       struct wireless_dev *wdev, const void  *data, int len);
int ifx_cfg80211_vndr_cmds_bsscolor(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int len);
int ifx_cfg80211_vndr_cmds_muedca_opt(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int len);
int ifx_cfg80211_vndr_cmds_amsdu(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int len);
int ifx_cfg80211_vndr_cmds_ldpc_cap(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int len);

#endif /* IFX_VENDOR_H */

