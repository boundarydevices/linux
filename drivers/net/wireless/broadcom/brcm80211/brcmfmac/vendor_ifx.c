/* Infineon WLAN driver: vendor specific implement
 *
 * Copyright 2022 Cypress Semiconductor Corporation (an Infineon company)
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

#include <net/netlink.h>
#include "core.h"
#include "cfg80211.h"
#include "debug.h"
#include "fwil.h"
#include "vendor_ifx.h"

static int ifx_cfg80211_vndr_send_cmd_reply(struct wiphy *wiphy,
					    const void  *data, int len)
{
	struct sk_buff *skb;

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
	if (unlikely(!skb)) {
		brcmf_err("skb alloc failed\n");
		return -ENOMEM;
	}

	/* Push the data to the skb */
	nla_put_nohdr(skb, len, data);
	return cfg80211_vendor_cmd_reply(skb);
}

/* Wake Duration derivation from Nominal Minimum Wake Duration */
static inline u32
ifx_twt_min_twt_to_wake_dur(u8 min_twt, u8 min_twt_unit)
{
	u32 wake_dur;

	if (min_twt_unit == 1) {
		/* If min_twt_unit is 1, then min_twt is
		 * in units of TUs (i.e) 102400 usecs.
		 */
		wake_dur = (u32)min_twt * 102400;
	} else if (min_twt_unit == 0) {
		/* If min_twt_unit is 0, then min_twt is
		 * in units of 256 usecs.
		 */
		wake_dur = (u32)min_twt * 256;
	} else {
		/* Invalid min_twt */
		wake_dur = 0;
	}

	return wake_dur;
}

/* Wake Interval derivation from Wake Interval Mantissa & Exponent */
static inline u32
ifx_twt_float_to_uint32(u8 exponent, u16 mantissa)
{
	return (u32)mantissa << exponent;
}

int ifx_twt_setup(struct wireless_dev *wdev, struct ifx_twt twt)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct ifx_twt_setup val;
	s32 err;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	memset(&val, 0, sizeof(val));
	val.version = IFX_TWT_SETUP_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	/* Default values, Override Below */
	val.desc.flow_flags = 0x0;
	val.desc.wake_dur = 0xFFFFFFFF;
	val.desc.wake_int = 0xFFFFFFFF;
	val.desc.wake_int_max = 0xFFFFFFFF;

	/* Setup command */
	val.desc.setup_cmd = twt.setup_cmd;

	/* Flow flags */
	val.desc.flow_flags |= ((twt.negotiation_type & 0x02) >> 1 ?
				IFX_TWT_FLOW_FLAG_BROADCAST : 0);
	val.desc.flow_flags |= (twt.implicit ? IFX_TWT_FLOW_FLAG_IMPLICIT : 0);
	val.desc.flow_flags |= (twt.flow_type ? IFX_TWT_FLOW_FLAG_UNANNOUNCED : 0);
	val.desc.flow_flags |= (twt.trigger ? IFX_TWT_FLOW_FLAG_TRIGGER : 0);
	val.desc.flow_flags |= ((twt.negotiation_type & 0x01) ?
				IFX_TWT_FLOW_FLAG_WAKE_TBTT_NEGO : 0);
	val.desc.flow_flags |= (twt.requestor ? IFX_TWT_FLOW_FLAG_REQUEST : 0);
	val.desc.flow_flags |= (twt.protection ? IFX_TWT_FLOW_FLAG_PROTECT : 0);

	if (twt.flow_id) {
		/* Flow ID */
		val.desc.flow_id = twt.flow_id;
	} else if (twt.bcast_twt_id) {
		/* Broadcast TWT ID */
		val.desc.bid = twt.bcast_twt_id;
	} else {
		/* Let the FW choose the Flow ID, Broadcast TWT ID */
		val.desc.flow_id = 0xFF;
		val.desc.bid = 0xFF;
	}

	if (twt.twt) {
		/* Target Wake Time parameter */
		val.desc.wake_time_h = (u32)(twt.twt >> 32);
		val.desc.wake_time_l = (u32)(twt.twt);
		val.desc.wake_type = IFX_TWT_TIME_TYPE_BSS;
	} else if (twt.twt_offset) {
		/* Target Wake Time offset parameter */
		val.desc.wake_time_h = (u32)(twt.twt_offset >> 32);
		val.desc.wake_time_l = (u32)(twt.twt_offset);
		val.desc.wake_type = IFX_TWT_TIME_TYPE_OFFSET;
	} else {
		/* Let the FW choose the Target Wake Time */
		val.desc.wake_time_h = 0x0;
		val.desc.wake_time_l = 0x0;
		val.desc.wake_type = IFX_TWT_TIME_TYPE_AUTO;
	}

	/* Wake Duration or Service Period */
	val.desc.wake_dur = ifx_twt_min_twt_to_wake_dur(twt.min_twt,
							twt.min_twt_unit);
	/* Wake Interval or Service Interval */
	val.desc.wake_int = ifx_twt_float_to_uint32(twt.exponent,
						    twt.mantissa);
	/* TWT Negotiation_type */
	val.desc.negotiation_type = (u8)twt.negotiation_type;
	err = brcmf_fil_xtlv_data_set(ifp, "twt", IFX_TWT_CMD_SETUP,
				      (void *)&val, sizeof(val));

	brcmf_dbg(TRACE, "TWT setup\n"
		"Setup command	: %u\n"
		"Flow flags	: 0x %02x\n"
		"Flow ID		: %u\n"
		"Broadcast TWT ID	: %u\n"
		"Wake Time H,L	: 0x %08x %08x\n"
		"Wake Type		: %u\n"
		"Wake Dururation	: %u usecs\n"
		"Wake Interval	: %u usecs\n"
		"Negotiation type	: %u\n",
		val.desc.setup_cmd,
		val.desc.flow_flags,
		val.desc.flow_id,
		val.desc.bid,
		val.desc.wake_time_h,
		val.desc.wake_time_l,
		val.desc.wake_type,
		val.desc.wake_dur,
		val.desc.wake_int,
		val.desc.negotiation_type);

	if (err < 0)
		brcmf_err("TWT setup failed. ret:%d\n", err);

	return err;
}

int ifx_twt_teardown(struct wireless_dev *wdev, struct ifx_twt twt)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct ifx_twt_teardown val;
	s32 err;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	memset(&val, 0, sizeof(val));
	val.version = IFX_TWT_TEARDOWN_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	if (twt.flow_id) {
		/* Flow ID */
		val.teardesc.flow_id = twt.flow_id;
	} else if (twt.bcast_twt_id) {
		/* Broadcast TWT ID */
		val.teardesc.bid = twt.bcast_twt_id;
	} else {
		/* Let the FW choose the Flow ID, Broadcast TWT */
		val.teardesc.flow_id = 0xFF;
		val.teardesc.bid = 0xFF;
	}

	/* TWT Negotiation_type */
	val.teardesc.negotiation_type = (u8)twt.negotiation_type;
	/* Teardown all Negotiated TWT */
	val.teardesc.alltwt = twt.teardown_all_twt;
	err = brcmf_fil_xtlv_data_set(ifp, "twt", IFX_TWT_CMD_TEARDOWN,
				      (void *)&val, sizeof(val));

	brcmf_dbg(TRACE, "TWT teardown\n"
		"Flow ID		: %u\n"
		"Broadcast TWT ID	: %u\n"
		"Negotiation type	: %u\n"
		"Teardown all TWT	: %u\n",
		val.teardesc.flow_id,
		val.teardesc.bid,
		val.teardesc.negotiation_type,
		val.teardesc.alltwt);

	if (err < 0)
		brcmf_err("TWT teardown failed. ret:%d\n", err);

	return err;
}

int ifx_twt_oper(struct wireless_dev *wdev, struct ifx_twt twt)
{
	int ret = -1;

	switch (twt.twt_oper) {
	case IFX_TWT_OPER_SETUP:
		ret = ifx_twt_setup(wdev, twt);
		break;
	case IFX_TWT_OPER_TEARDOWN:
		ret = ifx_twt_teardown(wdev, twt);
		break;
	default:
		brcmf_err("Requested TWT operation (%d) is not supported\n",
			  twt.twt_oper);
		ret = -EINVAL;
		goto exit;
	}
exit:
	return ret;
}

static void
ifx_cfgvendor_twt_parse_params(const struct nlattr *attr_iter,
			       struct ifx_twt *twt)
{
	int tmp, twt_param;
	const struct nlattr *twt_param_iter;

	nla_for_each_nested(twt_param_iter, attr_iter, tmp) {
		twt_param = nla_type(twt_param_iter);
		switch (twt_param) {
		case IFX_VENDOR_ATTR_TWT_PARAM_NEGO_TYPE:
			twt->negotiation_type = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_SETUP_CMD_TYPE:
			twt->setup_cmd = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_DIALOG_TOKEN:
			twt->dialog_token = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME:
			twt->twt = nla_get_u64(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME_OFFSET:
			twt->twt_offset = nla_get_u64(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION:
			twt->min_twt = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_EXPONENT:
			twt->exponent = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_MANTISSA:
			twt->mantissa = nla_get_u16(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_REQUESTOR:
			twt->requestor = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_TRIGGER:
			twt->trigger = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_IMPLICIT:
			twt->implicit = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_FLOW_TYPE:
			twt->flow_type = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_FLOW_ID:
			twt->flow_id = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_BCAST_TWT_ID:
			twt->bcast_twt_id = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_PROTECTION:
			twt->protection = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_CHANNEL:
			twt->twt_channel = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_TWT_INFO_FRAME_DISABLED:
			twt->twt_info_frame_disabled = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION_UNIT:
			twt->min_twt_unit = nla_get_u8(twt_param_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAM_TEARDOWN_ALL_TWT:
			twt->teardown_all_twt = nla_get_u8(twt_param_iter);
			break;
		default:
			brcmf_dbg(TRACE, "Unknown TWT param %d, skipping\n",
				  twt_param);
			break;
		}
	}
}

int ifx_cfg80211_vndr_cmds_twt(struct wiphy *wiphy,
			       struct wireless_dev *wdev, const void  *data, int len)
{
	int tmp, attr_type;
	const struct nlattr *attr_iter;

	struct ifx_twt twt = {
		.twt_oper = 0,
		.negotiation_type = IFX_TWT_PARAM_NEGO_TYPE_ITWT,
		.setup_cmd = IFX_TWT_OPER_SETUP_CMD_TYPE_REQUEST,
		.dialog_token = 1,
		.twt = 0,
		.twt_offset = 0,
		.requestor = 1,
		.trigger = 0,
		.implicit = 1,
		.flow_type = 0,
		.flow_id = 0,
		.bcast_twt_id = 0,
		.protection = 0,
		.twt_channel = 0,
		.twt_info_frame_disabled = 0,
		.min_twt_unit = 0,
		.teardown_all_twt = 0
	};

	nla_for_each_attr(attr_iter, data, len, tmp) {
		attr_type = nla_type(attr_iter);

		switch (attr_type) {
		case IFX_VENDOR_ATTR_TWT_OPER:
			twt.twt_oper = nla_get_u8(attr_iter);
			break;
		case IFX_VENDOR_ATTR_TWT_PARAMS:
			ifx_cfgvendor_twt_parse_params(attr_iter, &twt);
			break;
		default:
			brcmf_dbg(TRACE, "Unknown TWT attribute %d, skipping\n",
				  attr_type);
			break;
		}
	}

	return ifx_twt_oper(wdev, twt);
}

int ifx_cfg80211_vndr_cmds_bsscolor(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int len)
{
	int ret = 0;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct bcm_xtlv *he_tlv;
	u8 val = *(u8 *)data;
	u8 param[8] = {0};

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;
	he_tlv = (struct bcm_xtlv *)param;
	he_tlv->id = cpu_to_le16(IFX_HE_CMD_BSSCOLOR);

	if (val == 0xa) {
		/* To get fw iovars of the form "wl he bsscolor" using iw,
		 * call the parent iovar "he" with the subcmd filled and
		 * passed along ./iw dev wlan0 vendor recv 0x000319 0x10 0xa
		 */
		ret = brcmf_fil_iovar_data_get(ifp, "he", param, sizeof(param));
		if (ret) {
			brcmf_err("get he bss_color error:%d\n", ret);
		} else {
			brcmf_dbg(INFO, "get he bss_color: %d\n", *param);
			ifx_cfg80211_vndr_send_cmd_reply(wiphy, param, 1);
		}
	} else {
		brcmf_dbg(INFO, "not support set bsscolor during runtime!\n");
	}

	return ret;
}

int ifx_cfg80211_vndr_cmds_muedca_opt(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int len)
{
	int ret = 0;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct bcm_xtlv *he_tlv;
	u8 val = *(u8 *)data;
	u8 param[8] = {0};

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;
	he_tlv = (struct bcm_xtlv *)param;
	he_tlv->id = cpu_to_le16(IFX_HE_CMD_MUEDCA_OPT);

	if (val == 0xa) {
		/* To get fw iovars of the form "wl he muedca_opt_enable"
		 * using iw, call the parent iovar "he" with the subcmd
		 * filled and passed along
		 * ./iw dev wlan0 vendor recv 0x000319 0xb 0xa
		 */
		ret = brcmf_fil_iovar_data_get(ifp, "he", param, sizeof(param));
		if (ret) {
			brcmf_err("get he muedca_opt_enable error:%d\n", ret);
		} else {
			brcmf_dbg(INFO,
				  "get he muedca_opt_enable: %d\n", *param);
			ifx_cfg80211_vndr_send_cmd_reply(wiphy, param, 1);
		}
	} else {
		he_tlv->len = cpu_to_le16(1);
		he_tlv->data[0] = val;
		ret = brcmf_fil_iovar_data_set(ifp, "he",
					       param, sizeof(param));
		if (ret)
			brcmf_err("set he muedca_opt_enable error:%d\n", ret);
	}

	return ret;
}

int ifx_cfg80211_vndr_cmds_amsdu(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int len)
{
	int ret = 0;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	int val = *(s32 *)data;
	s32 get_amsdu = 0;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	if (val == 0xa) {
		ret = brcmf_fil_iovar_int_get(ifp, "amsdu", &get_amsdu);
		if (ret) {
			brcmf_err("get amsdu error:%d\n", ret);

			return ret;
		}

		brcmf_dbg(INFO, "get amsdu: %d\n", get_amsdu);
		ifx_cfg80211_vndr_send_cmd_reply(
						wiphy, &get_amsdu, sizeof(int));
	} else {
		ret = brcmf_fil_iovar_int_set(ifp, "amsdu", val);
		if (ret)
			brcmf_err("set amsdu error:%d\n", ret);
	}

	return ret;
}

int ifx_cfg80211_vndr_cmds_ldpc_cap(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int len)
{
	int ret = 0;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	int val = *(s32 *)data;
	s32 buf = 0;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	if (val == 0xa) {
		ret = brcmf_fil_iovar_int_get(ifp, "ldpc_cap", &buf);
		if (ret) {
			brcmf_err("get ldpc_cap error:%d\n", ret);

			return ret;
		}

		brcmf_dbg(INFO, "get ldpc_cap: %d\n", buf);
		ifx_cfg80211_vndr_send_cmd_reply(wiphy, &buf, sizeof(int));
	} else {
		ret = brcmf_fil_iovar_int_set(ifp, "ldpc_cap", val);
		if (ret)
			brcmf_err("set ldpc_cap error:%d\n", ret);
	}

	return ret;
}

int ifx_cfg80211_vndr_cmds_oce_enable(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int len)
{
	int ret = 0;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct bcm_iov_buf *oce_iov;
	struct bcm_xtlv *oce_xtlv;
	u8 val = *(u8 *)data;
	u8 param[16] = {0};

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;
	oce_iov = (struct bcm_iov_buf *)param;
	oce_iov->version = cpu_to_le16(IFX_OCE_IOV_VERSION);
	oce_iov->id = cpu_to_le16(IFX_OCE_CMD_ENABLE);
	oce_xtlv = (struct bcm_xtlv *)oce_iov->data;

	if (val == 0xa) {
		/* To get fw iovars of the form "wl oce enable"
		 * using iw, call the parent iovar "oce" with the subcmd
		 * filled and passed along
		 * ./iw dev wlan0 vendor recv 0x000319 0xf 0xa
		 */
		ret = brcmf_fil_iovar_data_get(ifp, "oce",
					       param, sizeof(param));
		if (ret) {
			brcmf_err("get oce enable error:%d\n", ret);
		} else {
			brcmf_dbg(INFO,
				  "get oce enable: %d\n", oce_xtlv->data[0]);
			ifx_cfg80211_vndr_send_cmd_reply(wiphy, oce_xtlv->data,
							 sizeof(int));
		}
	} else {
		oce_iov->len = cpu_to_le16(8);
		oce_xtlv->id = cpu_to_le16(IFX_OCE_XTLV_ENABLE);
		oce_xtlv->len = cpu_to_le16(1);
		oce_xtlv->data[0] = val;
		ret = brcmf_fil_iovar_data_set(ifp, "oce",
					       param, sizeof(param));
		if (ret)
			brcmf_err("set oce enable error:%d\n", ret);
	}

	return ret;
}

int ifx_cfg80211_vndr_cmds_randmac(struct wiphy *wiphy,
				   struct wireless_dev *wdev, const void *data, int len)
{
	int ret = 0;
	struct ifx_randmac iov_buf = {0};
	u8 val = *(u8 *)data;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;
	iov_buf.version = WL_RANDMAC_API_VERSION;
	iov_buf.subcmd_id = WL_RANDMAC_SUBCMD_ENABLE;
	iov_buf.len = offsetof(struct ifx_randmac, data);

	if (val == 0x1) {
		/* To set fw iovars of the form "wl randmac enable" using iw, call the
		 * parent iovar "randmac" with the subcmd filled and passed along
		 * ./iw dev wlan0 vendor send 0x000319 0x11 0x1
		 */
		ret = brcmf_fil_bsscfg_data_set(ifp, "randmac", (void *)&iov_buf, iov_buf.len);
		if (ret)
			brcmf_err("Failed to set randmac enable: %d\n", ret);
	} else if (val == 0x0) {
		iov_buf.subcmd_id = WL_RANDMAC_SUBCMD_DISABLE;
		/* To set fw iovars of the form "wl randmac disable" using iw, call the
		 * parent iovar "randmac" with the subcmd filled and passed along
		 * ./iw dev wlan0 vendor send 0x000319 0x11 0x0
		 */
		ret = brcmf_fil_bsscfg_data_set(ifp, "randmac", (void *)&iov_buf, iov_buf.len);
		if (ret)
			brcmf_err("Failed to set randmac disable: %d\n", ret);
	} else if (val == 0xa) {
		int result_data = 0;
		struct ifx_randmac *iov_resp = NULL;
		u8 buf[64] = {0};
		/* To get fw iovars of the form "wl randmac" using iw, call the
		 * parent iovar "randmac" with the subcmd filled and passed along
		 * ./iw dev wlan0 vendor recv 0x000319 0x11 0xa
		 */
		memcpy(buf, (void *)&iov_buf, iov_buf.len);
		ret = brcmf_fil_iovar_data_get(ifp, "randmac", (void *)buf, sizeof(buf));
		if (ret) {
			brcmf_err("Failed to get randmac enable or disable: %d\n", ret);
		} else {
			iov_resp = (struct ifx_randmac *)buf;
			if (iov_resp->subcmd_id == WL_RANDMAC_SUBCMD_ENABLE)
				result_data = 1;
			ifx_cfg80211_vndr_send_cmd_reply(wiphy, &result_data, sizeof(int));
		}
	}
	return ret;
}

