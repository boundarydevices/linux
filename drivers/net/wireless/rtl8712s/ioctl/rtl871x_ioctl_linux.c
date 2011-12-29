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
#define _RTL871X_IOCTL_LINUX_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <wlan_bssdef.h>
#include <rtl871x_debug.h>
#include <wifi.h>
#include <rtl871x_mlme.h>
#include <rtl871x_ioctl.h>
#include <rtl871x_ioctl_set.h>
#include <rtl871x_ioctl_query.h>
#ifdef RFKILL_SUPPORT
#include <linux/rfkill.h>
#endif
#ifdef CONFIG_MP_INCLUDED
#include <rtl871x_mp_ioctl.h>
#endif


#define RTL_IOCTL_WPA_SUPPLICANT	SIOCIWFIRSTPRIV+30

#define SCAN_ITEM_SIZE 768
#define MAX_CUSTOM_LEN 64
#define RATE_COUNT 4


u32 rtl8180_rates[] = {1000000,2000000,5500000,11000000,
	6000000,9000000,12000000,18000000,24000000,36000000,48000000,54000000};

const long ieee80211_wlan_frequencies[] = {
	2412, 2417, 2422, 2427,
	2432, 2437, 2442, 2447,
	2452, 2457, 2462, 2467,
	2472, 2484
};

const char * const iw_operation_mode[] =
{
	"Auto", "Ad-Hoc", "Managed",  "Master", "Repeater", "Secondary", "Monitor"
};

static int hex2num_i(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int hex2byte_i(const char *hex)
{
	int a, b;
	a = hex2num_i(*hex++);
	if (a < 0)
		return -1;
	b = hex2num_i(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

/**
 * hwaddr_aton - Convert ASCII string to MAC address
 * @txt: MAC address as a string (e.g., "00:11:22:33:44:55")
 * @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 * Returns: 0 on success, -1 on failure (e.g., string not a MAC address)
 */
static int hwaddr_aton_i(const char *txt, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++) {
		int a, b;

		a = hex2num_i(*txt++);
		if (a < 0)
			return -1;
		b = hex2num_i(*txt++);
		if (b < 0)
			return -1;
		*addr++ = (a << 4) | b;
		if (i < 5 && *txt++ != ':')
			return -1;
	}

	return 0;
}

void request_wps_pbc_event(_adapter *padapter)
{
	u8 *buff, *p;
	union iwreq_data wrqu;


	buff = _malloc(IW_CUSTOM_MAX);
	if(!buff)
		return;
		
	_memset(buff, 0, IW_CUSTOM_MAX);
		
	p=buff;
		
	p+=sprintf(p, "WPS_PBC_START.request=TRUE");
		
	_memset(&wrqu,0,sizeof(wrqu));
		
	wrqu.data.length = p-buff;
		
	wrqu.data.length = (wrqu.data.length<IW_CUSTOM_MAX) ? wrqu.data.length:IW_CUSTOM_MAX;

	printk(KERN_DEBUG "%s\n", __FUNCTION__);
		
	wireless_send_event(padapter->pnetdev, IWEVCUSTOM, &wrqu, buff);

	if(buff)
	{
		_mfree(buff, IW_CUSTOM_MAX);
	}

}


void indicate_wx_assoc_event(_adapter *padapter)
{
	union iwreq_data wrqu;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;

	wrqu.ap_addr.sa_family = ARPHRD_ETHER;

	_memcpy(wrqu.ap_addr.sa_data, pmlmepriv->cur_network.network.MacAddress, ETH_ALEN);

	//printk("+indicate_wx_assoc_event\n");
	wireless_send_event(padapter->pnetdev, SIOCGIWAP, &wrqu, NULL);
}

void indicate_wx_disassoc_event(_adapter *padapter)
{
	union iwreq_data wrqu;

	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);

	//printk("+indicate_wx_disassoc_event\n");
	wireless_send_event(padapter->pnetdev, SIOCGIWAP, &wrqu, NULL);
}

/*
uint is_cckrates_included(u8 *rate)
{
	u32 i = 0;

	while (rate[i] != 0)
	{
		if ( (((rate[i]) & 0x7f) == 2) || (((rate[i]) & 0x7f) == 4) ||
			(((rate[i]) & 0x7f) == 11) || (((rate[i]) & 0x7f) == 22) )
			return _TRUE;
		i++;
	}

	return _FALSE;
}

uint is_cckratesonly_included(u8 *rate)
{
	u32 i = 0;

	while (rate[i] != 0)
	{
		if ( (((rate[i]) & 0x7f) != 2) && (((rate[i]) & 0x7f) != 4) &&
			(((rate[i]) & 0x7f) != 11)  && (((rate[i]) & 0x7f) != 22) )
			return _FALSE;
		i++;
	}

	return _TRUE;
}
*/

static inline char *translate_scan(PADAPTER padapter, struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop)
{
	struct iw_event iwe;
	u32 i = 0, ht_ielen = 0;
	u16 cap, max_rate, rate, ht_cap = _FALSE, mcs_rate = 0;
	u8 rssi, bw_40MHz = 0, short_GI = 0;
	s8 custom[MAX_CUSTOM_LEN];
	s8 *p;
	struct ieee80211_ht_cap *pht_capie;
	struct registry_priv *pregpriv = &padapter->registrypriv;
	PNDIS_WLAN_BSSID_EX pwlan = &pnetwork->network;
	

	if ( ( pwlan->Configuration.DSConfig < 1 ) || ( pnetwork->network.Configuration.DSConfig > 14 ) )
	{
		printk( "[%s] SSID = %s, DSConfig = %d\n", __FUNCTION__, pwlan->Ssid.Ssid,
								pwlan->Configuration.DSConfig );
		if ( pwlan->Configuration.DSConfig < 1 )
		{
			pwlan->Configuration.DSConfig = 1;
		}
		else
		{
			pwlan->Configuration.DSConfig = 14;
		}
	}

	/* AP MAC address */
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	_memcpy(iwe.u.ap_addr.sa_data, pwlan->MacAddress, ETH_ALEN);
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_ADDR_LEN);

	/* Add the ESSID */
#ifdef PLATFORM_ANDROID
	if (validate_ssid(&pwlan->Ssid) == _FALSE)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_err_, ("translate_scan : validate_ssid==FALSE \n"));
	}
	else
#endif
	{
		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.flags = 1;
		iwe.u.data.length = (u16)min((u16)pwlan->Ssid.SsidLength, (u16)32);
		start = iwe_stream_add_point(info, start, stop, &iwe, pwlan->Ssid.Ssid);
	}

	// parsing HT_CAP_IE
	p = get_ie(&pwlan->IEs[12], _HT_CAPABILITY_IE_, &ht_ielen, pwlan->IELength-12);
	if (p && (ht_ielen > 0))
	{
		ht_cap = _TRUE;
		pht_capie = (struct ieee80211_ht_cap*)(p + 2);
		_memcpy(&mcs_rate, pht_capie->supp_mcs_set, 2);
		bw_40MHz = (pht_capie->cap_info & IEEE80211_HT_CAP_SUP_WIDTH) ? 1:0;
		short_GI = (pht_capie->cap_info & (IEEE80211_HT_CAP_SGI_20|IEEE80211_HT_CAP_SGI_40)) ? 1:0;
	}

	/* Add the protocol name */
	iwe.cmd = SIOCGIWNAME;
	if (is_cckratesonly_included((u8*)&pwlan->SupportedRates) == _TRUE)
	{
		if (ht_cap == _TRUE)
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11bn");
		else
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11b");
	}
	else if (is_cckrates_included((u8*)&pwlan->SupportedRates) == _TRUE)
	{
		if (ht_cap == _TRUE)
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11bgn");
		else
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11bg");
	}
	else
	{
		if (ht_cap == _TRUE)
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11gn");
		else
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11g");
	}
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_CHAR_LEN);

	/* Add mode */
	iwe.cmd = SIOCGIWMODE;
	_memcpy((u8*)&cap, get_capability_from_ie(pwlan->IEs), 2);
	cap = le16_to_cpu(cap);
	if (cap & (WLAN_CAPABILITY_IBSS | WLAN_CAPABILITY_BSS)) {
		if (cap & WLAN_CAPABILITY_BSS)
			iwe.u.mode = (u32)IW_MODE_MASTER;
		else
			iwe.u.mode = (u32)IW_MODE_ADHOC;

		start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_UINT_LEN);
	}

	/* Add frequency/channel */
	iwe.cmd = SIOCGIWFREQ;
	{
		// check legel index
		u8 dsconfig = pwlan->Configuration.DSConfig;
		if(dsconfig >= 1 && dsconfig <= sizeof(ieee80211_wlan_frequencies)/sizeof(long))
		{
			iwe.u.freq.m = (s32)(ieee80211_wlan_frequencies[pnetwork->network.Configuration.DSConfig-1] * 100000);
		}	
		else
		{
			iwe.u.freq.m = 0;
		}
	}
	iwe.u.freq.e = (s16)1;
	iwe.u.freq.i = (u8)pwlan->Configuration.DSConfig;
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_FREQ_LEN);

	/* Add encryption capability */
	iwe.cmd = SIOCGIWENCODE;
	if (cap & WLAN_CAPABILITY_PRIVACY)
		iwe.u.data.flags = (u16)(IW_ENCODE_ENABLED | IW_ENCODE_NOKEY);
	else
		iwe.u.data.flags = (u16)(IW_ENCODE_DISABLED);
	iwe.u.data.length = (u16)0;
	start = iwe_stream_add_point(info, start, stop, &iwe, pwlan->Ssid.Ssid);

	/* Add basic and extended rates */
	max_rate = 0;
	p = custom;
	p += snprintf(p, MAX_CUSTOM_LEN - (p - custom), " Rates (Mb/s): ");
	while (pwlan->SupportedRates[i] != 0)
	{
		rate = pwlan->SupportedRates[i] & 0x7F;
		if (rate > max_rate)
			max_rate = rate;
		p += snprintf(p, MAX_CUSTOM_LEN - (p - custom),
			      "%d%s ", rate >> 1, (rate & 1) ? ".5" : "");
		i++;
	}

	if (ht_cap == _TRUE)
	{
		if (mcs_rate & 0x8000)//MCS15
		{
			max_rate = (bw_40MHz) ? (short_GI?300:270):(short_GI?144:130);
		}
		else if (mcs_rate & 0x0080)//MCS7
		{
			max_rate = (bw_40MHz) ? (short_GI?150:135):(short_GI?72:65);
		}
		else//default MCS7
		{
			max_rate = (bw_40MHz) ? (short_GI?150:135):(short_GI?72:65);
		}

#if 0
#ifdef CONFIG_80211N_HT
		bw_40MHz = pregpriv->cbw40_enable;
#else
		bw_40MHz = 0;
#endif

		switch (pregpriv->rf_config)
		{
			case RTL8712_RF_1T1R:
				max_rate = (bw_40MHz) ? 150:65;
				break;
			case RTL8712_RF_2T2R:
				max_rate = (bw_40MHz) ? 300:130;
				break;
			case RTL8712_RF_1T2R:
			default:
				max_rate = (bw_40MHz) ? 270:130;
				break;
		}
#endif
		max_rate *= 2;//Mbps/2;
	}

	iwe.cmd = SIOCGIWRATE;
	iwe.u.bitrate.fixed = iwe.u.bitrate.disabled = (u8)0;
	iwe.u.bitrate.value = (s32)(max_rate * 500000);
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_PARAM_LEN);

#if (WIRELESS_EXT >= 18 )
	// parsing WPA/WPA2 IE
	{
		int out_len = 0;
		u8 buf[MAX_WPA_IE_LEN];
		u16 wpa_len = 0, rsn_len = 0;
		u8 wpa_ie[255], rsn_ie[255];

		out_len = get_sec_ie(pwlan->IEs, pwlan->IELength, rsn_ie, &rsn_len, wpa_ie, &wpa_len);
		RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,
			 ("r8711_wx_get_scan: ssid=%s wpa_len=%d rsn_len=%d\n",
			  pwlan->Ssid.Ssid, wpa_len, rsn_len));

		if (wpa_len > 0)
		{
			p = buf;
			_memset(buf, 0, MAX_WPA_IE_LEN);
			p += snprintf(p, 7, "wpa_ie=");
			for (i = 0; i < wpa_len; i++) {
				p += snprintf(p, 2, "%02x", wpa_ie[i]);
			}

			_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVCUSTOM;
			iwe.u.data.length = (u16)strlen(buf);
			start = iwe_stream_add_point(info, start, stop, &iwe, buf);

			_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = (u16)wpa_len;
			start = iwe_stream_add_point(info, start, stop, &iwe, wpa_ie);
		}

		if (rsn_len > 0)
		{
			p = buf;
			_memset(buf, 0, MAX_WPA_IE_LEN);
			p += snprintf(p, 7, "rsn_ie=");
			for (i = 0; i < rsn_len; i++) {
				p += snprintf(p, 2, "%02x", rsn_ie[i]);
			}

			_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVCUSTOM;
			iwe.u.data.length = strlen(buf);
			start = iwe_stream_add_point(info, start, stop, &iwe, buf);

			_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = rsn_len;
			start = iwe_stream_add_point(info, start, stop, &iwe, rsn_ie);
		}
	}

	// parsing WPS IE
	{
		uint wps_ielen;
		u8 wps_ie[512];
		int ret;

		ret = get_wps_ie(pwlan->IEs, pwlan->IELength, wps_ie, &wps_ielen);
		if (ret == _TRUE)
		{
			if (wps_ielen > 2)
			{
				iwe.cmd = IWEVGENIE;
				iwe.u.data.length = (u16)wps_ielen;
				start = iwe_stream_add_point(info, start, stop, &iwe, wps_ie);
			}
		}
	}
#endif

	/* Add quality statistics */
	iwe.cmd = IWEVQUAL;

	rssi = signal_scale_mapping(pwlan->Rssi);

//	iwe.u.qual.updated = IW_QUAL_QUAL_UPDATED | IW_QUAL_LEVEL_UPDATED | IW_QUAL_NOISE_INVALID;

	// we only update signal_level (signal strength) that is rssi.
	iwe.u.qual.updated = (u8)(IW_QUAL_QUAL_UPDATED | IW_QUAL_LEVEL_UPDATED | IW_QUAL_NOISE_INVALID | IW_QUAL_DBM);

	//H(signal strength) = H(signal quality) + H(noise level);
	iwe.u.qual.level = (u8)((rssi+1)>>1)-95;  //signal strength
	iwe.u.qual.qual = (u8)rssi; // signal quality
	iwe.u.qual.noise = (u8)0; // noise level
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_QUAL_LEN);

	//how to translate rssi to ?%
	//rssi = (iwe.u.qual.level*2) + 190;
	//if(rssi>100) rssi = 100;
	//if(rssi<0) rssi = 0;

	return start;
}

static int wpa_set_auth_algs(struct net_device *dev, u32 value)
{
	_adapter *padapter = ( _adapter *) _netdev_priv(dev);
	int ret = 0;

	if ((value & AUTH_ALG_SHARED_KEY) && (value & AUTH_ALG_OPEN_SYSTEM))
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
			 ("wpa_set_auth_algs, AUTH_ALG_SHARED_KEY and  AUTH_ALG_OPEN_SYSTEM [value:0x%x]\n", value));
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
		padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeAutoSwitch;
		padapter->securitypriv.dot11AuthAlgrthm = 3;
	}
	else if (value & AUTH_ALG_SHARED_KEY)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
			 ("wpa_set_auth_algs, AUTH_ALG_SHARED_KEY  [value:0x%x]\n", value));
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
		padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeShared;
		padapter->securitypriv.dot11AuthAlgrthm = 1;
	}
	else if (value & AUTH_ALG_OPEN_SYSTEM)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
			 ("wpa_set_auth_algs, AUTH_ALG_OPEN_SYSTEM\n"));
		//padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
		if (padapter->securitypriv.ndisauthtype < Ndis802_11AuthModeWPAPSK)
		{
			padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeOpen;
 			padapter->securitypriv.dot11AuthAlgrthm = 0;
		}
	}
	else if (value & AUTH_ALG_LEAP) {
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
			 ("wpa_set_auth_algs, AUTH_ALG_LEAP\n"));
	} else {
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_err_,
			("wpa_set_auth_algs, error!\n"));
		ret = -EINVAL;
	}

	return ret;
}

static int wpa_set_encryption(struct net_device *dev, struct ieee_param *param, u32 param_len)
{
	int ret = 0;
	u32 wep_key_idx, wep_key_len=0;
	NDIS_802_11_WEP	 *pwep = NULL;
	_adapter *padapter = ( _adapter *) _netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct security_priv *psecuritypriv = &padapter->securitypriv;

_func_enter_;

	param->u.crypt.err = 0;
	param->u.crypt.alg[IEEE_CRYPT_ALG_NAME_LEN - 1] = '\0';

	if (param_len != (u32) ((u8 *) param->u.crypt.key - (u8 *) param) + param->u.crypt.key_len)
	{
		ret = -EINVAL;
		goto exit;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{
		if (param->u.crypt.idx >= WEP_KEYS)
		{
			ret = -EINVAL;
			goto exit;
		}
	} else {
		ret = -EINVAL;
		goto exit;
	}

	if (strcmp(param->u.crypt.alg, "WEP") == 0)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("wpa_set_encryption, crypt.alg = WEP\n"));
		printk(KERN_INFO "wpa_set_encryption, crypt.alg = WEP\n");

		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
		padapter->securitypriv.dot11PrivacyAlgrthm = _WEP40_;
		padapter->securitypriv.dot118021XGrpPrivacy = _WEP40_;

		wep_key_idx = param->u.crypt.idx;
		wep_key_len = param->u.crypt.key_len;

		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("(1)wep_key_idx=%d\n", wep_key_idx));
		printk(KERN_INFO "(1)wep_key_idx=%d\n", wep_key_idx);

		if (wep_key_idx > WEP_KEYS)
			return -EINVAL;

		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("(2)wep_key_idx=%d\n", wep_key_idx));

		if (wep_key_len > 0)
		{
			wep_key_len = wep_key_len <= 5 ? 5 : 13;

		 	pwep =(NDIS_802_11_WEP *) _malloc((u32)(wep_key_len + FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial)));
			if (pwep == NULL) {
				RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,(" wpa_set_encryption: pwep allocate fail !!!\n"));
				goto exit;
			}

			_memset(pwep, 0, sizeof(NDIS_802_11_WEP));

			pwep->KeyLength = wep_key_len;
			pwep->Length = wep_key_len + FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial);

			if (wep_key_len == 13)
			{
				padapter->securitypriv.dot11PrivacyAlgrthm=_WEP104_;
				padapter->securitypriv.dot118021XGrpPrivacy=_WEP104_;
			}
		}
		else {
			ret = -EINVAL;
			goto exit;
		}

		pwep->KeyIndex = wep_key_idx;
		pwep->KeyIndex |= 0x80000000;

		_memcpy(pwep->KeyMaterial, param->u.crypt.key, pwep->KeyLength);

		if (param->u.crypt.set_tx)
		{
			printk(KERN_INFO "wep, set_tx=1\n");

			if (set_802_11_add_wep(padapter, pwep) == (u8)_FAIL)
			{
				ret = -EOPNOTSUPP;
			}
		}
		else
		{
			printk(KERN_INFO "wep, set_tx=0\n");

			//don't update "psecuritypriv->dot11PrivacyAlgrthm" and
			//"psecuritypriv->dot11PrivacyKeyIndex=keyid", but can set_key to fw/cam

			if (wep_key_idx >= WEP_KEYS) {
				ret = -EOPNOTSUPP;
				goto exit;
			}

			_memcpy(&(psecuritypriv->dot11DefKey[wep_key_idx].skey[0]), pwep->KeyMaterial, pwep->KeyLength);
			psecuritypriv->dot11DefKeylen[wep_key_idx] = pwep->KeyLength;
			set_key(padapter, psecuritypriv, wep_key_idx);
		}

		goto exit;
	}

	if (padapter->securitypriv.dot11AuthAlgrthm == 2) // 802_1x
	{
		struct sta_info *psta, *pbcmc_sta;
		struct sta_priv *pstapriv = &padapter->stapriv;

		if (check_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_MP_STATE) == _TRUE) //sta mode
		{
			psta = get_stainfo(pstapriv, get_bssid(pmlmepriv));
			if (psta == NULL) {
				//DEBUG_ERR( ("Set wpa_set_encryption: Obtain Sta_info fail \n"));
			}
			else
			{
				psta->ieee8021x_blocked = _FALSE;

				if((padapter->securitypriv.ndisencryptstatus == Ndis802_11Encryption2Enabled)||
						(padapter->securitypriv.ndisencryptstatus ==  Ndis802_11Encryption3Enabled))
				{
					psta->dot118021XPrivacy = padapter->securitypriv.dot11PrivacyAlgrthm;
				}

				if(param->u.crypt.set_tx ==1)//pairwise key
				{
					_memcpy(psta->dot118021x_UncstKey.skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));

					if(strcmp(param->u.crypt.alg, "TKIP") == 0)//set mic key
					{
						//DEBUG_ERR(("\nset key length :param->u.crypt.key_len=%d\n", param->u.crypt.key_len));
						_memcpy(psta->dot11tkiptxmickey.skey, &(param->u.crypt.key[16]), 8);
						_memcpy(psta->dot11tkiprxmickey.skey, &(param->u.crypt.key[24]), 8);

						padapter->securitypriv.busetkipkey=_FALSE;
						_set_timer(&padapter->securitypriv.tkip_timer, 50);
					}

					//DEBUG_ERR(("\n param->u.crypt.key_len=%d\n",param->u.crypt.key_len));
					//DEBUG_ERR(("\n ~~~~stastakey:unicastkey\n"));

					setstakey_cmd(padapter, (unsigned char *)psta, _TRUE);
				}
				else//group key
				{
					if(0<param->u.crypt.idx &&  param->u.crypt.idx<3){  //group key idx is 1 or 2
				
					_memcpy(padapter->securitypriv.dot118021XGrpKey[param->u.crypt.idx-1].skey,  param->u.crypt.key,(param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));
					_memcpy(padapter->securitypriv.dot118021XGrptxmickey[param->u.crypt.idx-1].skey,&(param->u.crypt.key[16]),8);
					_memcpy(padapter->securitypriv.dot118021XGrprxmickey[param->u.crypt.idx-1].skey,&(param->u.crypt.key[24]),8);
					padapter->securitypriv.binstallGrpkey = _TRUE;
					//DEBUG_ERR(("\n param->u.crypt.key_len=%d\n", param->u.crypt.key_len));
					//DEBUG_ERR(("\n ~~~~stastakey:groupkey\n"));
					set_key(padapter,&padapter->securitypriv,param->u.crypt.idx);
#ifdef CONFIG_PWRCTRL
					if(padapter->registrypriv.power_mgnt > PS_MODE_ACTIVE){
						if(padapter->registrypriv.power_mgnt != padapter->pwrctrlpriv.pwr_mode){
							_set_timer(&(padapter->mlmepriv.dhcp_timer), 60000);
						}
					}
#endif
					}
				}
			}

			pbcmc_sta=get_bcmc_stainfo(padapter);
			if (pbcmc_sta == NULL)
			{
				//DEBUG_ERR( ("Set OID_802_11_ADD_KEY: bcmc stainfo is null \n"));
			}
			else
			{
				pbcmc_sta->ieee8021x_blocked = _FALSE;
				if((padapter->securitypriv.ndisencryptstatus == Ndis802_11Encryption2Enabled)||
						(padapter->securitypriv.ndisencryptstatus ==  Ndis802_11Encryption3Enabled))
				{
					pbcmc_sta->dot118021XPrivacy = padapter->securitypriv.dot11PrivacyAlgrthm;
				}
			}
		}
		else if(check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) //adhoc mode
		{
		}
	}

exit:

	if (pwep) {
		_mfree((u8 *)pwep, wep_key_len + FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial));
	}

_func_exit_;

	return ret;
}

static int r871x_set_wpa_ie(_adapter *padapter, char *pie, unsigned short ielen)
{
	u8 *buf = NULL, *pos = NULL;
	u32 left;
	int group_cipher = 0, pairwise_cipher = 0;
	int ret = 0;

	if ((ielen > MAX_WPA_IE_LEN) || (pie == NULL))
		return -EINVAL;

	if (ielen)
	{
		buf = _malloc(ielen);
		if (buf == NULL){
			ret = -ENOMEM;
			goto exit;
		}

		_memcpy(buf, pie , ielen);

		//dump
		{
			int i;
			printk(KERN_DEBUG "\n wpa_ie(length:%d):\n", ielen);
			for(i=0;i<ielen;i=i+8)
				printk(KERN_DEBUG "0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x \n",buf[i],buf[i+1],buf[i+2],buf[i+3],buf[i+4],buf[i+5],buf[i+6],buf[i+7]);
		}

		pos = buf;
		if (ielen < RSN_HEADER_LEN) {
			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("Ie len too short %d\n", ielen));
			ret  = -1;
			goto exit;
		}

#if 0
		pos += RSN_HEADER_LEN;
		left = ielen - RSN_HEADER_LEN;

		if (left >= RSN_SELECTOR_LEN) {
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
		else if (left > 0){
			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("Ie length mismatch, %u too much \n", left));
			ret = -1;
			goto exit;
		}
#endif

		if (parse_wpa_ie(buf, ielen, &group_cipher, &pairwise_cipher) == _SUCCESS)
		{
			padapter->securitypriv.dot11AuthAlgrthm = 2;
			padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeWPAPSK;
		}

		if (parse_wpa2_ie(buf, ielen, &group_cipher, &pairwise_cipher) == _SUCCESS)
		{
			padapter->securitypriv.dot11AuthAlgrthm = 2;
			padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeWPA2PSK;
		}

		switch (group_cipher)
		{
			case WPA_CIPHER_NONE:
				padapter->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
				break;
			case WPA_CIPHER_WEP40:
				padapter->securitypriv.dot118021XGrpPrivacy = _WEP40_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
			case WPA_CIPHER_TKIP:
				padapter->securitypriv.dot118021XGrpPrivacy = _TKIP_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
				break;
			case WPA_CIPHER_CCMP:
				padapter->securitypriv.dot118021XGrpPrivacy = _AES_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
				break;
			case WPA_CIPHER_WEP104:
				padapter->securitypriv.dot118021XGrpPrivacy = _WEP104_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
		}

		switch (pairwise_cipher)
		{
			case WPA_CIPHER_NONE:
				padapter->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
				break;
			case WPA_CIPHER_WEP40:
				padapter->securitypriv.dot11PrivacyAlgrthm = _WEP40_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
			case WPA_CIPHER_TKIP:
				padapter->securitypriv.dot11PrivacyAlgrthm = _TKIP_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
				break;
			case WPA_CIPHER_CCMP:
				padapter->securitypriv.dot11PrivacyAlgrthm = _AES_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
				break;
			case WPA_CIPHER_WEP104:
				padapter->securitypriv.dot11PrivacyAlgrthm = _WEP104_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
		}

		padapter->securitypriv.wps_phase = _FALSE;
		{//set wps_ie
			u16 cnt = 0;
			u8 eid, wps_oui[4]={0x0,0x50,0xf2,0x04};

			while (cnt < ielen)
			{
				eid = buf[cnt];

				if((eid==_VENDOR_SPECIFIC_IE_)&&(_memcmp(&buf[cnt+2], wps_oui, 4)==_TRUE))
				{
					printk(KERN_DEBUG "SET WPS_IE\n");

					padapter->securitypriv.wps_ie_len = ( (buf[cnt+1]+2) < (MAX_WPA_IE_LEN<<2)) ? (buf[cnt+1]+2):(MAX_WPA_IE_LEN<<2);

					_memcpy(padapter->securitypriv.wps_ie, &buf[cnt], padapter->securitypriv.wps_ie_len);

					padapter->securitypriv.wps_phase = _TRUE;

					printk(KERN_INFO "SET WPS_IE, wps_phase==_TRUE\n");

					cnt += buf[cnt+1]+2;

					break;
				} else {
					cnt += buf[cnt+1]+2; //goto next
				}
			}
		}
	}

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
		 ("r871x_set_wpa_ie: pairwise_cipher=0x%08x padapter->securitypriv.ndisencryptstatus=%d padapter->securitypriv.ndisauthtype=%d\n",
		  pairwise_cipher, padapter->securitypriv.ndisencryptstatus, padapter->securitypriv.ndisauthtype));

exit:

	if (buf) _mfree(buf, ielen);

	return ret;
}

static int r8711_wx_get_name(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	u16 cap;
	u32 ht_ielen = 0;
	char *p;
	u8 ht_cap=_FALSE;
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	NDIS_WLAN_BSSID_EX  *pcur_bss = &pmlmepriv->cur_network.network;
	NDIS_802_11_RATES_EX* prates = NULL;

	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("cmd_code=%x\n", info->cmd));

_func_enter_;

	if (check_fwstate(pmlmepriv, _FW_LINKED|WIFI_ADHOC_MASTER_STATE) == _TRUE)
	{
		//parsing HT_CAP_IE
		p = get_ie(&pcur_bss->IEs[12], _HT_CAPABILITY_IE_, &ht_ielen, pcur_bss->IELength-12);
		if(p && ht_ielen>0)
		{
			ht_cap = _TRUE;
		}

		prates = &pcur_bss->SupportedRates;

		if (is_cckratesonly_included((u8*)prates) == _TRUE)
		{
			if (ht_cap == _TRUE)
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bn");
			else
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11b");
		}
		else if ((is_cckrates_included((u8*)prates)) == _TRUE)
		{
			if (ht_cap == _TRUE)
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bgn");
			else
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bg");
		}
		else
		{
			if(ht_cap == _TRUE)
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11gn");
			else
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11g");
		}
	}
	else
	{
		//prates = &padapter->registrypriv.dev_network.SupportedRates;
		//snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11g");
		snprintf(wrqu->name, IFNAMSIZ, "unassociated");
	}

_func_exit_;

	return 0;
}
static const long frequency_list[] = {
    2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472, 2484,
    4915, 4920, 4925, 4935, 4940, 4945, 4960, 4980,
    5035, 5040, 5045, 5055, 5060, 5080, 5170, 5180, 5190, 5200, 5210, 5220, 5230, 5240,
    5260, 5280, 5300, 5320, 5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680,
    5700, 5745, 5765, 5785, 5805, 5825
        };

static int r8711_wx_set_freq(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct iw_freq *fwrq = & wrqu->freq;
	int rc = 0;

_func_enter_;

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_, ("+r8711_wx_set_freq\n"));
// If setting by frequency, convert to a channel
	if((fwrq->e == 1) &&
		(fwrq->m >= (int) 2.412e8) &&
		(fwrq->m <= (int) 2.487e8)) {
		int f = fwrq->m / 100000;
                 int c = 0;
                while((c < 14) && (f != frequency_list[c]))
                        c++;
		fwrq->e = 0;
		fwrq->m = c + 1;
		printk(KERN_INFO "+r8711_wx_set_freq  f=%x c=%x wrqu->e=%x wrqu->m=%x\n",f,c,fwrq->e,fwrq->m);
        }
        // Setting by channel number
	if((fwrq->m > 14) || (fwrq->e > 0))
                rc = -EOPNOTSUPP;
        else {
		int channel = fwrq->m;
                if((channel < 1) || (channel > 14)) {
			RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("%s: New channel value of %d is invalid!\n", dev->name, fwrq->m));
                        rc = -EINVAL;
                } else {
                          // Yes ! We can set it !!!
             RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, (" Set to channel = %d\n", channel));
        	padapter->registrypriv.channel=channel;  
                }
        }

_func_exit_;

	return rc;
}

static int r8711_wx_get_freq(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	NDIS_WLAN_BSSID_EX *pcur_bss = &pmlmepriv->cur_network.network;

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_, ("+r8711_wx_get_freq\n"));

	if (check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE)
	{
		u32 n = sizeof(ieee80211_wlan_frequencies)/sizeof(long);
		if ((pcur_bss->Configuration.DSConfig < 1) ||
		    (pcur_bss->Configuration.DSConfig > n))
			return -1;

		wrqu->freq.m = ieee80211_wlan_frequencies[pcur_bss->Configuration.DSConfig-1] * 100000;
		wrqu->freq.e = 1;
		wrqu->freq.i = pcur_bss->Configuration.DSConfig;
	} else
		return -1;

	return 0;
}

static int r8711_wx_set_mode(struct net_device *dev,
				struct iw_request_info *a,
				union iwreq_data *wrqu, char *b)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	NDIS_802_11_NETWORK_INFRASTRUCTURE networkType;
	int ret = 0;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r8711_wx_set_mode: mode=0x%x\n", wrqu->mode));

	switch(wrqu->mode)
	{
		case IW_MODE_AUTO: // 0
			networkType = Ndis802_11AutoUnknown;
			printk(KERN_INFO "set_mode = IW_MODE_AUTO\n");
			break;
		case IW_MODE_ADHOC: // 1
			networkType = Ndis802_11IBSS;
			printk(KERN_INFO "set_mode = IW_MODE_ADHOC\n");
			break;
		case IW_MODE_MASTER: // 3
			networkType = Ndis802_11APMode;
			printk(KERN_INFO "set_mode = IW_MODE_MASTER\n");
//			setopmode_cmd(padapter, networkType);
			break;
		case IW_MODE_INFRA: // 2
			networkType = Ndis802_11Infrastructure;
			printk(KERN_INFO "set_mode = IW_MODE_INFRA\n");
			break;

		default :
			ret = -EINVAL;
			RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_err_, ("Mode: %d is not supported\n", wrqu->mode));
//			RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_err_, ("Mode: %s is not supported\n", iw_operation_mode[wrqu->mode]));
			goto exit;
	}

	if (Ndis802_11APMode == networkType) {
		setopmode_cmd(padapter, networkType);
	} else {
		setopmode_cmd(padapter, Ndis802_11AutoUnknown);
	}

	if (set_802_11_infrastructure_mode(padapter, networkType) == _FALSE) {
		ret = -1;
		goto exit;
	}

exit:

_func_exit_;

	return ret;
}

static int r8711_wx_get_mode(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r8711_wx_get_mode\n"));

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE) == _TRUE) {
		wrqu->mode = IW_MODE_INFRA;
	} else if (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE|WIFI_ADHOC_STATE) == _TRUE) {
		wrqu->mode = IW_MODE_ADHOC;
	}
	else if(check_fwstate(pmlmepriv, WIFI_AP_STATE) == _TRUE)
	{
		wrqu->mode = IW_MODE_MASTER;
	}
	else
	{
		wrqu->mode = IW_MODE_AUTO;
	}

_func_exit_;

	return 0;
}

#if (WIRELESS_EXT >= 18)
static int r871x_wx_set_pmkid(struct net_device *dev,
			     struct iw_request_info *a,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct security_priv *psecuritypriv = &padapter->securitypriv;

	struct iw_pmksa *pPMK = (struct iw_pmksa*) extra;

	u8 strZeroMacAddress[ETH_ALEN] = { 0x00 };
	u8 strIssueBssid[ETH_ALEN] = { 0x00 };

	u8 j, blInserted = _FALSE;
	int intReturn = _FALSE;

#if 0
	struct iw_pmksa
	{
		__u32 cmd;
		struct sockaddr bssid;
		__u8 pmkid[IW_PMKID_LEN];	//IW_PMKID_LEN=16
	}
#endif

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_, ("+r871x_wx_set_pmkid\n"));
/*
	There are the BSSID information in the bssid.sa_data array.
	If cmd is IW_PMKSA_FLUSH, it means the wpa_suppplicant wants to clear all the PMKID information.
	If cmd is IW_PMKSA_ADD, it means the wpa_supplicant wants to add a PMKID/BSSID to driver.
	If cmd is IW_PMKSA_REMOVE, it means the wpa_supplicant wants to remove a PMKID/BSSID from driver.
*/

	_memcpy(strIssueBssid, pPMK->bssid.sa_data, ETH_ALEN);

	if (pPMK->cmd == IW_PMKSA_ADD)
	{
		printk(KERN_INFO "r871x_wx_set_pmkid: IW_PMKSA_ADD!\n");
		if (_memcmp(strIssueBssid, strZeroMacAddress, ETH_ALEN) == _TRUE) {
			return intReturn;
		} else {
			intReturn = _TRUE;
		}
		blInserted = _FALSE;

		//overwrite PMKID
		for (j=0 ; j<NUM_PMKID_CACHE; j++)
		{
			if (_memcmp(psecuritypriv->PMKIDList[j].Bssid, strIssueBssid, ETH_ALEN) ==_TRUE)
			{ // BSSID is matched, the same AP => rewrite with new PMKID.
				printk(KERN_INFO "r871x_wx_set_pmkid: BSSID exists in the PMKList.\n" );

				_memcpy(psecuritypriv->PMKIDList[j].PMKID, pPMK->pmkid, IW_PMKID_LEN);
				psecuritypriv->PMKIDList[j].bUsed = _TRUE;
				psecuritypriv->PMKIDIndex = j + 1; //?
				blInserted = _TRUE;
				break;
			}
		}

		if (!blInserted)
		{
			// Find a new entry
			printk(KERN_INFO "r871x_wx_set_pmkid: Use the new entry index = %d for this PMKID.\n",
				psecuritypriv->PMKIDIndex);

			_memcpy(psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].Bssid, strIssueBssid, ETH_ALEN);
			_memcpy(psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].PMKID, pPMK->pmkid, IW_PMKID_LEN);

			psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].bUsed = _TRUE;
			psecuritypriv->PMKIDIndex++ ;
			if (psecuritypriv->PMKIDIndex == NUM_PMKID_CACHE) {
				psecuritypriv->PMKIDIndex = 0;
			}
		}
	}
	else if (pPMK->cmd == IW_PMKSA_REMOVE)
	{
		printk(KERN_INFO "r871x_wx_set_pmkid: IW_PMKSA_REMOVE!\n");
		intReturn = _TRUE;
		for (j=0; j<NUM_PMKID_CACHE; j++)
		{
			if( _memcmp( psecuritypriv->PMKIDList[j].Bssid, strIssueBssid, ETH_ALEN) ==_TRUE )
			{ // BSSID is matched, the same AP => Remove this PMKID information and reset it.
				_memset( psecuritypriv->PMKIDList[ j ].Bssid, 0x00, ETH_ALEN );
				psecuritypriv->PMKIDList[ j ].bUsed = _FALSE;
				break;
			}
		}
	}
	else if (pPMK->cmd == IW_PMKSA_FLUSH)
	{
		printk(KERN_INFO "r871x_wx_set_pmkid: IW_PMKSA_FLUSH!\n");
		_memset(psecuritypriv->PMKIDList, 0, sizeof(RT_PMKID_LIST) * NUM_PMKID_CACHE);
		psecuritypriv->PMKIDIndex = 0;
		intReturn = _TRUE;
	}

	return intReturn;
}
#endif
static int r8711_wx_get_sens(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
//	_adapter *padapter = netdev_priv(dev);

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_, ("+r8711_wx_get_sens\n"));

	wrqu->sens.value = 0;
	wrqu->sens.fixed = 0;	/* no auto select */
	wrqu->sens.disabled = 1;

	return 0;
}

static int r8711_wx_get_range(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct iw_range *range = (struct iw_range *)extra;
//	_adapter *padapter = (_adapter *)netdev_priv(dev);

	u16 val;
	int i;

_func_enter_;

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_,
		 ("+r8711_wx_get_range: cmd_code=%x\n", info->cmd));

	wrqu->data.length = sizeof(*range);
	_memset(range, 0, sizeof(*range));

	/* Let's try to keep this struct in the same order as in
	 * linux/include/wireless.h
	 */

	/* TODO: See what values we can set, and remove the ones we can't
	 * set, or fill them with some default data.
	 */

	/* ~5 Mb/s real (802.11b) */
	range->throughput = 5 * 1000 * 1000;

	// TODO: Not used in 802.11b?
//	range->min_nwid;	/* Minimal NWID we are able to set */
	// TODO: Not used in 802.11b?
//	range->max_nwid;	/* Maximal NWID we are able to set */

	/* Old Frequency (backward compat - moved lower ) */
//	range->old_num_channels;
//	range->old_num_frequency;
//	range->old_freq[6]; /* Filler to keep "version" at the same offset */

	// TODO: 8711 sensitivity ?
	/* signal level threshold range */

#ifdef CONFIG_RTL8711
	range->max_qual.qual = 100;
	/* TODO: Find real max RSSI and stick here */
	range->max_qual.level = 0;
	range->max_qual.noise = -98;
	range->max_qual.updated = 7; /* Updated all three */
#endif

#ifdef CONFIG_RTL8712
	//percent values between 0 and 100.
	range->max_qual.qual = 100;
	range->max_qual.level = 0;
	range->max_qual.noise = -98;
	range->max_qual.updated = 7 | IW_QUAL_DBM; /* Updated all three */
#endif

	range->avg_qual.qual = 92; /* > 8% missed beacons is 'bad' */
	/* TODO: Find real 'good' to 'bad' threshol value for RSSI */
	range->avg_qual.level = 20 + -98;
	range->avg_qual.noise = 0;
	range->avg_qual.updated = 7 | IW_QUAL_DBM; /* Updated all three */

	range->num_bitrates = RATE_COUNT;

	for (i = 0; i < RATE_COUNT && i < IW_MAX_BITRATES; i++) {
		range->bitrate[i] = rtl8180_rates[i];
	}

	range->min_frag = MIN_FRAG_THRESHOLD;
	range->max_frag = MAX_FRAG_THRESHOLD;

	range->pm_capa = 0;

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 16;

//	range->retry_capa;	/* What retry options are supported */
//	range->retry_flags;	/* How to decode max/min retry limit */
//	range->r_time_flags;	/* How to decode max/min retry life */
//	range->min_retry;	/* Minimal number of retries */
//	range->max_retry;	/* Maximal number of retries */
//	range->min_r_time;	/* Minimal retry lifetime */
//	range->max_r_time;	/* Maximal retry lifetime */

	range->num_channels = 14;

	for (i = 0, val = 0; i < 14; i++) {

		// Include only legal frequencies for some countries
		//if ((priv->challow)[i+1]) {
			range->freq[val].i = i + 1;
			range->freq[val].m = ieee80211_wlan_frequencies[i] * 100000;
			range->freq[val].e = 1;
			val++;
		//} else {
			// FIXME: do we need to set anything for channels
			// we don't use ?
		//}

		if (val == IW_MAX_FREQUENCIES)
		break;
	}

	range->num_frequency = val;

// Commented by Albert 2009/10/13
// The following code will proivde the security capability to network manager.
// If the driver doesn't provide this capability to network manager,
// the WPA/WPA2 routers can't be choosen in the network manager.

/*
#define IW_SCAN_CAPA_NONE		0x00
#define IW_SCAN_CAPA_ESSID		0x01
#define IW_SCAN_CAPA_BSSID		0x02
#define IW_SCAN_CAPA_CHANNEL	0x04
#define IW_SCAN_CAPA_MODE		0x08
#define IW_SCAN_CAPA_RATE		0x10
#define IW_SCAN_CAPA_TYPE		0x20
#define IW_SCAN_CAPA_TIME		0x40
*/

#if WIRELESS_EXT > 17
	range->enc_capa = IW_ENC_CAPA_WPA|IW_ENC_CAPA_WPA2|
				IW_ENC_CAPA_CIPHER_TKIP|IW_ENC_CAPA_CIPHER_CCMP;
#endif

#ifdef IW_SCAN_CAPA_ESSID
	range->scan_capa = IW_SCAN_CAPA_ESSID | IW_SCAN_CAPA_TYPE |IW_SCAN_CAPA_BSSID|
					IW_SCAN_CAPA_CHANNEL|IW_SCAN_CAPA_MODE|IW_SCAN_CAPA_RATE;
#endif

_func_exit_;

	return 0;
}

static int r8711_wx_get_rate(struct net_device *dev, 
			     struct iw_request_info *info, 
			     union iwreq_data *wrqu, char *extra);

static int r871x_wx_set_priv(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *awrq,
				char *extra)
{
	int ret = 0, len = 0;
	char *ext;

	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct iw_point *dwrq = (struct iw_point*)awrq;

	#ifdef DEBUG_RTW_WX_SET_PRIV
	char *ext_dbg;
	#endif

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r871x_wx_set_priv\n"));

#ifdef PLATFORM_ANDROID

	len = dwrq->length;
	if (!(ext = _malloc(len)))
		return -ENOMEM;

	if (copy_from_user(ext, dwrq->pointer, len)) {
		_mfree(ext, len);
		return -EFAULT;
	}

	#ifdef DEBUG_RTW_WX_SET_PRIV
	if (!(ext_dbg = _malloc(len)))
		return -ENOMEM;
	
	_memcpy(ext_dbg, ext, len);
	#endif

#if 0
	if(0 == strcasecmp(ext,"START")){
		//Turn on Wi-Fi hardware
		//OK if successful
		ret=-1;
		goto FREE_EXT;
		
	}else if(0 == strcasecmp(ext,"STOP")){
		//Turn off Wi-Fi hardwoare
		//OK if successful
		ret=-1;
		goto FREE_EXT;
		
	}else
	#endif
	if(0 == strcasecmp(ext,"RSSI")){
		//Return received signal strength indicator in -db for current AP
		//<ssid> Rssi xx 
		struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);	
		struct	wlan_network	*pcur_network = &pmlmepriv->cur_network;
		//static u8 xxxx;
		if(check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE) {
			sprintf(ext, "%s rssi %d",
				pcur_network->network.Ssid.Ssid,
				//(xxxx=xxxx+10)
				((padapter->recvpriv.fw_rssi)>>1)-95
				//pcur_network->network.Rssi
				);
		} else {
			sprintf(ext, "OK");
		}
		
	}else if(0 == strcasecmp(ext,"LINKSPEED")){
		//Return link speed in MBPS
		//LinkSpeed xx 
		union iwreq_data wrqd;
		int ret_inner;
		int mbps;
		
		if( 0!=(ret_inner=r8711_wx_get_rate(dev, info, &wrqd, extra)) ){
			mbps=0;
		} else {
			mbps=wrqd.bitrate.value / 1000000;
		}
		
		sprintf(ext, "LINKSPEED %d", mbps);
		
		
	}else if(0 == strcasecmp(ext,"MACADDR")){
		//Return mac address of the station
		//Macaddr = xx.xx.xx.xx.xx.xx 
		sprintf(ext,
			"MACADDR = %02x.%02x.%02x.%02x.%02x.%02x",
			*(dev->dev_addr),*(dev->dev_addr+1),*(dev->dev_addr+2),
			*(dev->dev_addr+3),*(dev->dev_addr+4),*(dev->dev_addr+5));

	}else if(0 == strcasecmp(ext,"SCAN-ACTIVE")){
		//Set scan type to active
		//OK if successful
		struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
		pmlmepriv->passive_mode=1;
		sprintf(ext, "OK");
		
	}else if(0 == strcasecmp(ext,"SCAN-PASSIVE")){
		//Set scan type to passive
		//OK if successfu
		struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
		pmlmepriv->passive_mode=0;
		sprintf(ext, "OK");
		
	}else if(0 == strncmp(ext,"DCE-E",5)){
		//Set scan type to passive
		//OK if successfu
		
		disconnectCtrlEx_cmd(padapter
			, 1 //u32 enableDrvCtrl
			, 5 //u32 tryPktCnt
			, 100 //u32 tryPktInterval
			, 5000 //u32 firstStageTO
		);
		sprintf(ext, "OK");

	}else if(0 == strncmp(ext,"DCE-D",5)){
		//Set scan type to passive
		//OK if successfu
		
		disconnectCtrlEx_cmd(padapter
			, 0 //u32 enableDrvCtrl
			, 5 //u32 tryPktCnt
			, 100 //u32 tryPktInterval
			, 5000 //u32 firstStageTO
		);
		sprintf(ext, "OK");
		
	}else{
		#ifdef DEBUG_RTW_WX_SET_PRIV
		printk("%s: %s unknowned req=%s\n", 
		__FUNCTION__,dev->name, ext_dbg);
		#endif
		goto FREE_EXT;

	}

	if (copy_to_user(dwrq->pointer, ext, min(dwrq->length,strlen(ext)+1) ) )
		ret = -EFAULT;

	#ifdef DEBUG_RTW_WX_SET_PRIV
	printk(KERN_INFO "%s: %s req=%s rep=%s\n", 
	__FUNCTION__,dev->name, ext_dbg ,ext);
#endif

FREE_EXT:
	_mfree(ext, len);
	#ifdef DEBUG_RTW_WX_SET_PRIV
	_mfree(ext_dbg, len);
	#endif

#endif //#ifdef PLATFORM_ANDROID

	return ret;
}

//set bssid flow
//s1. set_802_11_infrastructure_mode()
//s2. set_802_11_authentication_mode()
//s3. set_802_11_encryption_mode()
//s4. set_802_11_bssid()
static int r8711_wx_set_wap(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *awrq,
			 char *extra)
{
	int ret = 0;
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	_queue *queue = &pmlmepriv->scanned_queue;

	struct sockaddr *temp = (struct sockaddr *)awrq;

	_irqL irqL;
	_list *phead;
	u8 *dst_bssid;
	struct wlan_network *pnetwork = NULL;
	NDIS_802_11_AUTHENTICATION_MODE	authmode;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r8711_wx_set_wap\n"));

	if (padapter->bup == _FALSE)
		return -1;

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY) == _TRUE)
		return -1;

	if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING) == _TRUE)
		return ret;

	if (temp->sa_family != ARPHRD_ETHER)
		return -EINVAL;

	authmode = padapter->securitypriv.ndisauthtype;

	_enter_critical(&queue->lock, &irqL);

	phead = get_list_head(queue);
	pmlmepriv->pscanned = get_next(phead);

	while (1)
	{
		if (end_of_queue_search(phead, pmlmepriv->pscanned) == _TRUE)
			break;

		pnetwork = LIST_CONTAINOR(pmlmepriv->pscanned, struct wlan_network, list);

		pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

		dst_bssid = pnetwork->network.MacAddress;

		if (_memcmp(dst_bssid, temp->sa_data, ETH_ALEN) == _TRUE)
		{
			if (set_802_11_infrastructure_mode(padapter, pnetwork->network.InfrastructureMode) == _FALSE)
				ret = -1;

			break;
		}
	}

	_exit_critical(&queue->lock, &irqL);

	if (!ret) {
		if (set_802_11_authentication_mode(padapter, authmode) == _FALSE)
			ret = -1;
		else {
//			set_802_11_encryption_mode(padapter, padapter->securitypriv.ndisencryptstatus);

			if (set_802_11_bssid(padapter, temp->sa_data) == _FALSE) {
				ret = -1;
				RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_err_, ("-r8711_wx_set_wap: set bssid fail!\n"));
			}
		}
	}

_func_exit_;

	return ret;
}

static int r8711_wx_get_wap(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	NDIS_WLAN_BSSID_EX *pcur_bss = &pmlmepriv->cur_network.network;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r8711_wx_get_wap\n"));

	wrqu->ap_addr.sa_family = ARPHRD_ETHER;
	_memset(wrqu->ap_addr.sa_data, 0, ETH_ALEN);

	if (check_fwstate(pmlmepriv, _FW_LINKED|WIFI_ADHOC_MASTER_STATE|WIFI_AP_STATE) == _TRUE) {
		_memcpy(wrqu->ap_addr.sa_data, pcur_bss->MacAddress, ETH_ALEN);
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
			 ("-r8711_wx_get_wap: AP=%02x:%02x:%02x:%02x:%02x:%02x\n",
			  pcur_bss->MacAddress[0], pcur_bss->MacAddress[1], pcur_bss->MacAddress[2],
			  pcur_bss->MacAddress[3], pcur_bss->MacAddress[4], pcur_bss->MacAddress[5]));
	}

_func_exit_;

	return 0;
}

#if (WIRELESS_EXT >= 18)
static int r871x_wx_set_mlme(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
#if 0
/* SIOCSIWMLME data */
struct	iw_mlme
{
	__u16		cmd; /* IW_MLME_* */
	__u16		reason_code;
	struct sockaddr	addr;
};
#endif

	int ret = 0;
	u16 reason;
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct iw_mlme *mlme = (struct iw_mlme *) extra;


	if (mlme == NULL)
		return -1;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("+r871x_wx_set_mlme: cmd=%d\n", mlme->cmd));

	reason = cpu_to_le16(mlme->reason_code); //?

	switch (mlme->cmd)
	{
		case IW_MLME_DEAUTH:
			if (!set_802_11_disassociate(padapter))
				ret = -1;
			break;

		case IW_MLME_DISASSOC:
			if (!set_802_11_disassociate(padapter))
				ret = -1;

			break;

		default:
			return -EOPNOTSUPP;
	}

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("-r871x_wx_set_mlme: ret=%d\n", ret));

	return ret;
}
#endif

int r8711_wx_set_scan(struct net_device *dev,
			struct iw_request_info *a,
			union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;

	u8 status = _TRUE;
	int ret = 0;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r8711_wx_set_scan\n"));

	if (padapter->bDriverStopped == _TRUE) {
		printk("!r8711_wx_set_scan: bDriverStopped=%d\n", padapter->bDriverStopped);
		ret = -EPERM;
		goto exit;
	}

	if (padapter->bup == _FALSE) {
		ret = -EPERM;
		goto exit;
	}

	if (padapter->hw_init_completed == _FALSE) {
		ret = -EPERM;
		goto exit;
	}

	if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING) == _TRUE) {
		ret = -EPERM;
		goto exit;
	}

	if ((check_fwstate(pmlmepriv, _FW_UNDER_SURVEY) == _TRUE) ||
	    (pmlmepriv->sitesurveyctrl.traffic_busy == _TRUE))
	{
		goto exit;
	} 

#if WIRELESS_EXT >= 17
	if (wrqu->data.length == sizeof(struct iw_scan_req))
	{
		struct iw_scan_req *req = (struct iw_scan_req *)extra;

		if (wrqu->data.flags & IW_SCAN_THIS_ESSID)
		{
			NDIS_802_11_SSID ssid;
			_irqL irqL;
			u32 len = (u32) min((u8)req->essid_len, (u8)IW_ESSID_MAX_SIZE);

			_memset((unsigned char*)&ssid, 0, sizeof(NDIS_802_11_SSID));

			_memcpy(ssid.Ssid, req->essid, len);
			ssid.SsidLength = len;

			printk(KERN_INFO "r8711_wx_set_scan: IW_SCAN_THIS_ESSID, ssid=%s, len=%d\n", req->essid, req->essid_len);

			_enter_critical(&pmlmepriv->lock, &irqL);

			if ((check_fwstate(pmlmepriv, _FW_UNDER_SURVEY|_FW_UNDER_LINKING) == _TRUE) ||
			    (pmlmepriv->sitesurveyctrl.traffic_busy == _TRUE)) {
				if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING) == _TRUE)
					status = _FALSE;
			} else
				status = sitesurvey_cmd(padapter, &ssid);

			_exit_critical(&pmlmepriv->lock, &irqL);
		}
		else if (req->scan_type == IW_SCAN_TYPE_PASSIVE)
		{
			printk(KERN_INFO "r8711_wx_set_scan: req->scan_type == IW_SCAN_TYPE_PASSIVE\n");
		}
	}
	else
#endif
	{
		status = set_802_11_bssid_list_scan(padapter);
	}

	if (status == _FALSE)
		ret = -1;

exit:

_func_exit_;

	return ret;
}

static int r8711_wx_get_scan(struct net_device *dev,
				struct iw_request_info *a,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	_queue *queue = &pmlmepriv->scanned_queue;
	struct wlan_network *pnetwork = NULL;

	_irqL irqL;
	_list *plist, *phead;

	char *ev = extra;
	char *stop = ev + wrqu->data.length;
	u32 ret = 0, cnt = 0;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r8711_wx_get_scan: Start of Query SIOCGIWSCAN\n"));

	if (padapter->bDriverStopped) {
		ret= -EINVAL;
		goto exit;
	}

	while (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY) == _TRUE)
	{
		msleep_os(30);
		cnt++;
		if (padapter->bSurpriseRemoved) {
				ret= -EINVAL;
				printk(KERN_NOTICE "\n  +r8711_wx_get_scan: padapter->bSurpriseRemoved  return\n");
				goto exit;
		}
		if (cnt > 1000)
			break;
	}

	_enter_critical(&queue->lock, &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1)
	{
		if (end_of_queue_search(phead, plist) == _TRUE)
			break;

		if ((stop - ev) < SCAN_ITEM_SIZE) {
			ret = -E2BIG;
			break;
		}

		pnetwork = LIST_CONTAINOR(plist, struct wlan_network, list);

		ev = translate_scan(padapter, a, pnetwork, ev, stop);

		plist = get_next(plist);
	}

	_exit_critical(&queue->lock, &irqL);

	wrqu->data.length = ev - extra;
	wrqu->data.flags = 0;

exit:

_func_exit_;

//	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_, ("-r8711_wx_get_scan\n"));

	return ret;
}

//set ssid flow
//s1. set_802_11_infrastructure_mode()
//s2. set_802_11_authenticaion_mode()
//s3. set_802_11_encryption_mode()
//s4. set_802_11_ssid()
static int r8711_wx_set_essid(struct net_device *dev,
				struct iw_request_info *a,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	_queue *queue = &pmlmepriv->scanned_queue;

	struct wlan_network *pnetwork = NULL;

	NDIS_802_11_AUTHENTICATION_MODE	authmode;
	NDIS_802_11_SSID ndis_ssid;

	u8 *dst_ssid, *src_ssid;
	_list *phead;
	u32 ret = 0, len;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("+r871x_wx_set_essid: fw_state=0x%08x\n", get_fwstate(pmlmepriv)));

	if (padapter->bup == _FALSE) {
		ret = -1;
		goto exit;
	}

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY) == _TRUE) {
		ret = -1;
		goto exit;
	}

	if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING) == _TRUE)
		goto exit;

	if (wrqu->essid.length > IW_ESSID_MAX_SIZE) {
		ret = -E2BIG;
		goto exit;
	}

	authmode = padapter->securitypriv.ndisauthtype;

	if (wrqu->essid.flags && wrqu->essid.length)
	{
		// Commented by Albert 20100519
		// We got the codes in "set_info" function of iwconfig source code.
		//	=========================================
		//	wrq.u.essid.length = strlen(essid) + 1;
	  	//	if(we_kernel_version > 20)
		//		wrq.u.essid.length--;
		//	=========================================
		//	That means, if the WIRELESS_EXT less than or equal to 20, the correct ssid len should subtract 1.
#if WIRELESS_EXT <= 20
		len = ((wrqu->essid.length-1) < IW_ESSID_MAX_SIZE) ? (wrqu->essid.length-1) : IW_ESSID_MAX_SIZE;
#else
		len = (wrqu->essid.length < IW_ESSID_MAX_SIZE) ? wrqu->essid.length : IW_ESSID_MAX_SIZE;
#endif

		_memset(&ndis_ssid, 0, sizeof(NDIS_802_11_SSID));
		ndis_ssid.SsidLength = len;
		_memcpy(ndis_ssid.Ssid, extra, len);
		src_ssid = ndis_ssid.Ssid;

		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_, ("r871x_wx_set_essid: ssid=[%s]\n", src_ssid));

		phead = get_list_head(queue);
		pmlmepriv->pscanned = get_next(phead);

		while (1)
		{
			if (end_of_queue_search(phead, pmlmepriv->pscanned) == _TRUE)
			{
#if 0
				if(check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE)
				{
					set_802_11_ssid(padapter, &ndis_ssid);

					goto exit;
				}
				else
				{
					RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("r871x_wx_set_ssid(): scanned_queue is empty\n"));
					ret = -EINVAL;
					goto exit;
				}
#endif
				RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_warning_,
					 ("r871x_wx_set_essid: scan_q is empty, set ssid to check if scanning again!\n"));

				break;
			}

			pnetwork = LIST_CONTAINOR(pmlmepriv->pscanned, struct wlan_network, list);

			pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

			dst_ssid = pnetwork->network.Ssid.Ssid;

			RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
				 ("r871x_wx_set_essid: dst_ssid=%s\n",
				  pnetwork->network.Ssid.Ssid));

			if ((_memcmp(dst_ssid, src_ssid, ndis_ssid.SsidLength) == _TRUE) &&
			    (pnetwork->network.Ssid.SsidLength == ndis_ssid.SsidLength))
			{
				RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_, ("r871x_wx_set_essid: find match, set infra mode\n"));

				if (set_802_11_infrastructure_mode(padapter, pnetwork->network.InfrastructureMode) == _FALSE)
				{
					ret = -1;
					goto exit;
				}

				break;
			}
		}

		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
			 ("set ssid: set_802_11_auth. mode=%d\n", authmode));
		set_802_11_authentication_mode(padapter, authmode);
//		set_802_11_encryption_mode(padapter, padapter->securitypriv.ndisencryptstatus);
		if (set_802_11_ssid(padapter, &ndis_ssid) == _FALSE)
			ret = -1;
	}

exit:

_func_exit_;

	return ret;
}

static int r8711_wx_get_essid(struct net_device *dev,
				struct iw_request_info *a,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	NDIS_WLAN_BSSID_EX *pcur_bss = &pmlmepriv->cur_network.network;

	u32 len, ret = 0;

_func_enter_;

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_, ("+r8711_wx_get_essid\n"));

	if (check_fwstate(pmlmepriv, _FW_LINKED|WIFI_ADHOC_MASTER_STATE) == _TRUE)
	{
		len = pcur_bss->Ssid.SsidLength;

		wrqu->essid.length = len;

		_memcpy(extra, pcur_bss->Ssid.Ssid, len);

		wrqu->essid.flags = 1;
	} else
		ret = -1;

_func_exit_;

	return ret;
}

static int r8711_wx_set_rate(struct net_device *dev,
				struct iw_request_info *a,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	u32 target_rate = wrqu->bitrate.value;
	u32 fixed = wrqu->bitrate.fixed;

	u32 ratevalue = 0;
	u8 datarates[NumRates];
	u8 mpdatarate[NumRates] = {11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0xff};

	int i, ret = 0;

_func_enter_;

	RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_notice_,("+r8711_wx_set_rate: target_rate=%d, fixed=%d\n", target_rate, fixed));

	if (target_rate == -1) {
		ratevalue = 11;
		goto set_rate;
	}

	target_rate = target_rate/100000;

	switch (target_rate)
	{
		case 10:
			ratevalue = 0;
			break;
		case 20:
			ratevalue = 1;
			break;
		case 55:
			ratevalue = 2;
			break;
		case 60:
			ratevalue = 3;
			break;
		case 90:
			ratevalue = 4;
			break;
		case 110:
			ratevalue = 5;
			break;
		case 120:
			ratevalue = 6;
			break;
		case 180:
			ratevalue = 7;
			break;
		case 240:
			ratevalue = 8;
			break;
		case 360:
			ratevalue = 9;
			break;
		case 480:
			ratevalue = 10;
			break;
		case 540:
			ratevalue = 11;
			break;
		default:
			ratevalue = 11;
			break;
	}

set_rate:

	for (i=0; i<NumRates; i++)
	{
		if (ratevalue == mpdatarate[i]) {
			datarates[i] = mpdatarate[i];
			if (fixed == 0) break;
		} else {
			datarates[i] = 0xff;
		}

		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("datarate_inx=%d\n",datarates[i]));
	}

	if (setdatarate_cmd(padapter, datarates) != _SUCCESS) {
		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("r8711_wx_set_rate Fail!!!\n"));
		ret = -1;
	}

_func_exit_;

	return ret;
}

static int r8711_wx_get_rate(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct registry_priv *pregpriv = &padapter->registrypriv;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	NDIS_WLAN_BSSID_EX *pcur_bss = &pmlmepriv->cur_network.network;

	struct ieee80211_ht_cap *pht_capie;

	int i;
	u8 *p;
	u16 rate, max_rate=0, ht_cap=_FALSE;
	u32 ht_ielen = 0;
	u8 bw_40MHz = 0, short_GI = 0;
	u16 mcs_rate = 0;


	RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_notice_,("+r8711_wx_get_rate\n"));

	i = 0;
	if (check_fwstate(pmlmepriv, _FW_LINKED|WIFI_ADHOC_MASTER_STATE) == _TRUE)
	{
		p = get_ie(&pcur_bss->IEs[12], _HT_CAPABILITY_IE_, &ht_ielen, pcur_bss->IELength-12);
		if (p && ht_ielen>0)
		{
			ht_cap = _TRUE;

			pht_capie = (struct ieee80211_ht_cap *)(p+2);

			_memcpy(&mcs_rate , pht_capie->supp_mcs_set, 2);

			bw_40MHz = (pht_capie->cap_info&IEEE80211_HT_CAP_SUP_WIDTH) ? 1:0;

			short_GI = (pht_capie->cap_info&(IEEE80211_HT_CAP_SGI_20|IEEE80211_HT_CAP_SGI_40)) ? 1:0;
		}

		while ((pcur_bss->SupportedRates[i]!=0) && (pcur_bss->SupportedRates[i]!=0xFF))
		{
			rate = pcur_bss->SupportedRates[i] & 0x7F;
			if (rate > max_rate)
				max_rate = rate;

			wrqu->bitrate.fixed = 0;	/* no auto select */
			//wrqu->bitrate.disabled = 1/;
			wrqu->bitrate.value = rate*500000;

			i++;
		}

		if (ht_cap == _TRUE)
		{
			if (mcs_rate & 0x8000)//MCS15
			{
				max_rate = (bw_40MHz) ? ((short_GI)?300:270):((short_GI)?144:130);

			}
			else if (mcs_rate & 0x0080)//MCS7
			{
				max_rate = (bw_40MHz) ? ((short_GI)?150:135):((short_GI)?72:65);
			}
			else//default MCS7
			{
				//printk("wx_get_rate, mcs_rate_bitmap=0x%x\n", mcs_rate);
				max_rate = (bw_40MHz) ? ((short_GI)?150:135):((short_GI)?72:65);
			}
#if 0
#ifdef CONFIG_80211N_HT
			bw_40MHz = pregpriv->cbw40_enable;
#else
			bw_40MHz = 0;
#endif

			switch (pregpriv->rf_config)
			{
				case RTL8712_RF_1T1R:
					max_rate = (bw_40MHz) ? 150:65;
					break;
				case RTL8712_RF_2T2R:
					max_rate = (bw_40MHz) ? 300:130;
					break;
				case RTL8712_RF_1T2R:
				default:
					max_rate = (bw_40MHz) ? 270:130;
					break;
			}
#endif

			max_rate *= 2; // Mbps/2
			wrqu->bitrate.value = max_rate * 500000;
		}
	}
	else {
		return -1;
	}

	return 0;
}

static int r8711_wx_get_rts(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);

_func_enter_;

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_, ("+r8711_wx_get_rts\n"));

	wrqu->rts.value = padapter->registrypriv.rts_thresh;
	wrqu->rts.fixed = 0;	/* no auto select */
	//wrqu->rts.disabled = (wrqu->rts.value == DEFAULT_RTS_THRESHOLD);

_func_exit_;

	return 0;
}

static int r8711_wx_set_frag(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);

_func_enter_;

	if (wrqu->frag.disabled)
		padapter->xmitpriv.frag_len = MAX_FRAG_THRESHOLD;
	else {
		if (wrqu->frag.value < MIN_FRAG_THRESHOLD ||
		    wrqu->frag.value > MAX_FRAG_THRESHOLD)
			return -EINVAL;

		padapter->xmitpriv.frag_len = wrqu->frag.value & ~0x1;
	}

_func_exit_;

	return 0;
}

static int r8711_wx_get_frag(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);

_func_enter_;

	wrqu->frag.value = padapter->xmitpriv.frag_len;
	wrqu->frag.fixed = 0;	/* no auto select */
//	wrqu->frag.disabled = (wrqu->frag.value == DEFAULT_FRAG_THRESHOLD);

_func_exit_;

	return 0;
}

static int r8711_wx_get_retry(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
//	_adapter *padapter = netdev_priv(dev);

	wrqu->retry.value = 7;
	wrqu->retry.fixed = 0;	/* no auto select */
	wrqu->retry.disabled = 1;

	return 0;
}

#if 0
#define IW_ENCODE_INDEX		0x00FF	/* Token index (if needed) */
#define IW_ENCODE_FLAGS		0xFF00	/* Flags defined below */
#define IW_ENCODE_MODE		0xF000	/* Modes defined below */
#define IW_ENCODE_DISABLED	0x8000	/* Encoding disabled */
#define IW_ENCODE_ENABLED	0x0000	/* Encoding enabled */
#define IW_ENCODE_RESTRICTED	0x4000	/* Refuse non-encoded packets */
#define IW_ENCODE_OPEN		0x2000	/* Accept non-encoded packets */
#define IW_ENCODE_NOKEY		0x0800  /* Key is write only, so not present */
#define IW_ENCODE_TEMP		0x0400  /* Temporary key */
/*
iwconfig wlan0 key on -> flags = 0x6001 -> maybe it means auto
iwconfig wlan0 key off -> flags = 0x8800
iwconfig wlan0 key open -> flags = 0x2800
iwconfig wlan0 key open 1234567890 -> flags = 0x2000
iwconfig wlan0 key restricted -> flags = 0x4800
iwconfig wlan0 key open [3] 1234567890 -> flags = 0x2003
iwconfig wlan0 key restricted [2] 1234567890 -> flags = 0x4002
iwconfig wlan0 key open [3] -> flags = 0x2803
iwconfig wlan0 key restricted [2] -> flags = 0x4802
*/
#endif

static int r8711_wx_set_enc(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *keybuf)
{
	u32 key, ret = 0;
	u32 keyindex_provided;
	NDIS_802_11_WEP	 wep;	
	NDIS_802_11_AUTHENTICATION_MODE authmode;

	struct iw_point *erq = &(wrqu->encoding);
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	key = erq->flags & IW_ENCODE_INDEX;

_func_enter_;

	printk(KERN_NOTICE "+r8711_wx_set_enc: flags=0x%x\n", erq->flags);

	_memset(&wep, 0, sizeof(NDIS_802_11_WEP));

	if (erq->flags & IW_ENCODE_DISABLED)
	{
		printk(KERN_INFO "r8711_wx_set_enc: EncryptionDisabled\n");
		padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
		padapter->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
		padapter->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
		padapter->securitypriv.dot11AuthAlgrthm= 0; //open system
  		authmode = Ndis802_11AuthModeOpen;
		padapter->securitypriv.ndisauthtype=authmode;

		goto exit;
	}

	if (key) {
		if (key > WEP_KEYS)
			return -EINVAL;
		key--;
		keyindex_provided = 1;
	} 
	else
	{
		keyindex_provided = 0;
		key = padapter->securitypriv.dot11PrivacyKeyIndex;
		printk(KERN_INFO "r871x_wx_set_enc, key=%d\n", key);
	}
	
	//set authentication mode
	if (erq->flags & IW_ENCODE_OPEN)
	{
		printk(KERN_INFO "r8711_wx_set_enc: IW_ENCODE_OPEN\n");
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;//Ndis802_11EncryptionDisabled;
		padapter->securitypriv.dot11AuthAlgrthm = 0; //open system
		padapter->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
		padapter->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
  		authmode = Ndis802_11AuthModeOpen;
		padapter->securitypriv.ndisauthtype=authmode;
	}
	else if (erq->flags & IW_ENCODE_RESTRICTED)
	{
		printk(KERN_INFO "r8711_wx_set_enc: IW_ENCODE_RESTRICTED\n");
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
		padapter->securitypriv.dot11AuthAlgrthm = 1; //shared system
		padapter->securitypriv.dot11PrivacyAlgrthm = _WEP40_;
		padapter->securitypriv.dot118021XGrpPrivacy = _WEP40_;
		authmode = Ndis802_11AuthModeShared;
		padapter->securitypriv.ndisauthtype=authmode;
	}
	else
	{
//		printk("r8711_wx_set_enc(): erq->flags=0x%x\n", erq->flags);

		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;//Ndis802_11EncryptionDisabled;
		padapter->securitypriv.dot11AuthAlgrthm = 0; //open system
		padapter->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
		padapter->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
  		authmode = Ndis802_11AuthModeOpen;
		padapter->securitypriv.ndisauthtype=authmode;
	}

	wep.KeyIndex = key;

	if (erq->length > 0) {
		wep.KeyLength = erq->length <= 5 ? 5 : 13;
		wep.Length = wep.KeyLength + FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial);
	} else {
		wep.KeyLength = 0 ;

		if (keyindex_provided == 1)// set key_id only, no given KeyMaterial(erq->length==0).
		{
			padapter->securitypriv.dot11PrivacyKeyIndex = key;

			printk(KERN_INFO "r871x_wx_set_enc: keyindex provided, keyid=%d, key_len=%d\n", key, padapter->securitypriv.dot11DefKeylen[key]);

			switch (padapter->securitypriv.dot11DefKeylen[key])
			{
				case 5:
					padapter->securitypriv.dot11PrivacyAlgrthm = _WEP40_;
					break;
				case 13:
					padapter->securitypriv.dot11PrivacyAlgrthm = _WEP104_;
					break;
				default:
					padapter->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
					break;
			}

			goto exit;
		}
	}

	wep.KeyIndex |= 0x80000000;	// transmit key

	_memcpy(wep.KeyMaterial, keybuf, wep.KeyLength);

	if(set_802_11_add_wep(padapter, &wep) == _FAIL)
	{
		ret = -EOPNOTSUPP;
		goto exit;
	}

exit:

_func_exit_;

	return ret;
}

static int r8711_wx_get_enc(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *keybuf)
{
	uint key, ret =0;
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct iw_point *erq = &(wrqu->encoding);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);

_func_enter_;

	if (check_fwstate(pmlmepriv, _FW_LINKED) == _FALSE)
	{
		if (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) == _FALSE)
		{
			erq->length = 0;
			erq->flags |= IW_ENCODE_DISABLED;
			return 0;
		}
	}

	key = erq->flags & IW_ENCODE_INDEX;

	if (key) {
		if (key > WEP_KEYS)
			return -EINVAL;
		key--;
	} else {
		key = padapter->securitypriv.dot11PrivacyKeyIndex;
	}

	erq->flags = key + 1;

	//if(padapter->securitypriv.ndisauthtype == Ndis802_11AuthModeOpen)
	//{
	//	erq->flags |= IW_ENCODE_OPEN;
	//}

	switch (padapter->securitypriv.ndisencryptstatus)
	{
		case Ndis802_11EncryptionNotSupported:
		case Ndis802_11EncryptionDisabled:
			erq->length = 0;
			erq->flags |= IW_ENCODE_DISABLED;
			break;

		case Ndis802_11Encryption1Enabled:
			erq->length = padapter->securitypriv.dot11DefKeylen[key];
			if (erq->length) {
				_memcpy(keybuf, padapter->securitypriv.dot11DefKey[key].skey, padapter->securitypriv.dot11DefKeylen[key]);

				erq->flags |= IW_ENCODE_ENABLED;

				if (padapter->securitypriv.ndisauthtype == Ndis802_11AuthModeOpen)
				{
					erq->flags |= IW_ENCODE_OPEN;
				}
				else if (padapter->securitypriv.ndisauthtype == Ndis802_11AuthModeShared)
				{
					erq->flags |= IW_ENCODE_RESTRICTED;
				}
			} else {
				erq->length = 0;
				erq->flags |= IW_ENCODE_DISABLED;
			}
			break;

		case Ndis802_11Encryption2Enabled:
		case Ndis802_11Encryption3Enabled:
			erq->length = 16;
			erq->flags |= (IW_ENCODE_ENABLED | IW_ENCODE_OPEN | IW_ENCODE_NOKEY);
			break;

		default:
			erq->length = 0;
			erq->flags |= IW_ENCODE_DISABLED;
			break;
	}

_func_exit_;

	return ret;
}

static int r8711_wx_get_power(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	//_adapter *padapter = netdev_priv(dev);

	wrqu->power.value = 0;
	wrqu->power.fixed = 0;	/* no auto select */
	wrqu->power.disabled = 1;

	return 0;
}

static int r871x_wx_set_gen_ie(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);

	return r871x_set_wpa_ie(padapter, extra, wrqu->data.length);
}

#if (WIRELESS_EXT >= 18)
static int r871x_wx_set_auth(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct iw_param *param = (struct iw_param*)&(wrqu->param);
	//Added by Jordan
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	int paramid;
	int paramval;
	int ret = 0;


	paramid = param->flags & IW_AUTH_INDEX;
	paramval = param->value;
	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("+r871x_wx_set_auth: id=0x%x, val=%d\n",
		  paramid, paramval));

	switch (paramid)
	{
		case IW_AUTH_WPA_VERSION: // 0
			break;
		case IW_AUTH_CIPHER_PAIRWISE: // 1
			break;
		case IW_AUTH_CIPHER_GROUP: // 2
			break;
		case IW_AUTH_KEY_MGMT: // 3
			/*
			 *  ??? does not use these parameters
			 */
			break;

		case IW_AUTH_TKIP_COUNTERMEASURES: // 4
		{
			if (paramval) {
				// wpa_supplicant is enabling the tkip countermeasure.
				padapter->securitypriv.btkip_countermeasure = _TRUE;
			} else {
				// wpa_supplicant is disabling the tkip countermeasure.
				padapter->securitypriv.btkip_countermeasure = _FALSE;
			}
			break;
		}

		case IW_AUTH_DROP_UNENCRYPTED: // 5
		{
			/* HACK:
			 *
			 * wpa_supplicant calls set_wpa_enabled when the driver
			 * is loaded and unloaded, regardless of if WPA is being
			 * used.  No other calls are made which can be used to
			 * determine if encryption will be used or not prior to
			 * association being expected.  If encryption is not being
			 * used, drop_unencrypted is set to false, else true -- we
			 * can use this to determine if the CAP_PRIVACY_ON bit should
			 * be set.
			 */

			if (padapter->securitypriv.ndisencryptstatus == Ndis802_11Encryption1Enabled)
			{
				break;//it means init value, or using wep, ndisencryptstatus = Ndis802_11Encryption1Enabled,
						// then it needn't reset it;
			}

			if (paramval) {
				padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
				padapter->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
				padapter->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
				padapter->securitypriv.dot11AuthAlgrthm= 0; //open system
				padapter->securitypriv.ndisauthtype=Ndis802_11AuthModeOpen;
			}

			break;
		}

		case IW_AUTH_80211_AUTH_ALG: // 6
			//Added by Jordan
			//When wpa_supplicant issued IW_AUTH_80211_AUTH_ALG, a new connection is going to kick off,
			//Thus the previous security settings should be cleared. 
			
			
			if (check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE)
			{
			    	printk("r871x_wx_set_auth: fwstate=FW_LINKED, disconnecting with AP...\n ");
				disassoc_cmd(padapter);

				indicate_disconnect(padapter);
			
				free_assoc_resources(padapter);
				clear_security_priv(padapter);
			}
			ret = wpa_set_auth_algs(dev, (u32)paramval);
			break;

		case IW_AUTH_WPA_ENABLED: // 7
#if 0
			if (paramval)
				padapter->securitypriv.dot11AuthAlgrthm = 2; //802.1x
			else
				padapter->securitypriv.dot11AuthAlgrthm = 0; //open system
#endif
			//_disassociate(priv);
			break;

		case IW_AUTH_RX_UNENCRYPTED_EAPOL: // 8
			//ieee->ieee802_1x = param->value;
			break;

		case IW_AUTH_PRIVACY_INVOKED: // 10
			//ieee->privacy_invoked = param->value;
			break;

		default:
			return -EOPNOTSUPP;
	}

	return ret;
}

static int r871x_wx_set_enc_ext(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	struct iw_point *pencoding = &wrqu->encoding;
 	struct iw_encode_ext *pext = (struct iw_encode_ext *)extra;

	struct ieee_param *param = NULL;
	char *alg_name;
	u32 param_len;

	int ret = 0;

	param_len = sizeof(struct ieee_param) + pext->key_len;
	param = (struct ieee_param *)_malloc(param_len);
	if (param == NULL)
		return -1;
	_memset(param, 0, param_len);

	param->cmd = IEEE_CMD_SET_ENCRYPTION;

	_memset(param->sta_addr, 0xff, ETH_ALEN);

	switch (pext->alg) {
		case IW_ENCODE_ALG_NONE:
		//todo: remove key 
		//remove = 1;	
			alg_name = "none";
			break;
		case IW_ENCODE_ALG_WEP:
			alg_name = "WEP";
			break;
		case IW_ENCODE_ALG_TKIP:
			alg_name = "TKIP";
			break;
		case IW_ENCODE_ALG_CCMP:
			alg_name = "CCMP";
			break;
		default:
			return -1;
	}

	strncpy((char *)param->u.crypt.alg, alg_name, IEEE_CRYPT_ALG_NAME_LEN);

	if (pext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)//?
	{
		param->u.crypt.set_tx = 0;
	}

	if (pext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)//?
	{
		param->u.crypt.set_tx = 1;
	}

	param->u.crypt.idx = (pencoding->flags & 0x00FF) -1;

	if (pext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
		_memcpy(param->u.crypt.seq, pext->rx_seq, 8);
	}

	if (pext->key_len) {
		param->u.crypt.key_len = pext->key_len;
		_memcpy(param + 1, pext + 1, pext->key_len);
	}


	if (pencoding->flags & IW_ENCODE_DISABLED)
	{		
		//todo: remove key 
		//remove = 1;		
	}	
	
	ret = wpa_set_encryption(dev, param, param_len);

	if (param) {
		_mfree((u8*)param, param_len);
	}

	return ret;
}
#endif
static int r871x_wx_get_nick(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	if (extra) {
		wrqu->data.length = 8;
		wrqu->data.flags = 1;
		_memcpy(extra, "rtl_wifi", 8);
	}

	return 0;
}

/*
 * return:
 *	0	MAC address
 *	1	Base Band address
 *	2	RF address
 */
static u8 addr_type(u32 addr)
{
	u8 type = 0;	// 0: mac, 1:bb, 2:rf

	if (addr <= 0xFF)
		type = 2; // rf
	else if (addr <= 0xFFF)
		type = 1;

	return type;
}

static int r8711_wx_read32(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *keybuf)
{
	PADAPTER padapter = (PADAPTER) _netdev_priv(dev);

	u32 addr;
	u32 data32;
	u32 bytes;
	u8 type;	// 0: mac, 1:bb, 2:rf


	bytes = *(u32*)keybuf;
	addr = *((u32*)keybuf + 1);
	type = addr_type(addr);

	switch (type)
	{
		case 1:
{
			u8 shift = 0;
			shift = addr & 0x3;
			if (shift == 0)
				data32 = get_bbreg(padapter, addr, bMaskDWord);
			else {
				u32 tmp;
				addr &= ~0x3;
				tmp = get_bbreg(padapter, addr, bMaskDWord);
				tmp >>= shift * 8;
				data32 |= tmp;
				addr += 0x4;
				tmp = get_bbreg(padapter, addr, bMaskDWord);
				tmp <<= (0x4 - shift) * 8;
				data32 |= tmp;
			}
}
			sprintf(keybuf, "0x%08X", data32);
			printk(KERN_INFO "%s: addr=0x%03X data=0x%08X\n", __FUNCTION__, addr, data32);
			break;

		case 2:
			data32 = get_rfreg(padapter, bytes, addr, bMaskDWord);
			sprintf(keybuf, "0x%05X", data32);
			printk(KERN_INFO "%s: addr=0x%02X data=0x%05X\n", __FUNCTION__, addr, data32);
			break;

		case 0:
		default:
			switch (bytes) {
				case 1:
					data32 = read8(padapter, addr);
					sprintf(keybuf, "0x%02X", data32);
					printk(KERN_INFO "%s: addr=0x%08X data=0x%02X\n", __FUNCTION__, addr, data32);
					break;
				case 2:
					data32 = read16(padapter, addr);
					sprintf(keybuf, "0x%04X", data32);
					printk(KERN_INFO "%s: addr=0x%08X data=0x%04X\n", __FUNCTION__, addr, data32);
					break;
				case 4:
				default:
					data32 = read32(padapter, addr);
					sprintf(keybuf, "0x%08X", data32);
					printk(KERN_INFO "%s: addr=0x%08X data=0x%08X\n", __FUNCTION__, addr, data32);
					break;
			}
			break;
	}

	return 0;
}

static int r8711_wx_write32(struct net_device *dev,
				 struct iw_request_info *info,
				 union iwreq_data *wrqu, char *keybuf)
{
	PADAPTER padapter = (PADAPTER) _netdev_priv(dev);

	u32 addr;
	u32 data32;
	u32 bytes;
	u8 type;	// 0: mac, 1:bb, 2:rf
	u8 ret;


	bytes = *(u32*)keybuf;
	addr = *((u32*)keybuf + 1);
	data32 = *((u32*)keybuf + 2);
	type = addr_type(addr);

	switch (type)
	{
		case 1:
{
			u32 tmp;
			u32 mask, mask_w;
			u8 shift = 0;

			switch (bytes) {
				case 1:
					mask = 0xFF;
					break;
				case 2:
					mask = 0xFFFF;
					break;
				case 3:
					mask = 0xFFFFFF;
					break;
				default:
					bytes = 4;
					mask = bMaskDWord;
					break;
			}

			shift = addr & 0x3;
			mask_w =  mask << (shift * 8);
			addr &= ~0x3;

			ret = set_bbreg(padapter, addr, mask_w, data32);
			printk(KERN_INFO "%s: addr=0x%03X bytes=%d data=0x%08X maks=0x%08X (ret=%d)\n", __FUNCTION__, addr, bytes, data32, mask_w, ret);
			if (ret == _FALSE) return -EIO;

			shift = 0x4 - shift;
			if (bytes <= shift) break;
			bytes -= shift;
			if (bytes) {
				addr += 0x4;
				mask_w = mask >> (shift * 8);
				data32 >>= shift * 8;
				ret = set_bbreg(padapter, addr, mask_w, data32);
				printk(KERN_INFO "%s: addr=0x%03X bytes=%d data=0x%08X maks=0x%08X (ret=%d)\n", __FUNCTION__, addr, bytes, data32, mask_w, ret);
				if (ret == _FALSE) return -EIO;
			}
}
			break;

		case 2:
			ret = set_rfreg(padapter, bytes, addr, bMaskDWord, data32);
			printk(KERN_INFO "%s: path=%d addr=0x%02X data=0x%05X (ret=%d)\n", __FUNCTION__, bytes, addr, data32, ret);
			if (ret == _FALSE) return -EIO;
			break;

		case 0:
		default:
			switch (bytes) {
				case 1:
					write8(padapter, addr, (u8)data32);
					printk(KERN_INFO "%s: addr=0x%08X data=0x%02X\n", __FUNCTION__, addr, (u8)data32);
					break;
				case 2:
					write16(padapter, addr, (u16)data32);
					printk(KERN_INFO "%s: addr=0x%08X data=0x%04X\n", __FUNCTION__, addr, (u16)data32);
					break;
				case 4:
					write32(padapter, addr, data32);
					printk(KERN_INFO "%s: addr=0x%08X data=0x%08X\n", __FUNCTION__, addr, data32);
					break;
				default:
					printk(KERN_INFO "%s: usage> write [bytes/path] [address] [data]\n", __FUNCTION__);
					return -EINVAL;
			}
			break;
	}
	return 0;
}

static int dummy(struct net_device *dev,
		struct iw_request_info *a,
		union iwreq_data *wrqu, char *b)
{
	_adapter *padapter = (_adapter *)_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("WLAN IOCTL: cmd_code=%x, fwstate=0x%x\n",
		  a->cmd, pmlmepriv->fw_state));

	return -1;
}

/*
typedef int (*iw_handler)(struct net_device *dev, struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra);
*/
/*
 *	For all data larger than 16 octets, we need to use a
 *	pointer to memory allocated in user space.
 */
static int r8711_drvext_hdl(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{

#if 0
struct	iw_point
{
	void __user *pointer;	/* Pointer to the data  (in user space) */
	__u16 length;		/* number of fields or size in bytes */
	__u16 flags;		/* Optional params */
};
#endif

#if 0
	u8 res;
	struct drvext_handler *phandler;
	struct drvext_oidparam *poidparam;
	int ret;
	u16 len;
	u8 *pparmbuf, bset;
	_adapter *padapter = netdev_priv(dev);
	struct iw_point *p = &wrqu->data;

	if ((!p->length) || (!p->pointer)) {
		ret = -EINVAL;
		goto _r8711_drvext_hdl_exit;
	}

	bset = (u8)(p->flags&0xFFFF);
	len = p->length;
	pparmbuf = (u8*)_malloc(len);
	if (pparmbuf == NULL) {
		ret = -ENOMEM;
		goto _r8711_drvext_hdl_exit;
	}

	if(bset)//set info
	{
		if (copy_from_user(pparmbuf, p->pointer,len)) {
			_mfree(pparmbuf, len);
			ret = -EFAULT;
			goto _r8711_drvext_hdl_exit;
		}
	}
	else//query info
	{

	}

	poidparam = (struct drvext_oidparam *)pparmbuf;

	RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("drvext set oid subcode [%d], len[%d], InformationBufferLength[%d]\r\n",
						 poidparam->subcode, poidparam->len, len));


	//check subcode
	if ( poidparam->subcode >= MAX_DRVEXT_HANDLERS)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("no matching drvext handlers\r\n"));
		ret = -EINVAL;
		goto exit_drvext_set_info;
	}


	if ( poidparam->subcode >= MAX_DRVEXT_OID_SUBCODES)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("no matching drvext subcodes\r\n"));
		ret = -EINVAL;
		goto _r8711_drvext_hdl_exit;
	}

	phandler = drvextoidhandlers + poidparam->subcode;

	if (poidparam->len != phandler->parmsize)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("no matching drvext param size %d vs %d\r\n",
						poidparam->len , phandler->parmsize));
		ret = -EINVAL;
		goto _r8711_drvext_hdl_exit;
	}

	res = phandler->handler(padapter, bset, poidparam->data);

	if (res == 0)
		ret = 0
	else
		ret = -EFAULT;

_r8711_drvext_hdl_exit:

	return ret;

#endif

	return 0;
}

static int r871x_mp_ioctl_hdl(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);

	struct iw_point *p = &wrqu->data;

	struct oid_par_priv oid_par;
	struct mp_ioctl_handler *phandler;
	struct mp_ioctl_param *poidparam;
	unsigned long BytesRead, BytesWritten, BytesNeeded;
	u8 *pparmbuf = NULL, bset;

	u16 len;
	uint status;
	int ret = 0;

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+r871x_mp_ioctl_hdl\n"));

#ifdef CONFIG_MP_INCLUDED
	//mutex_lock(&ioctl_mutex);

	if ((!p->length) || (!p->pointer)) {
		ret = -EINVAL;
		goto _r871x_mp_ioctl_hdl_exit;
	}

	bset = (u8)(p->flags & 0xFFFF);
	len = p->length;

	pparmbuf = NULL;
	pparmbuf = (u8*)_malloc(len);
	if (pparmbuf == NULL) {
		ret = -ENOMEM;
		goto _r871x_mp_ioctl_hdl_exit;
	}

	if (copy_from_user(pparmbuf, p->pointer, len)) {
		ret = -EFAULT;
		goto _r871x_mp_ioctl_hdl_exit;
	}

	poidparam = (struct mp_ioctl_param *)pparmbuf;
	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("r871x_mp_ioctl_hdl: buffer_len[%d] subcode[%d] len[%d]\n",
		  len, poidparam->subcode, poidparam->len));

	if (poidparam->subcode >= MAX_MP_IOCTL_SUBCODE) {
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_err_,
			 ("!r871x_mp_ioctl_hdl: no matching drvext subcodes\n"));
		ret = -EINVAL;
		goto _r871x_mp_ioctl_hdl_exit;
	}

	phandler = mp_ioctl_hdl + poidparam->subcode;

	if ((phandler->paramsize != 0) && (poidparam->len < phandler->paramsize))
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_err_,
			 ("!r871x_mp_ioctl_hdl: param size not match %d\n",
			  phandler->paramsize));
		ret = -EINVAL;
		goto _r871x_mp_ioctl_hdl_exit;
	}

	if (phandler->oid == 0 && phandler->handler) {
	        status = phandler->handler(&oid_par);
	}
	else if (phandler->handler)
	{
		oid_par.adapter_context = padapter;
		oid_par.oid = phandler->oid;
		oid_par.information_buf = poidparam->data;
		oid_par.information_buf_len = poidparam->len;
		oid_par.dbg = 0;

		BytesWritten = 0;
		BytesNeeded = 0;

		if (bset) {
			oid_par.bytes_rw = &BytesRead;
			oid_par.bytes_needed = &BytesNeeded;
			oid_par.type_of_oid = SET_OID;
		} else {
			oid_par.bytes_rw = &BytesWritten;
			oid_par.bytes_needed = &BytesNeeded;
			oid_par.type_of_oid = QUERY_OID;
		}

		status = phandler->handler(&oid_par);

	//todo:check status, BytesNeeded, etc.
	}
	else {
		printk(KERN_ERR "r871x_mp_ioctl_hdl(): err!, subcode=%d, oid=%d, handler=%p\n", 
			poidparam->subcode, phandler->oid, phandler->handler);
		ret = -EFAULT;
		goto _r871x_mp_ioctl_hdl_exit;
	}

	if (bset == 0x00) {//query info
		//_memcpy(p->pointer, pparmbuf, len);
		if (copy_to_user(p->pointer, pparmbuf, len))
			ret = -EFAULT;
	}

	if (status) {
		ret = -EFAULT;
		goto _r871x_mp_ioctl_hdl_exit;
	}

_r871x_mp_ioctl_hdl_exit:

	if (pparmbuf != NULL)
		_mfree(pparmbuf, 0);

	//mutex_unlock(&ioctl_mutex);
#else
	ret = -EINVAL;
#endif

	return ret;
}

static int r871x_get_ap_info(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	_queue *queue = &pmlmepriv->scanned_queue;
	struct iw_point *pdata = &wrqu->data;

	struct wlan_network *pnetwork = NULL;

	int bssid_match, ret = 0;
	u32 cnt = 0, wpa_ielen;
	_irqL irqL;
	_list *plist, *phead;
	unsigned char *pbuf;
	u8 bssid[ETH_ALEN];
	char data[32];


	printk(KERN_NOTICE "+r871x_get_aplist_info\n");

	if (padapter->bDriverStopped || (pdata==NULL)) {
		ret= -EINVAL;
		goto exit;
	}

	while (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY|_FW_UNDER_LINKING) == _TRUE) {
		msleep_os(30);
		cnt++;
		if(cnt > 100)
			break;
	}


	//pdata->length = 0;//?	
	pdata->flags = 0;
	if(pdata->length>=32)
	{
		if(copy_from_user(data, pdata->pointer, 32))
		{
			ret= -EINVAL;
			goto exit;
		}
	}	
	else
	{
		ret= -EINVAL;
		goto exit;
	}	

	_enter_critical(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1)
	{
		if (end_of_queue_search(phead,plist)== _TRUE)
			break;

		pnetwork = LIST_CONTAINOR(plist, struct wlan_network, list);

		//if(hwaddr_aton_i(pdata->pointer, bssid)) 
		if(hwaddr_aton_i(data, bssid)) 
		{			
			printk(KERN_NOTICE "Invalid BSSID '%s'.\n", (u8*)data);
			return -EINVAL;
		}

		printk(KERN_INFO "BSSID:" MACSTR "\n", MAC2STR(bssid));

		if(_memcmp(bssid, pnetwork->network.MacAddress, ETH_ALEN) == _TRUE)//BSSID match, then check if supporting wpa/wpa2
		{
			pbuf = get_wpa_ie(&pnetwork->network.IEs[12], &wpa_ielen, pnetwork->network.IELength-12);
			if (pbuf && (wpa_ielen>0)) {
				pdata->flags = 1;
				break;
			}

			pbuf = get_wpa2_ie(&pnetwork->network.IEs[12], &wpa_ielen, pnetwork->network.IELength-12);
			if(pbuf && (wpa_ielen>0)) {
				pdata->flags = 2;
				break;
			}
		}
		plist = get_next(plist);
	}

	_exit_critical(&(pmlmepriv->scanned_queue.lock), &irqL);

	if(pdata->length>=34)
	{
		if(copy_to_user((u8*)pdata->pointer+32, (u8*)&pdata->flags, 1))
		{
			ret= -EINVAL;
			goto exit;
		}
	}	
	
exit:

	return ret;
}

static int r871x_set_pid(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct iw_point *pdata = &wrqu->data;

	printk(KERN_NOTICE "+r871x_set_pid\n");

	if((padapter->bDriverStopped) || (pdata==NULL))
	{
		ret= -EINVAL;
		goto exit;
	}

	//pdata->length = 0;
	//pdata->flags = 0;

	//_memcpy(&padapter->pid, pdata->pointer, sizeof(int));
	if(copy_from_user(&padapter->pid, pdata->pointer, sizeof(int)))
	{
		ret= -EINVAL;
		goto exit;
	}

	printk(KERN_INFO "got pid=%d\n", padapter->pid);

exit:

	return ret;
}

static int r871x_wps_start(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	
	int ret = 0;	
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct iw_point *pdata = &wrqu->data;
	u32   u32wps_start = 0;
        unsigned int uintRet = 0;

        uintRet = copy_from_user( ( void* ) &u32wps_start, pdata->pointer, 4 );

	if((padapter->bDriverStopped) || (pdata==NULL))
	{                
		ret= -EINVAL;
		goto exit;
	}		

       if ( u32wps_start == 0 )
       {
           u32wps_start = *extra;
       }

	printk(KERN_INFO "[%s] wps_start = %d\n", __FUNCTION__, u32wps_start );

       if ( u32wps_start == 1 ) // WPS Start
       {
           padapter->ledpriv.LedControlHandler(padapter, LED_CTL_START_WPS);
       }
	else if ( u32wps_start == 2 ) // WPS Stop because of wps success
	{
           padapter->ledpriv.LedControlHandler(padapter, LED_CTL_STOP_WPS);
	}
	else if ( u32wps_start == 3 ) // WPS Stop because of wps fail
	{
           padapter->ledpriv.LedControlHandler(padapter, LED_CTL_STOP_WPS_FAIL);
	}
exit:
	
	return ret;
		
}

#ifdef POWER_DOWN_SUPPORT

extern s32 dev_power_on(_adapter *padapter);
extern void dev_power_off(_adapter *padapter);
extern s32 start_drv_threads(_adapter *padapter);
extern void stop_drv_threads(_adapter *padapter);
extern void start_drv_timers(_adapter *padapter);
extern void stop_drv_timers(_adapter *padapter);
extern void enable_video_mode(_adapter *padapter, int cbw40_value);
extern void disable_interrupt(_adapter *padapter);

#ifdef CONFIG_80211N_HT
extern int video_mode;
#endif

#define POWER_OFF	0
#define POWER_ON	1

/*
 * Control GPIO to pull or release power down pin (PWN) 
 * to power on/off WiFi.
 *
 * Parameters
 *	power_on
 *		1: power on
 *		0: power off
 *
 * Return
 *	_SUCCESS	power on/off successfully
 *	_FAIL		power on/off fail
 */
static s32 WiFi_power_switch(u8 power_on)
{
	return _SUCCESS;
}

static s32 WiFi_power_on(PADAPTER padapter)
{
	u32 v32;
	s32 err;
	struct net_device *dev = padapter->pnetdev;

	// power on
//	printk("+%s: start to power on\n", __func__);

	//3 1. power on
	err = WiFi_power_switch(POWER_ON);
	if (err != _SUCCESS) {
		printk(KERN_ERR "%s: POWER ON RTL8712 WiFi FAIL!\n", __func__);
		return -EAGAIN;
	}

	//3 2. recover hardware, interface host/bus
	// recover interface host/bus
	err = dev_power_on(padapter);
	if (err != _SUCCESS) {
		printk(KERN_ERR "%s: dev_power_on FAIL(%d)!\n", __func__, err);
		return -EAGAIN;
	}
	padapter->bpoweroff = _FALSE;

	//3 3. initialize driver
	padapter->mlmepriv.fw_state = 0;
	padapter->mlmepriv.sitesurveyctrl.traffic_busy = _FALSE;
	padapter->mlmepriv.to_join = _FALSE;
	padapter->evtpriv.event_seq = 0;
#ifdef CONFIG_SDIO_HCI
	_memset(padapter->xmitpriv.free_pg, 0, 8);
	padapter->xmitpriv.public_pgsz = 0;
	padapter->xmitpriv.required_pgsz = 0;
	padapter->xmitpriv.used_pgsz = 0;
	padapter->xmitpriv.init_pgsz = 0;
#endif

	//3 4. power on sequence, download firmware, enable interrupt
	// reference from netdev_open(), ifconfig up
	padapter->bDriverStopped = _FALSE;
	padapter->bSurpriseRemoved = _FALSE;
	padapter->bup = _TRUE;

	err = rtl871x_hal_init(padapter);
	if (err != _SUCCESS) {
		printk(KERN_ERR "%s: rtl871x_hal_init FAIL!\n", __func__);
		padapter->bDriverStopped = _TRUE;
		padapter->bup = _FALSE;
		return -EAGAIN;
	}

	v32 = read32(padapter, TXFF_EMPTY_TH);
	v32 |= 0xfffff;
	write32(padapter, TXFF_EMPTY_TH, v32);

	//3 5. recover software
#ifdef CONFIG_SDIO_HCI
	_init_sema(&padapter->xmitpriv.xmit_sema, 0);
	_init_sema(&padapter->xmitpriv.terminate_xmitthread_sema, 0);
#endif
#ifdef CONFIG_RECV_THREAD_MODE
	_init_sema(&padapter->recvpriv.recv_sema, 0);
	_init_sema(&padapter->recvpriv.terminate_recvthread_sema, 0);
#endif
	_init_sema(&padapter->cmdpriv.cmd_queue_sema, 0);
//	_init_sema(&padapter->cmdpriv.cmd_done_sema, 0);
	_init_sema(&padapter->cmdpriv.terminate_cmdthread_sema, 0);
#ifdef CONFIG_EVENT_THREAD_MODE
	_init_sema(&padapter->evtpriv.evt_notify, 0);
	_init_sema(&padapter->evtpriv.terminate_evtthread_sema, 0);
#endif
#ifdef CONFIG_H2CLBK
	_init_sema(&padapter->evtpriv.lbkevt_done, 0);
#endif
	err = start_drv_threads(padapter);
	if (err != _SUCCESS) {
		printk(KERN_ERR "%s: start_drv_threads FAIL!\n", __func__);
//		return -EPERM;
	}

	// setup default disconnect detection mechanism
	disconnectCtrlEx_cmd(padapter, 1, 5, 100, 2000);

#ifdef CONFIG_PWRCTRL
	free_pwrctrl_priv(padapter);
	init_pwrctrl_priv(padapter);
	set_ps_mode(padapter, padapter->registrypriv.power_mgnt, padapter->registrypriv.smart_ps);
#endif

	// notify OS to start traffic
//	netif_device_attach(dev);
	netif_wake_queue(dev);

#ifdef CONFIG_80211N_HT
	if (video_mode)
		enable_video_mode(padapter, padapter->registrypriv.cbw40_enable);
#endif

	// start driver mlme relation timer
	start_drv_timers(padapter);

	padapter->ledpriv.LedControlHandler(padapter, LED_CTL_NO_LINK);

//	printk("-%s: POWER ON OK, bup=%d\n", __func__, padapter->bup);

	return 0;
}

static void WiFi_power_off(PADAPTER padapter)
{
	_irqL irqL;
	struct net_device *dev = padapter->pnetdev;

#ifdef CONFIG_PWRCTRL
	//3 0. Leave Power Saving Mode
	leave_ps_mode(padapter);
#endif

	//3 1. Change status to prepare Power OFF, Stop accepting IOCTL
	padapter->bup = _FALSE;

	//3 2. Stop TX Traffic
	// don't use netif_stop_queue() outside the hard_xmit_xmit
	netif_tx_disable(dev);
//	netif_device_detach(dev);

	//3 3. Disable Interrupt, Stop RX Traffic
	disable_interrupt(padapter);

	//3 4. Inform OS disconnect
	_enter_critical(&padapter->mlmepriv.lock, &irqL);
	if (check_fwstate(&padapter->mlmepriv, _FW_LINKED) == _TRUE) {
		disassoc_cmd(padapter);
		indicate_disconnect(padapter);
		free_assoc_resources(padapter);
	}
	free_scanqueue(&padapter->mlmepriv);
//	free_network_queue(padapter);
	_exit_critical(&padapter->mlmepriv.lock, &irqL);

	//3 5. Stop Timer
	stop_drv_timers(padapter);

	//3 6. Stop Thread
	padapter->bDriverStopped = _TRUE;
	stop_drv_threads(padapter);

	//3 7 Close LED
	padapter->ledpriv.LedControlHandler(padapter, LED_CTL_POWER_OFF);

	//3 8. interface host/bus relative
	dev_power_off(padapter);

	//3 9. hardware power off
	WiFi_power_switch(POWER_OFF);

	padapter->bSurpriseRemoved = _TRUE;
	padapter->bpoweroff = _TRUE;
}

s32 _r8712_power_off(PADAPTER padapter, u8 poweroff)
{
	s32 error = 0;


	if (padapter->bpoweroff && poweroff)
		return 0/*-EPERM*/;

	if (!padapter->bpoweroff && !poweroff)
		return 0/*-EPERM*/;

	if (poweroff)
		WiFi_power_off(padapter);
	else
		error = WiFi_power_on(padapter);

	return error;
}
#endif /* #ifdef POWER_DOWN_SUPPORT */

static int r8712_power_off(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter*) _netdev_priv(dev);
	u8 power_off = 0;
	s32 error;


	if (extra == NULL) return -EINVAL;
	power_off = *(u8*)extra;
	printk(KERN_NOTICE "+%s: power_off=%d\n", __func__, power_off);

#ifdef POWER_DOWN_SUPPORT
	error = _r8712_power_off(padapter, !!(power_off));
#endif

#ifdef RFKILL_SUPPORT
	if (!error)
		rfkill_set_sw_state(padapter->prfkill, !!(power_off));
#endif

	printk(KERN_INFO "-%s: power_off=%d error=%d\n", __func__, power_off, error);
	return error;
}

static int r8712_sleep_wakeup(struct net_device *dev,				
	struct iw_request_info *info,				
	union iwreq_data *wrqu, char *extra)
{	
	_adapter	 *padapter = (_adapter*) _netdev_priv(dev);	
	u8 sleep = 0;	
	s32 err;	
	int ret = 0;	
	if (extra == NULL) 
		return -EINVAL;	
	sleep = *(u8*)extra;	
	printk("+%s: r8712_sleep_wakeup=%d\n", __func__, sleep);

	if ((padapter->hw_init_completed== _FALSE) )		
		return 0/*-EPERM*/;
	//	if ((padapter->bup == _FALSE) && power_off)
	//		return 0/*-EPERM*/;	
		if (sleep == 1)		
			ret = pnp_set_power_sleep(padapter);	
		else		
			pnp_set_power_wakeup(padapter);

	printk("-%s: r8712_sleep_wakeup=%d ret=%d\n", __func__, sleep, ret);	
	return ret;
}
#ifdef RTK_DMP_PLATFORM
static int r871x_wx_null(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra)
{
	return 0;
}


static int r871x_wx_get_ap_status(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = netdev_priv(dev);
	int name_len;

	//count the length of input ssid
	for(name_len=0 ; ((char*)wrqu->data.pointer)[name_len]!='\0' ; name_len++);

	wrqu->data.length = (padapter->recvpriv.rssi + 95)*2;

	return 0;
}

static int r871x_wx_set_countrycode(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	int	countrycode=0;

	countrycode = (int)wrqu->data.pointer;
	printk(KERN_NOTICE "\n======== Set Countrycode = %d !!!! ========\n",countrycode);

	return 0;
}
#else

static int r8712_disconnect_ctrl(struct net_device *dev,				
	struct iw_request_info *info,				
	union iwreq_data *wrqu, char *extra)
{	
	_adapter	 *padapter = (_adapter*) _netdev_priv(dev);	
	u8 disconnect_ctrl = 0;	
	s32 err;	
	int ret = 0;	
	if (extra == NULL) 
		return -EINVAL;	
	disconnect_ctrl = *(u8*)extra;	
	printk("+%s: r8712_disconnect_ctrl=%d\n", __func__, disconnect_ctrl);

	if ((padapter->hw_init_completed== _FALSE) )		
		return 0/*-EPERM*/;
	//	if ((padapter->bup == _FALSE) && power_off)
	//		return 0/*-EPERM*/;	
		if (disconnect_ctrl == 0)		
			ret = setconnectionctrl_cmd(padapter, 0, 0);	
		else{		
			setconnectionctrl_cmd(padapter, 1, (disconnect_ctrl>>1));
		}
	printk("-%s: r8712_sleep_wakeup=%d ret=%d\n", __func__, disconnect_ctrl, ret);	
	return ret;
}

#endif

static int wpa_set_param(struct net_device *dev, u8 name, u32 value)
{
	uint ret = 0;
	u32 flags;
	_adapter *padapter = (_adapter *) _netdev_priv(dev);

	switch (name)
	{
		case IEEE_PARAM_WPA_ENABLED:
			padapter->securitypriv.dot11AuthAlgrthm = 2; //802.1x
			//ret = ieee80211_wpa_enable(ieee, value);

			switch((value)&0xff)
			{
				case 1 : //WPA
					padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeWPAPSK; //WPA_PSK
					padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
					break;
				case 2: //WPA2
					padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeWPA2PSK; //WPA2_PSK
					padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
					break;
			}

			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("wpa_set_param:padapter->securitypriv.ndisauthtype=%d\n", padapter->securitypriv.ndisauthtype));

			break;

		case IEEE_PARAM_TKIP_COUNTERMEASURES:
			//ieee->tkip_countermeasures=value;
			break;

		case IEEE_PARAM_DROP_UNENCRYPTED:
		{
			/* HACK:
			 *
			 * wpa_supplicant calls set_wpa_enabled when the driver
			 * is loaded and unloaded, regardless of if WPA is being
			 * used.  No other calls are made which can be used to
			 * determine if encryption will be used or not prior to
			 * association being expected.  If encryption is not being
			 * used, drop_unencrypted is set to false, else true -- we
			 * can use this to determine if the CAP_PRIVACY_ON bit should
			 * be set.
			 */

#if 0
			struct ieee80211_security sec = {
				.flags = SEC_ENABLED,
				.enabled = value,
			};
 			ieee->drop_unencrypted = value;
			/* We only change SEC_LEVEL for open mode. Others
			 * are set by ipw_wpa_set_encryption.
			 */
			if (!value) {
				sec.flags |= SEC_LEVEL;
				sec.level = SEC_LEVEL_0;
			}
			else {
				sec.flags |= SEC_LEVEL;
				sec.level = SEC_LEVEL_1;
			}
			if (ieee->set_security)
				ieee->set_security(ieee->dev, &sec);
#endif
			break;
		}

		case IEEE_PARAM_PRIVACY_INVOKED:
			//ieee->privacy_invoked=value;
			break;

		case IEEE_PARAM_AUTH_ALGS:
			ret = wpa_set_auth_algs(dev, value);
			break;

		case IEEE_PARAM_IEEE_802_1X:
			//ieee->ieee802_1x=value;
			break;

		case IEEE_PARAM_WPAX_SELECT:
			// added for WPA2 mixed mode
			//printk(KERN_WARNING "------------------------>wpax value = %x\n", value);
			/*
			spin_lock_irqsave(&ieee->wpax_suitlist_lock,flags);
			ieee->wpax_type_set = 1;
			ieee->wpax_type_notify = value;
			spin_unlock_irqrestore(&ieee->wpax_suitlist_lock,flags);
			*/
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
	}

	return ret;
}

static int wpa_mlme(struct net_device *dev, u32 command, u32 reason)
{
	int ret = 0;
	_adapter *padapter = (_adapter *) _netdev_priv(dev);

	switch (command)
	{
		case IEEE_MLME_STA_DEAUTH:
			if (!set_802_11_disassociate(padapter))
				ret = -1;
			break;

		case IEEE_MLME_STA_DISASSOC:
			if (!set_802_11_disassociate(padapter))
				ret = -1;
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
	}

	return ret;
}

#if 0
static int wpa_set_wpa_ie(struct net_device *dev,
			      struct ieee_param *param, u32 plen)
{
	u8 *buf;
	uint ret = 0;
	u32 left;
 	u8 *pos;
	int group_cipher, pairwise_cipher;
	_adapter *padapter = netdev_priv(dev);


	if((param->u.wpa_ie.len > MAX_WPA_IE_LEN) || (param->u.wpa_ie.data == NULL))
		return -EINVAL;

	if (param->u.wpa_ie.len)
	{
		buf = _malloc(param->u.wpa_ie.len);
		if (buf == NULL){
			ret =  -ENOMEM;
			goto exit;
		}

		_memcpy(buf, param->u.wpa_ie.data, param->u.wpa_ie.len);

		//dump
		{
			int i;
			printk("\n wpa_ie(length:%d):\n",param->u.wpa_ie.len);
			for(i=0;i<param->u.wpa_ie.len;i=i+8)
				printk("0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x \n",buf[i],buf[i+1],buf[i+2],buf[i+3],buf[i+4],buf[i+5],buf[i+6],buf[i+7]);
		}

		pos = buf;
		if(param->u.wpa_ie.len < RSN_HEADER_LEN){
			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("Ie len too short %lu\n",(u32)param->u.wpa_ie.len ));
			ret  = -1;
			goto exit;
		}

		pos += RSN_HEADER_LEN;
		left = param->u.wpa_ie.len - RSN_HEADER_LEN;

		if (left >= RSN_SELECTOR_LEN){
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
		else if (left > 0) {
			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("Ie length mismatch, %u too much \n", left));
			ret =-1;
			goto exit;
		}

		group_cipher = 0; pairwise_cipher = 0;
		if(parse_wpa_ie(buf, param->u.wpa_ie.len, &group_cipher, &pairwise_cipher) == _SUCCESS)
		{
			padapter->securitypriv.dot11AuthAlgrthm= 2;
			padapter->securitypriv.ndisauthtype=Ndis802_11AuthModeWPAPSK;
		}

		group_cipher = 0; pairwise_cipher = 0;
		if(parse_wpa2_ie(buf, param->u.wpa_ie.len, &group_cipher, &pairwise_cipher) == _SUCCESS)
		{
			padapter->securitypriv.dot11AuthAlgrthm= 2;
			padapter->securitypriv.ndisauthtype=Ndis802_11AuthModeWPA2PSK;
		}

		switch (group_cipher)
		{
			case WPA_CIPHER_NONE:
				padapter->securitypriv.dot118021XGrpPrivacy=_NO_PRIVACY_;
				padapter->securitypriv.ndisencryptstatus=Ndis802_11EncryptionDisabled;
				break;
			case WPA_CIPHER_WEP40:
				padapter->securitypriv.dot118021XGrpPrivacy=_WEP40_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
			case WPA_CIPHER_TKIP:
				padapter->securitypriv.dot118021XGrpPrivacy=_TKIP_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
				break;
			case WPA_CIPHER_CCMP:
				padapter->securitypriv.dot118021XGrpPrivacy=_AES_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
				break;
			case WPA_CIPHER_WEP104:
				padapter->securitypriv.dot118021XGrpPrivacy=_WEP104_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
		}

		switch(pairwise_cipher)
		{
			case WPA_CIPHER_NONE:
				padapter->securitypriv.dot11PrivacyAlgrthm=_NO_PRIVACY_;
				padapter->securitypriv.ndisencryptstatus=Ndis802_11EncryptionDisabled;
				break;
			case WPA_CIPHER_WEP40:
				padapter->securitypriv.dot11PrivacyAlgrthm=_WEP40_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
			case WPA_CIPHER_TKIP:
				padapter->securitypriv.dot11PrivacyAlgrthm=_TKIP_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
				printk("(TKIP Pairwise)ndisencryptstatus=%d\n", padapter->securitypriv.ndisencryptstatus);
				break;
			case WPA_CIPHER_CCMP:
				padapter->securitypriv.dot11PrivacyAlgrthm=_AES_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
				printk("(AES Pairwise)ndisencryptstatus=%d\n",padapter->securitypriv.ndisencryptstatus);
				break;
			case WPA_CIPHER_WEP104:
				padapter->securitypriv.dot11PrivacyAlgrthm=_WEP104_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
		}
	}

	RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("wpa_set_wpa_ie: pairwise_cipher=%d padapter->securitypriv.ndisencryptstatus =%d padapter->securitypriv.ndisauthtype=%d\n",pairwise_cipher,padapter->securitypriv.ndisencryptstatus,padapter->securitypriv.ndisauthtype));

exit:

	return ret;
}
#endif

int wpa_supplicant_ioctl(struct net_device *dev, struct iw_point *p)
{
	struct ieee_param *param;
	uint ret=0;
	_adapter *padapter = (_adapter *) _netdev_priv(dev);

	//down(&ieee->wx_sem);
	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("+wpa_supplicant_ioctl: cmd=%d\n", param->cmd));

	if (p->length < sizeof(struct ieee_param) || !p->pointer){
		ret = -EINVAL;
		goto out;
	}

	param = (struct ieee_param *)_malloc(p->length);
	if (param == NULL)
	{
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, p->pointer, p->length))
	{
		_mfree((u8*)param, 0);
		ret = -EFAULT;
		goto out;
	}

	switch (param->cmd)
	{
		case IEEE_CMD_SET_WPA_PARAM:
			ret = wpa_set_param(dev, param->u.wpa_param.name, param->u.wpa_param.value);
			break;

		case IEEE_CMD_SET_WPA_IE:
			//ret = wpa_set_wpa_ie(dev, param, p->length);
			ret =  r871x_set_wpa_ie( padapter, (char*)param->u.wpa_ie.data, (u16)param->u.wpa_ie.len);
			break;

		case IEEE_CMD_SET_ENCRYPTION:
			ret = wpa_set_encryption(dev, param, p->length);
			break;

		case IEEE_CMD_MLME:
			ret = wpa_mlme(dev, param->u.mlme.command, param->u.mlme.reason_code);
			break;

		default:
			printk(KERN_NOTICE "Unknown WPA supplicant request: %d\n", param->cmd);
			ret = -EOPNOTSUPP;
			break;
	}

	if (ret == 0 && copy_to_user(p->pointer, param, p->length))
		ret = -EFAULT;

	_mfree((u8 *)param, sizeof(struct ieee_param));

out:

	//up(&ieee->wx_sem);

	return ret;
}

//based on "driver_ipw" and for hostapd
int r871x_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct iwreq *wrq = (struct iwreq *)rq;
	int ret = 0;

	//down(&priv->wx_sem);

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
		 ("+r871x_ioctl: cmd=%x\n", cmd));

	switch (cmd)
	{
		case RTL_IOCTL_WPA_SUPPLICANT:
			ret = wpa_supplicant_ioctl(dev, &wrq->u.data);
			break;
#ifdef CONFIG_HOSTAPD_MODE
		case RTL_IOCTL_HOSTAPD:
			ret = rtl871x_hostapd_ioctl(dev, &wrq->u.data);
			break;
#endif
		default:
			ret = -EOPNOTSUPP;
			break;
	}

	//up(&priv->wx_sem);

	return ret;
}

static iw_handler r8711_handlers[] =
{
	NULL,				/* SIOCSIWCOMMIT */
	r8711_wx_get_name,		/* SIOCGIWNAME */
        dummy,                    			/* SIOCSIWNWID */
        dummy,                   			 /* SIOCGIWNWID */
	r8711_wx_set_freq,		/* SIOCSIWFREQ */
	r8711_wx_get_freq,		/* SIOCGIWFREQ */
	r8711_wx_set_mode,		/* SIOCSIWMODE */
	r8711_wx_get_mode,		/* SIOCGIWMODE */
        dummy,//r8711_wx_set_sens,       /* SIOCSIWSENS */
	r8711_wx_get_sens,		/* SIOCGIWSENS */
	NULL,				/* SIOCSIWRANGE */
	r8711_wx_get_range,		/* SIOCGIWRANGE */
	r871x_wx_set_priv,		/* SIOCSIWPRIV */
	NULL,				/* SIOCGIWPRIV */
	NULL,				/* SIOCSIWSTATS */
	NULL,				/* SIOCGIWSTATS */
	dummy,				/* SIOCSIWSPY */
	dummy,				/* SIOCGIWSPY */
	NULL,				/* SIOCGIWTHRSPY */
	NULL,				/* SIOCWIWTHRSPY */
	r8711_wx_set_wap,		/* SIOCSIWAP */
	r8711_wx_get_wap,		/* SIOCGIWAP */
#if (WIRELESS_EXT >= 18)
	r871x_wx_set_mlme,		/* request MLME operation; uses struct iw_mlme */
#else
	NULL,                           /* request MLME operation; uses struct iw_mlme */
#endif
        dummy,                     		/* SIOCGIWAPLIST -- depricated */
	r8711_wx_set_scan,		/* SIOCSIWSCAN */
	r8711_wx_get_scan,		/* SIOCGIWSCAN */
	r8711_wx_set_essid,		/* SIOCSIWESSID */
	r8711_wx_get_essid,		/* SIOCGIWESSID */
	dummy,				/* SIOCSIWNICKN */
	r871x_wx_get_nick,		/* SIOCGIWNICKN */
	NULL,				/* -- hole -- */
	NULL,				/* -- hole -- */
	r8711_wx_set_rate,		/* SIOCSIWRATE */
	r8711_wx_get_rate,		/* SIOCGIWRATE */
	dummy,				/* SIOCSIWRTS */
	r8711_wx_get_rts,		/* SIOCGIWRTS */
	r8711_wx_set_frag,		/* SIOCSIWFRAG */
	r8711_wx_get_frag,		/* SIOCGIWFRAG */
	dummy,				/* SIOCSIWTXPOW */
	dummy,				/* SIOCGIWTXPOW */
	dummy,//r8711_wx_set_retry,	/* SIOCSIWRETRY */
	r8711_wx_get_retry,		/* SIOCGIWRETRY */
	r8711_wx_set_enc,		/* SIOCSIWENCODE */
	r8711_wx_get_enc,		/* SIOCGIWENCODE */
	dummy,				/* SIOCSIWPOWER */
	r8711_wx_get_power,		/* SIOCGIWPOWER */
	NULL,				/*---hole---*/
	NULL,				/*---hole---*/
	r871x_wx_set_gen_ie,		/* SIOCSIWGENIE */
	NULL,				/* SIOCGIWGENIE */
#if (WIRELESS_EXT >= 18)
	r871x_wx_set_auth,		/* SIOCSIWAUTH */
	NULL,				/* SIOCGIWAUTH */
	r871x_wx_set_enc_ext,		/* SIOCSIWENCODEEXT */
	NULL,				/* SIOCGIWENCODEEXT */
	r871x_wx_set_pmkid,		/* SIOCSIWPMKSA */
#else
	NULL,                           /* SIOCSIWAUTH */
	NULL,                           /* SIOCGIWAUTH */
	NULL,                           /* SIOCSIWENCODEEXT */
	NULL,                           /* SIOCGIWENCODEEXT */
	NULL,                           /* SIOCSIWPMKSA */
#endif
	NULL,				/*---hole---*/
};

static const struct iw_priv_args r8711_private_args[] =
{
	{
		SIOCIWFIRSTPRIV + 0x0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "write"
	},
	{
		SIOCIWFIRSTPRIV + 0x1,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | IFNAMSIZ, "read"
	},
	{
		SIOCIWFIRSTPRIV + 0x2, 0, 0, "driver_ext"
	},
	{
		SIOCIWFIRSTPRIV + 0x3, 0, 0, "" // mp_ioctl
	},
	{
		SIOCIWFIRSTPRIV + 0x4,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "apinfo"
	},
	{
		SIOCIWFIRSTPRIV + 0x5,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setpid"
        }, 
	{
                SIOCIWFIRSTPRIV + 0x6,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps_start"
	},
	{
		SIOCIWFIRSTPRIV + 0x7,
		IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "poweroff"
	},
	{
		   SIOCIWFIRSTPRIV + 0x8,		
		   IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "sleep_wakeup"	
	},	
#ifdef RTK_DMP_PLATFORM
	,
	{
		SIOCIWFIRSTPRIV + 0x9,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "apstatus"
	},
	{
		SIOCIWFIRSTPRIV + 0xa,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "countrycode"
	}
#else
	{
		   SIOCIWFIRSTPRIV + 0x9,		
		   IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "disconnect_ctrl"	
	},	
#endif

};

static iw_handler r8711_private_handler[] =
{
	r8711_wx_write32,
	r8711_wx_read32,
	r8711_drvext_hdl,
	r871x_mp_ioctl_hdl,
	r871x_get_ap_info, /*for MM DTV platform*/
	r871x_set_pid,
	r871x_wps_start,
	r8712_power_off,
	 r8712_sleep_wakeup,
#ifdef RTK_DMP_PLATFORM
	r871x_wx_get_ap_status, /* offset must be 9 for DMP platform*/
	r871x_wx_set_countrycode, //Set Channel depend on the country code
#else	
	r8712_disconnect_ctrl,
#endif


};

#if WIRELESS_EXT >= 17
static struct iw_statistics *r871x_get_wireless_stats(struct net_device *dev)
{
	_adapter *padapter = (_adapter *) _netdev_priv(dev);
	struct iw_statistics *piwstats = &padapter->iwstats;
	int tmp_level = 0;
	int tmp_qual = 0;
	int tmp_noise = 0;

	if (check_fwstate(&padapter->mlmepriv, _FW_LINKED) != _TRUE)
	{
		piwstats->qual.qual = 0;
		piwstats->qual.level = 0;
		piwstats->qual.noise = 0;
		//printk("No link  level:%d, qual:%d, noise:%d\n", tmp_level, tmp_qual, tmp_noise);
	}
	else {
		//show percentage 0~100, we need transfer dbm to orignal value.
		tmp_level = padapter->recvpriv.fw_rssi;
		tmp_qual = padapter->recvpriv.signal;
		tmp_noise = padapter->recvpriv.noise;
		//printk("level:%d, qual:%d, noise:%d\n", tmp_level, tmp_qual, tmp_noise);

		piwstats->qual.level = tmp_level;
		piwstats->qual.qual = tmp_qual;
		piwstats->qual.noise = tmp_noise;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14))
	piwstats->qual.level =((piwstats->qual.level +1)>>1)-95;
	piwstats->qual.updated = (u8)(IW_QUAL_QUAL_UPDATED | IW_QUAL_LEVEL_UPDATED | IW_QUAL_NOISE_INVALID|IW_QUAL_DBM);
	//piwstats->qual.updated = IW_QUAL_ALL_UPDATED;

#else
//#ifdef RTK_DMP_PLATFORM
	//IW_QUAL_DBM= 0x8, if driver use this flag, wireless extension will show value of dbm.
	//remove this flag for show percentage 0~100
	piwstats->qual.updated = 0x07;
//#else
	//piwstats->qual.updated = 0x0f;
//#endif
#endif

	return &padapter->iwstats;
}
#endif

struct iw_handler_def r871x_handlers_def =
{
	.standard = r8711_handlers,
	.num_standard = sizeof(r8711_handlers) / sizeof(iw_handler),
	.private = r8711_private_handler,
	.private_args = (struct iw_priv_args *)r8711_private_args,
	.num_private = sizeof(r8711_private_handler) / sizeof(iw_handler),
 	.num_private_args = sizeof(r8711_private_args) / sizeof(struct iw_priv_args),
#if WIRELESS_EXT >= 17
	.get_wireless_stats = r871x_get_wireless_stats,
#endif
};

