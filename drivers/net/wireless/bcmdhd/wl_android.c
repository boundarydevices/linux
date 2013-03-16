/*
 * Linux cfg80211 driver - Android related functions
 *
 * Copyright (C) 1999-2011, Broadcom Corporation
 * 
 *         Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: wl_android.c,v 1.1.4.1.2.14 2011/02/09 01:40:07 Exp $
 */

#include <linux/module.h>
#include <linux/netdevice.h>

#include <wl_android.h>
#include <wldev_common.h>
#include <wlioctl.h>
#include <bcmutils.h>
#include <linux_osl.h>
#include <dhd_dbg.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <bcmsdbus.h>
#ifdef WL_CFG80211
#include <wl_cfg80211.h>
#endif
#if defined(CONFIG_WIFI_CONTROL_FUNC)
#include <linux/platform_device.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
#include <linux/wlan_plat.h>
#else
#include <linux/wifi_tiwlan.h>
#endif
#endif /* CONFIG_WIFI_CONTROL_FUNC */

#define dtoh32(i) i
#define dtoh16(i) i

/* message levels */
#define ANDROID_ERROR_LEVEL	0x0001
#define ANDROID_TRACE_LEVEL	0x0002
#define ANDROID_INFO_LEVEL	0x0004

uint android_msg_level = ANDROID_ERROR_LEVEL;

#define ANDROID_ERR(x) \
	do { \
		if (android_msg_level & ANDROID_ERROR_LEVEL) { \
			printk(KERN_ERR "ANDROID-ERROR) ");	\
			printk x; \
		} \
	} while (0)
#define ANDROID_TRACE(x) \
	do { \
		if (android_msg_level & ANDROID_TRACE_LEVEL) { \
			printk(KERN_ERR "ANDROID-ERROR) ");	\
			printk x; \
		} \
	} while (0)
#define ANDROID_INFO(x) \
	do { \
		if (android_msg_level & ANDROID_INFO_LEVEL) { \
			printk(KERN_ERR "ANDROID-ERROR) : ");	\
			printk x; \
		} \
	} while (0)

/*
 * Android private command strings, PLEASE define new private commands here
 * so they can be updated easily in the future (if needed)
 */

#define CMD_START				"START"
#define CMD_STOP				"STOP"
#define CMD_SCAN_ACTIVE			"SCAN-ACTIVE"
#define CMD_SCAN_PASSIVE		"SCAN-PASSIVE"
#define CMD_RSSI				"RSSI"
#define CMD_LINKSPEED			"LINKSPEED"
#define CMD_RXFILTER_START		"RXFILTER-START"
#define CMD_RXFILTER_STOP		"RXFILTER-STOP"
#define CMD_RXFILTER_ADD		"RXFILTER-ADD"
#define CMD_RXFILTER_REMOVE		"RXFILTER-REMOVE"
#define CMD_BTCOEXSCAN_START	"BTCOEXSCAN-START"
#define CMD_BTCOEXSCAN_STOP		"BTCOEXSCAN-STOP"
#define CMD_BTCOEXMODE			"BTCOEXMODE"
#define CMD_SETSUSPENDOPT		"SETSUSPENDOPT"
#define CMD_P2P_DEV_ADDR		"P2P_DEV_ADDR"
#define CMD_SETFWPATH			"SETFWPATH"
#define CMD_SETBAND				"SETBAND"
#define CMD_GETBAND				"GETBAND"
#define CMD_COUNTRY				"COUNTRY"
#define CMD_P2P_SET_NOA			"P2P_SET_NOA"
#define CMD_P2P_SET_PS			"P2P_SET_PS"
#define CMD_SET_AP_WPS_P2P_IE	"SET_AP_WPS_P2P_IE"


#ifdef PNO_SUPPORT
#define CMD_PNOSSIDCLR_SET	"PNOSSIDCLR"
#define CMD_PNOSETUP_SET	"PNOSETUP "
#define CMD_PNOENABLE_SET	"PNOFORCE"
#define CMD_PNODEBUG_SET	"PNODEBUG"

#define PNO_TLV_PREFIX			'S'
#define PNO_TLV_VERSION			'1'
#define PNO_TLV_SUBVERSION 		'2'
#define PNO_TLV_RESERVED		'0'
#define PNO_TLV_TYPE_SSID_IE		'S'
#define PNO_TLV_TYPE_TIME		'T'
#define PNO_TLV_FREQ_REPEAT		'R'
#define PNO_TLV_FREQ_EXPO_MAX		'M'

typedef struct cmd_tlv {
	char prefix;
	char version;
	char subver;
	char reserved;
} cmd_tlv_t;
#endif /* PNO_SUPPORT */

typedef struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;

/**
 * Extern function declarations (TODO: move them to dhd_linux.h)
 */
void dhd_customer_gpio_wlan_ctrl(int onoff);
uint dhd_dev_reset(struct net_device *dev, uint8 flag);
void dhd_dev_init_ioctl(struct net_device *dev);
#ifdef WL_CFG80211
int wl_cfg80211_get_p2p_dev_addr(struct net_device *net, struct ether_addr *p2pdev_addr);
int wl_cfg80211_set_btcoex_dhcp(struct net_device *dev, char *command);
#else
int wl_cfg80211_get_p2p_dev_addr(struct net_device *net, struct ether_addr *p2pdev_addr)
{ return 0; }
int wl_cfg80211_set_p2p_noa(struct net_device *net, char* buf, int len)
{ return 0; }
int wl_cfg80211_get_p2p_noa(struct net_device *net, char* buf, int len)
{ return 0; }
int wl_cfg80211_set_p2p_ps(struct net_device *net, char* buf, int len)
{ return 0; }
#endif
extern int dhd_os_check_if_up(void *dhdp);
extern void *bcmsdh_get_drvdata(void);

extern bool ap_fw_loaded;
extern char iface_name[IFNAMSIZ];

/**
 * Local (static) functions and variables
 */

/* Initialize g_wifi_on to 1 so dhd_bus_start will be called for the first
 * time (only) in dhd_open, subsequential wifi on will be handled by
 * wl_android_wifi_on
 */
int g_wifi_on = G_WLAN_SET_ON;

/**
 * Local (static) function definitions
 */
static int wl_android_get_link_speed(struct net_device *net, char *command, int total_len)
{
	int link_speed;
	int bytes_written;
	int error;

	error = wldev_get_link_speed(net, &link_speed);
	if (error)
		return -1;

	/* Convert Kbps to Android Mbps */
	link_speed = link_speed / 1000;
	bytes_written = snprintf(command, total_len, "LinkSpeed %d", link_speed);
	ANDROID_INFO(("%s: command result is %s\n", __FUNCTION__, command));
	return bytes_written;
}

static int wl_android_get_rssi(struct net_device *net, char *command, int total_len)
{
	wlc_ssid_t ssid = {0};
	int rssi;
	int bytes_written = 0;
	int error;

	error = wldev_get_rssi(net, &rssi);
	if (error)
		return -1;
#if defined(RSSIOFFSET) || 1
	rssi = wl_update_rssi_offset(rssi);
#endif

	error = wldev_get_ssid(net, &ssid);
	if (error)
		return -1;
	if ((ssid.SSID_len == 0) || (ssid.SSID_len > DOT11_MAX_SSID_LEN)) {
		ANDROID_ERR(("%s: wldev_get_ssid failed\n", __FUNCTION__));
	} else {
		memcpy(command, ssid.SSID, ssid.SSID_len);
		bytes_written = ssid.SSID_len;
	}
	bytes_written += snprintf(&command[bytes_written], total_len, " rssi %d", rssi);
	ANDROID_INFO(("%s: command result is %s (%d)\n", __FUNCTION__, command, bytes_written));
	return bytes_written;
}

static int wl_android_set_suspendopt(struct net_device *dev, char *command, int total_len)
{
	int suspend_flag;
	int ret_now;
	int ret = 0;

	suspend_flag = *(command + strlen(CMD_SETSUSPENDOPT) + 1) - '0';

	if (suspend_flag != 0)
		suspend_flag = 1;
	ret_now = net_os_set_suspend_disable(dev, suspend_flag);

	if (ret_now != suspend_flag) {
		if (!(ret = net_os_set_suspend(dev, ret_now)))
			ANDROID_INFO(("%s: Suspend Flag %d -> %d\n",
				__FUNCTION__, ret_now, suspend_flag));
		else
			ANDROID_ERR(("%s: failed %d\n", __FUNCTION__, ret));
	}
	return ret;
}

static int wl_android_get_band(struct net_device *dev, char *command, int total_len)
{
	uint band;
	int bytes_written;
	int error;

	error = wldev_get_band(dev, &band);
	if (error)
		return -1;
	bytes_written = snprintf(command, total_len, "Band %d", band);
	return bytes_written;
}

#ifdef PNO_SUPPORT
static int wl_android_set_pno_setup(struct net_device *dev, char *command, int total_len)
{
	wlc_ssid_t ssids_local[MAX_PFN_LIST_COUNT];
	int res = -1;
	int nssid = 0;
	cmd_tlv_t *cmd_tlv_temp;
	char *str_ptr;
	int tlv_size_left;
	int pno_time = 0;
	int pno_repeat = 0;
	int pno_freq_expo_max = 0;

#ifdef PNO_SET_DEBUG
	int i;
	char pno_in_example[] = {
		'P', 'N', 'O', 'S', 'E', 'T', 'U', 'P', ' ',
		'S', '1', '2', '0',
		'S',
		0x05,
		'd', 'l', 'i', 'n', 'k',
		'S',
		0x04,
		'G', 'O', 'O', 'G',
		'T',
		'0', 'B',
		'R',
		'2',
		'M',
		'2',
		0x00
		};
#endif /* PNO_SET_DEBUG */

	ANDROID_INFO(("%s: command=%s, len=%d\n", __FUNCTION__, command, total_len));

	if (total_len < (strlen(CMD_PNOSETUP_SET) + sizeof(cmd_tlv_t))) {
		ANDROID_ERR(("%s argument=%d less min size\n", __FUNCTION__, total_len));
		goto exit_proc;
	}


#ifdef PNO_SET_DEBUG
	memcpy(command, pno_in_example, sizeof(pno_in_example));
	for (i = 0; i < sizeof(pno_in_example); i++)
		printf("%02X ", command[i]);
	printf("\n");
	total_len = sizeof(pno_in_example);
#endif

	str_ptr = command + strlen(CMD_PNOSETUP_SET);
	tlv_size_left = total_len - strlen(CMD_PNOSETUP_SET);

	cmd_tlv_temp = (cmd_tlv_t *)str_ptr;
	memset(ssids_local, 0, sizeof(ssids_local));

	if ((cmd_tlv_temp->prefix == PNO_TLV_PREFIX) &&
		(cmd_tlv_temp->version == PNO_TLV_VERSION) &&
		(cmd_tlv_temp->subver == PNO_TLV_SUBVERSION)) {

		str_ptr += sizeof(cmd_tlv_t);
		tlv_size_left -= sizeof(cmd_tlv_t);

		if ((nssid = wl_iw_parse_ssid_list_tlv(&str_ptr, ssids_local,
			MAX_PFN_LIST_COUNT, &tlv_size_left)) <= 0) {
			ANDROID_ERR(("SSID is not presented or corrupted ret=%d\n", nssid));
			goto exit_proc;
		} else {
			if ((str_ptr[0] != PNO_TLV_TYPE_TIME) || (tlv_size_left <= 1)) {
				ANDROID_ERR(("%s scan duration corrupted field size %d\n",
					__FUNCTION__, tlv_size_left));
				goto exit_proc;
			}
			str_ptr++;
			pno_time = simple_strtoul(str_ptr, &str_ptr, 16);
			ANDROID_INFO(("%s: pno_time=%d\n", __FUNCTION__, pno_time));

			if (str_ptr[0] != 0) {
				if ((str_ptr[0] != PNO_TLV_FREQ_REPEAT)) {
					ANDROID_ERR(("%s pno repeat : corrupted field\n",
						__FUNCTION__));
					goto exit_proc;
				}
				str_ptr++;
				pno_repeat = simple_strtoul(str_ptr, &str_ptr, 16);
				ANDROID_INFO(("%s :got pno_repeat=%d\n", __FUNCTION__, pno_repeat));
				if (str_ptr[0] != PNO_TLV_FREQ_EXPO_MAX) {
					ANDROID_ERR(("%s FREQ_EXPO_MAX corrupted field size\n",
						__FUNCTION__));
					goto exit_proc;
				}
				str_ptr++;
				pno_freq_expo_max = simple_strtoul(str_ptr, &str_ptr, 16);
				ANDROID_INFO(("%s: pno_freq_expo_max=%d\n",
					__FUNCTION__, pno_freq_expo_max));
			}
		}
	} else {
		ANDROID_ERR(("%s get wrong TLV command\n", __FUNCTION__));
		goto exit_proc;
	}

	res = dhd_dev_pno_set(dev, ssids_local, nssid, pno_time, pno_repeat, pno_freq_expo_max);

exit_proc:
	return res;
}
#endif /* PNO_SUPPORT */

static int wl_android_get_p2p_dev_addr(struct net_device *ndev, char *command, int total_len)
{
	int ret;
	int bytes_written = 0;

	ret = wl_cfg80211_get_p2p_dev_addr(ndev, (struct ether_addr*)command);
	if (ret)
		return 0;
	bytes_written = sizeof(struct ether_addr);
	return bytes_written;
}

/**
 * Global function definitions (declared in wl_android.h)
 */

int wl_android_wifi_on(struct net_device *dev)
{
	int ret = 0;

	printk("%s in\n", __FUNCTION__);
	if (!dev) {
		ANDROID_ERR(("%s: dev is null\n", __FUNCTION__));
		return -EINVAL;
	}

	dhd_net_if_lock(dev);
	if (g_wifi_on == G_WLAN_SET_OFF) {
		dhd_customer_gpio_wlan_ctrl(WLAN_RESET_ON);
		sdioh_start(NULL, 0);
		ret = dhd_dev_reset(dev, FALSE);
		sdioh_start(NULL, 1);
		if (!ret)
			dhd_dev_init_ioctl(dev);
		g_wifi_on = G_WLAN_SET_ON;
	}
	dhd_net_if_unlock(dev);

	return ret;
}

int wl_android_wifi_off(struct net_device *dev)
{
	int ret = 0;

	printk("%s in\n", __FUNCTION__);
	if (!dev) {
		ANDROID_TRACE(("%s: dev is null\n", __FUNCTION__));
		return -EINVAL;
	}

	dhd_net_if_lock(dev);
	if (g_wifi_on == G_WLAN_SET_ON) {
		ret = dhd_dev_reset(dev, TRUE);
		sdioh_stop(NULL);
		dhd_customer_gpio_wlan_ctrl(WLAN_RESET_OFF);
		g_wifi_on = G_WLAN_SET_OFF;
	}
	dhd_net_if_unlock(dev);

	return ret;
}

static int wl_android_set_fwpath(struct net_device *net, char *command, int total_len)
{
	if ((strlen(command) - strlen(CMD_SETFWPATH)) > MOD_PARAM_PATHLEN)
		return -1;
	bcm_strncpy_s(fw_path, sizeof(fw_path),
		command + strlen(CMD_SETFWPATH) + 1, MOD_PARAM_PATHLEN - 1);
	if (strstr(fw_path, "apsta") != NULL) {
		ANDROID_INFO(("GOT APSTA FIRMWARE\n"));
		ap_fw_loaded = TRUE;
	} else {
		ANDROID_INFO(("GOT STA FIRMWARE\n"));
		ap_fw_loaded = FALSE;
	}
	return 0;
}

int wl_android_priv_cmd(struct net_device *net, struct ifreq *ifr, int cmd)
{
	int ret = 0;
	char *command = NULL;
	int bytes_written = 0;
	android_wifi_priv_cmd priv_cmd;

	net_os_wake_lock(net);

	if (!ifr->ifr_data) {
		ret = -EINVAL;
		goto exit;
	}
	if (copy_from_user(&priv_cmd, ifr->ifr_data, sizeof(android_wifi_priv_cmd))) {
		ret = -EFAULT;
		goto exit;
	}
	command = kmalloc(priv_cmd.total_len, GFP_KERNEL);
	if (!command)
	{
		ANDROID_ERR(("%s: failed to allocate memory\n", __FUNCTION__));
		ret = -ENOMEM;
		goto exit;
	}
	if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto exit;
	}

	ANDROID_INFO(("%s: Android private cmd \"%s\" on %s\n", __FUNCTION__, command, ifr->ifr_name));

	if (strnicmp(command, CMD_START, strlen(CMD_START)) == 0) {
		ANDROID_INFO(("%s, Received regular START command\n", __FUNCTION__));
		bytes_written = wl_android_wifi_on(net);
	}
	else if (strnicmp(command, CMD_SETFWPATH, strlen(CMD_SETFWPATH)) == 0) {
		bytes_written = wl_android_set_fwpath(net, command, priv_cmd.total_len);
	}

	if (g_wifi_on == G_WLAN_SET_OFF) {
		ANDROID_ERR(("%s: Ignore private cmd \"%s\" - iface %s is down\n",
			__FUNCTION__, command, ifr->ifr_name));
		ret = 0;
		goto exit;
	}

	if (strnicmp(command, CMD_STOP, strlen(CMD_STOP)) == 0) {
		bytes_written = wl_android_wifi_off(net);
	}
	else if (strnicmp(command, CMD_SCAN_ACTIVE, strlen(CMD_SCAN_ACTIVE)) == 0) {
		/* TBD: SCAN-ACTIVE */
	}
	else if (strnicmp(command, CMD_SCAN_PASSIVE, strlen(CMD_SCAN_PASSIVE)) == 0) {
		/* TBD: SCAN-PASSIVE */
	}
	else if (strnicmp(command, CMD_RSSI, strlen(CMD_RSSI)) == 0) {
		bytes_written = wl_android_get_rssi(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_LINKSPEED, strlen(CMD_LINKSPEED)) == 0) {
		bytes_written = wl_android_get_link_speed(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_RXFILTER_START, strlen(CMD_RXFILTER_START)) == 0) {
		bytes_written = net_os_set_packet_filter(net, 1);
	}
	else if (strnicmp(command, CMD_RXFILTER_STOP, strlen(CMD_RXFILTER_STOP)) == 0) {
		bytes_written = net_os_set_packet_filter(net, 0);
	}
	else if (strnicmp(command, CMD_RXFILTER_ADD, strlen(CMD_RXFILTER_ADD)) == 0) {
		int filter_num = *(command + strlen(CMD_RXFILTER_ADD) + 1) - '0';
		bytes_written = net_os_rxfilter_add_remove(net, TRUE, filter_num);
	}
	else if (strnicmp(command, CMD_RXFILTER_REMOVE, strlen(CMD_RXFILTER_REMOVE)) == 0) {
		int filter_num = *(command + strlen(CMD_RXFILTER_REMOVE) + 1) - '0';
		bytes_written = net_os_rxfilter_add_remove(net, FALSE, filter_num);
	}
	else if (strnicmp(command, CMD_BTCOEXSCAN_START, strlen(CMD_BTCOEXSCAN_START)) == 0) {
		/* TBD: BTCOEXSCAN-START */
	}
	else if (strnicmp(command, CMD_BTCOEXSCAN_STOP, strlen(CMD_BTCOEXSCAN_STOP)) == 0) {
		/* TBD: BTCOEXSCAN-STOP */
	}
	else if (strnicmp(command, CMD_BTCOEXMODE, strlen(CMD_BTCOEXMODE)) == 0) {
		uint mode = *(command + strlen(CMD_BTCOEXMODE) + 1) - '0';

		if (mode == 1)
			net_os_set_packet_filter(net, 0); /* DHCP starts */
		else
			net_os_set_packet_filter(net, 1); /* DHCP ends */
#ifdef WL_CFG80211
		bytes_written = wl_cfg80211_set_btcoex_dhcp(net, command);
#endif
	}
	else if (strnicmp(command, CMD_SETSUSPENDOPT, strlen(CMD_SETSUSPENDOPT)) == 0) {
		bytes_written = wl_android_set_suspendopt(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_SETBAND, strlen(CMD_SETBAND)) == 0) {
		uint band = *(command + strlen(CMD_SETBAND) + 1) - '0';
		bytes_written = wldev_set_band(net, band);
	}
	else if (strnicmp(command, CMD_GETBAND, strlen(CMD_GETBAND)) == 0) {
		bytes_written = wl_android_get_band(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_COUNTRY, strlen(CMD_COUNTRY)) == 0) {
		char *country_code = command + strlen(CMD_COUNTRY) + 1;
		bytes_written = wldev_set_country(net, country_code);
	}
#ifdef PNO_SUPPORT
	else if (strnicmp(command, CMD_PNOSSIDCLR_SET, strlen(CMD_PNOSSIDCLR_SET)) == 0) {
		bytes_written = dhd_dev_pno_reset(net);
	}
	else if (strnicmp(command, CMD_PNOSETUP_SET, strlen(CMD_PNOSETUP_SET)) == 0) {
		bytes_written = wl_android_set_pno_setup(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_PNOENABLE_SET, strlen(CMD_PNOENABLE_SET)) == 0) {
		uint pfn_enabled = *(command + strlen(CMD_PNOENABLE_SET) + 1) - '0';
		bytes_written = dhd_dev_pno_enable(net, pfn_enabled);
	}
#endif
	else if (strnicmp(command, CMD_P2P_DEV_ADDR, strlen(CMD_P2P_DEV_ADDR)) == 0) {
		bytes_written = wl_android_get_p2p_dev_addr(net, command, priv_cmd.total_len);
	}
	else if (strnicmp(command, CMD_P2P_SET_NOA, strlen(CMD_P2P_SET_NOA)) == 0) {
		int skip = strlen(CMD_P2P_SET_NOA) + 1;
		bytes_written = wl_cfg80211_set_p2p_noa(net, command + skip,
			priv_cmd.total_len - skip);
	}
	else if (strnicmp(command, CMD_P2P_SET_PS, strlen(CMD_P2P_SET_PS)) == 0) {
		int skip = strlen(CMD_P2P_SET_PS) + 1;
		bytes_written = wl_cfg80211_set_p2p_ps(net, command + skip,
			priv_cmd.total_len - skip);
	}
#ifdef WL_CFG80211
	else if (strnicmp(command, CMD_SET_AP_WPS_P2P_IE,
		strlen(CMD_SET_AP_WPS_P2P_IE)) == 0) {
		int skip = strlen(CMD_SET_AP_WPS_P2P_IE) + 3;
		bytes_written = wl_cfg80211_set_wps_p2p_ie(net, command + skip,
			priv_cmd.total_len - skip, *(command + skip - 2) - '0');
	}
#endif /* WL_CFG80211 */
	else {
		ANDROID_ERR(("Unknown PRIVATE command %s - ignored\n", command));
		snprintf(command, 3, "OK");
		bytes_written = strlen("OK");
	}

	if (bytes_written >= 0) {
		if ((bytes_written == 0) && (priv_cmd.total_len > 0))
			command[0] = '\0';
		if (bytes_written >= priv_cmd.total_len) {
			ANDROID_ERR(("%s: bytes_written = %d\n", __FUNCTION__, bytes_written));
			bytes_written = priv_cmd.total_len;
		} else {
			bytes_written++;
		}
		priv_cmd.used_len = bytes_written;
		if (copy_to_user(priv_cmd.buf, command, bytes_written)) {
			ANDROID_ERR(("%s: failed to copy data to user buffer\n", __FUNCTION__));
			ret = -EFAULT;
		}
	}
	else {
		ret = bytes_written;
	}

exit:
	net_os_wake_unlock(net);
	if (command) {
		kfree(command);
	}

	return ret;
}

int wl_android_init(void)
{
	int ret = 0;

	dhd_msg_level |= DHD_ERROR_VAL;
#ifdef ENABLE_INSMOD_NO_FW_LOAD
	dhd_download_fw_on_driverload = FALSE;
#endif /* ENABLE_INSMOD_NO_FW_LOAD */
	if (!iface_name[0]) {
		memset(iface_name, 0, IFNAMSIZ);
		bcm_strncpy_s(iface_name, IFNAMSIZ, "wlan", IFNAMSIZ);
	}
	return ret;
}

int wl_android_exit(void)
{
	int ret = 0;

	return ret;
}

void wl_android_post_init(void)
{
	if (!dhd_download_fw_on_driverload) {
		/* Call customer gpio to turn off power with WL_REG_ON signal */
		dhd_customer_gpio_wlan_ctrl(WLAN_RESET_OFF);
		g_wifi_on = G_WLAN_SET_OFF;
	}
}

#if defined(RSSIAVG)
void
wl_free_rssi_cache(wl_rssi_cache_ctrl_t *rssi_cache_ctrl)
{
	wl_rssi_cache_t *node, *cur, **rssi_head;
	int i=0;

	rssi_head = &rssi_cache_ctrl->m_cache_head;
	node = *rssi_head;

	for (;node;) {
//		ANDROID_ERR(("%s : Free %d with BSSID %pM\n",
//			__FUNCTION__, i, &node->BSSID));
		cur = node;
		node = cur->next;
		kfree(cur);
		i++;
	}
	*rssi_head = NULL;
}

void
wl_delete_dirty_rssi_cache(wl_rssi_cache_ctrl_t *rssi_cache_ctrl)
{
	wl_rssi_cache_t *node, *prev, **rssi_head;
	int i = -1, tmp = 0;
#if defined(BSSCACHE)
	int max = BSSCACHE_LEN;
#else
	int max = 1;
#endif

	rssi_head = &rssi_cache_ctrl->m_cache_head;
	node = *rssi_head;
	prev = node;
	for (;node;) {
		i++;
		if (node->dirty >= max) {
			if (node == *rssi_head) {
				tmp = 1;
				*rssi_head = node->next;
			} else {
				tmp = 0;
				prev->next = node->next;
			}
			ANDROID_TRACE(("%s : Del %d with BSSID %pM\n",
				__FUNCTION__, i, &node->BSSID));
			kfree(node);
			if (tmp == 1) {
				node = *rssi_head;
				prev = node;
			} else {
				node = prev->next;
			}
			continue;
		}
		prev = node;
		node = node->next;
	}
}

void
wl_reset_rssi_cache(wl_rssi_cache_ctrl_t *rssi_cache_ctrl)
{
	wl_rssi_cache_t *node, **rssi_head;

	rssi_head = &rssi_cache_ctrl->m_cache_head;

	/* reset dirty */
	node = *rssi_head;
	for (;node;) {
		node->dirty += 1;
//		ANDROID_ERR(("%s : Reset BSSID %pM, dirty=%d\n",
//				__FUNCTION__, &node->BSSID, node->dirty));
		node = node->next;
	}
}

void
wl_update_rssi_cache(wl_rssi_cache_ctrl_t *rssi_cache_ctrl, wl_scan_results_t *ss_list)
{
	wl_rssi_cache_t *node, *prev, *leaf, **rssi_head;
	wl_bss_info_t *bi = NULL;
	int i, j, k;

	if (!ss_list->count)
		return;

	rssi_head = &rssi_cache_ctrl->m_cache_head;

	/* update RSSI */
	for (i = 0; i < ss_list->count; i++) {
		node = *rssi_head;
		prev = NULL;
		k = 0;
		bi = bi ? (wl_bss_info_t *)((uintptr)bi + dtoh32(bi->length)) : ss_list->bss_info;
		for (;node;) {
			if (!memcmp(&node->BSSID, &bi->BSSID, ETHER_ADDR_LEN)) {
//				ANDROID_TRACE(("%s : Update %d with BSSID %pM, RSSI=%d, SSID \"%s\"\n",
//					__FUNCTION__, k, &bi->BSSID, dtoh16(bi->RSSI), bi->SSID));
				for(j=0; j<RSSIAVG_LEN-1; j++)
					node->RSSI[j] = node->RSSI[j+1];
				node->RSSI[j] = bi->RSSI;
				node->dirty = 0;
				break;
			}
			prev = node;
			node = node->next;
			k++;
		}

		if (node)
			continue;

		leaf = kmalloc(sizeof(wl_rssi_cache_t), GFP_KERNEL);
		if (!leaf) {
			ANDROID_ERR(("%s : Memory alloc failure %d\n",
				__FUNCTION__, sizeof(wl_rssi_cache_t)));
			return;
		}
//		ANDROID_TRACE(("%s: Add %d with cached BSSID %pM, RSSI=%d, SSID \"%s\" in the leaf\n",
//				__FUNCTION__, k, &bi->BSSID, dtoh16(bi->RSSI), bi->SSID));

		leaf->next = NULL;
		leaf->dirty = 0;
		memcpy(&leaf->BSSID, &bi->BSSID, ETHER_ADDR_LEN);
		for (j=0; j<RSSIAVG_LEN; j++)
			leaf->RSSI[j] = bi->RSSI;

		if (!prev)
			*rssi_head = leaf;
		else
			prev->next = leaf;
	}
}

int16
wl_get_avg_rssi(wl_rssi_cache_ctrl_t *rssi_cache_ctrl, void *addr)
{
	wl_rssi_cache_t *node, **rssi_head;
	int j, rssi_sum, rssi=-200;

	rssi_head = &rssi_cache_ctrl->m_cache_head;

	/* reset dirty */
	node = *rssi_head;
	for (;node;) {
		if (!memcmp(&node->BSSID, addr, ETHER_ADDR_LEN)) {
			rssi_sum = 0;
			rssi = 0;
			for (j=0; j<RSSIAVG_LEN; j++)
				rssi_sum += node->RSSI[RSSIAVG_LEN-j-1];
			rssi = rssi_sum / j;
			break;
		}
		node = node->next;
	}
	if (rssi >= -2)
		rssi = -2;
	if (rssi == -200) {
		ANDROID_ERR(("%s: BSSID %pM does not in RSSI cache\n",
		__FUNCTION__, addr));
	}
	return (int16)rssi;
}
#endif

#if defined(RSSIOFFSET) || 1
uint chip;
uint chiprev;

int
wl_update_rssi_offset(int rssi)
{
	if (chip == BCM4330_CHIP_ID && chiprev == BCM4330B2_CHIP_REV) {
#if defined(RSSIOFFSET_NEW)
		int j;
		for (j=0; j<RSSI_OFFSET; j++) {
			if (rssi - (RSSI_MIN+RSSI_INT*(j+1)) < 0)
				break;
		}
		rssi += j;
#else
		rssi += RSSI_OFFSET;
#endif
	}
	if (rssi >= -2)
		rssi = -2;
	return rssi;
}
#endif

#if defined(BSSCACHE)
#define WLC_IW_SS_CACHE_CTRL_FIELD_MAXLEN	32

void
wl_free_bss_cache(wl_bss_cache_ctrl_t *bss_cache_ctrl)
{
	wl_bss_cache_t *node, *cur, **bss_head;
	int i=0;

	ANDROID_TRACE(("%s called\n", __FUNCTION__));

	bss_head = &bss_cache_ctrl->m_cache_head;
	node = *bss_head;

	for (;node;) {
		ANDROID_TRACE(("%s : Free %d with BSSID %pM\n",
			__FUNCTION__, i, &node->results.bss_info->BSSID));
		cur = node;
		node = cur->next;
		kfree(cur);
		i++;
	}
	*bss_head = NULL;
}

void
wl_delete_dirty_bss_cache(wl_bss_cache_ctrl_t *bss_cache_ctrl)
{
	wl_bss_cache_t *node, *prev, **bss_head;
	int i = -1, tmp = 0;

	bss_head = &bss_cache_ctrl->m_cache_head;
	node = *bss_head;
	prev = node;
	for (;node;) {
		i++;
		if (node->dirty >= BSSCACHE_LEN) {
			if (node == *bss_head) {
				tmp = 1;
				*bss_head = node->next;
			} else {
				tmp = 0;
				prev->next = node->next;
			}
			ANDROID_TRACE(("%s : Del %d with BSSID %pM, RSSI=%d, SSID \"%s\"\n",
				__FUNCTION__, i, &node->results.bss_info->BSSID,
				dtoh16(node->results.bss_info->RSSI), node->results.bss_info->SSID));
			kfree(node);
			if (tmp == 1) {
				node = *bss_head;
				prev = node;
			} else {
				node = prev->next;
			}
			continue;
		}
		prev = node;
		node = node->next;
	}
}

void
wl_reset_bss_cache(wl_bss_cache_ctrl_t *bss_cache_ctrl)
{
	wl_bss_cache_t *node, **bss_head;

	bss_head = &bss_cache_ctrl->m_cache_head;

	/* reset dirty */
	node = *bss_head;
	for (;node;) {
		node->dirty += 1;
		node = node->next;
	}
}

void
wl_update_bss_cache(wl_bss_cache_ctrl_t *bss_cache_ctrl, wl_scan_results_t *ss_list)
{
	wl_bss_cache_t *node, *prev, *leaf, *tmp, **bss_head;
	wl_bss_info_t *bi = NULL;
	int i, k=0;

	if (!ss_list->count) {
		return;
	}

	bss_head = &bss_cache_ctrl->m_cache_head;

	for (i=0; i < ss_list->count; i++) {
		node = *bss_head;
		prev = NULL;
		bi = bi ? (wl_bss_info_t *)((uintptr)bi + dtoh32(bi->length)) : ss_list->bss_info;
		
		for (;node;) {
			if (!memcmp(&node->results.bss_info->BSSID, &bi->BSSID, ETHER_ADDR_LEN)) {
 				tmp = node;
				leaf = kmalloc(bi->length + WLC_IW_SS_CACHE_CTRL_FIELD_MAXLEN, GFP_KERNEL);
				if (!leaf) {
					ANDROID_ERR(("%s: Memory alloc failure %d and keep old BSS info\n",
						__FUNCTION__, bi->length + WLC_IW_SS_CACHE_CTRL_FIELD_MAXLEN));
					break;
				}

				memcpy(leaf->results.bss_info, bi, bi->length);
				leaf->next = node->next;
				leaf->dirty = 0;
				leaf->results.count = 1;
				leaf->results.version = ss_list->version;

				ANDROID_TRACE(("%s : Update %d with BSSID %pM, RSSI=%d, SSID \"%s\"\n",
					__FUNCTION__, k, &bi->BSSID, dtoh16(bi->RSSI), bi->SSID));
				if (!prev)
					*bss_head = leaf;
				else
					prev->next = leaf;
				node = leaf;
				prev = node;

				kfree(tmp);
				k++;
				break;
			}
			prev = node;
			node = node->next;
		}

		if (node)
			continue;

		leaf = kmalloc(bi->length + WLC_IW_SS_CACHE_CTRL_FIELD_MAXLEN, GFP_KERNEL);
		if (!leaf) {
			ANDROID_ERR(("%s: Memory alloc failure %d\n", __FUNCTION__,
				bi->length + WLC_IW_SS_CACHE_CTRL_FIELD_MAXLEN));
			return;
		}
		ANDROID_TRACE(("%s: Add %d with cached BSSID %pM, RSSI=%d, SSID \"%s\" in the leaf\n",
				__FUNCTION__, k, &bi->BSSID, dtoh16(bi->RSSI), bi->SSID));

		memcpy(leaf->results.bss_info, bi, bi->length);
		leaf->next = NULL;
		leaf->dirty = 0;
		leaf->results.count = 1;
		leaf->results.version = ss_list->version;
		k++;

		if (!prev)
			*bss_head = leaf;
		else
			prev->next = leaf;
	}
}

void
wl_run_bss_cache_timer(wl_bss_cache_ctrl_t *bss_cache_ctrl, int kick_off)
{
	struct timer_list **timer;

	timer = &bss_cache_ctrl->m_timer;

	if (*timer) {
		if (kick_off) {
			(*timer)->expires = jiffies + BSSCACHE_TIME * HZ / 1000;
			add_timer(*timer);
			ANDROID_TRACE(("%s : timer starts\n", __FUNCTION__));
		} else {
			del_timer_sync(*timer);
			ANDROID_TRACE(("%s : timer stops\n", __FUNCTION__));
		}
	}
}

void
wl_set_bss_cache_timer_flag(ulong data)
{
	wl_bss_cache_ctrl_t *bss_cache_ctrl = (wl_bss_cache_ctrl_t *)data;

	bss_cache_ctrl->m_timer_expired = 1;
	ANDROID_TRACE(("%s called\n", __FUNCTION__));
}

void
wl_release_bss_cache_ctrl(wl_bss_cache_ctrl_t *bss_cache_ctrl)
{
	ANDROID_TRACE(("%s :\n", __FUNCTION__));
	wl_free_bss_cache(bss_cache_ctrl);
	wl_run_bss_cache_timer(bss_cache_ctrl, 0);
	if (bss_cache_ctrl->m_timer) {
		kfree(bss_cache_ctrl->m_timer);
	}
}

void
wl_init_bss_cache_ctrl(wl_bss_cache_ctrl_t *bss_cache_ctrl)
{
	ANDROID_TRACE(("%s :\n", __FUNCTION__));
	bss_cache_ctrl->m_timer_expired = 0;

	bss_cache_ctrl->m_timer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (!bss_cache_ctrl->m_timer) {
		ANDROID_ERR(("%s: Memory alloc failure\n", __FUNCTION__ ));
		return;
	}
	init_timer(bss_cache_ctrl->m_timer);
	bss_cache_ctrl->m_timer->function = (void *)wl_set_bss_cache_timer_flag;
	bss_cache_ctrl->m_timer->data = (ulong)bss_cache_ctrl;
}
#endif

/**
 * Functions for Android WiFi card detection
 */
#if defined(CONFIG_WIFI_CONTROL_FUNC)

static int g_wifidev_registered = 0;
static struct semaphore wifi_control_sem;
static struct wifi_platform_data *wifi_control_data = NULL;
static struct resource *wifi_irqres = NULL;

static int wifi_add_dev(void);
static void wifi_del_dev(void);

int wl_android_wifictrl_func_add(void)
{
	int ret = 0;
	sema_init(&wifi_control_sem, 0);

	ret = wifi_add_dev();
	if (ret) {
		ANDROID_ERR(("%s: platform_driver_register failed\n", __FUNCTION__));
		return ret;
	}
	g_wifidev_registered = 1;

	/* Waiting callback after platform_driver_register is done or exit with error */
	if (down_timeout(&wifi_control_sem,  msecs_to_jiffies(1000)) != 0) {
		ret = -EINVAL;
		ANDROID_ERR(("%s: platform_driver_register timeout\n", __FUNCTION__));
	}

	return ret;
}

void wl_android_wifictrl_func_del(void)
{
	if (g_wifidev_registered)
	{
		wifi_del_dev();
		g_wifidev_registered = 0;
	}
}

void* wl_android_prealloc(int section, unsigned long size)
{
	void *alloc_ptr = NULL;
	if (wifi_control_data && wifi_control_data->mem_prealloc) {
		alloc_ptr = wifi_control_data->mem_prealloc(section, size);
		if (alloc_ptr) {
			ANDROID_INFO(("success alloc section %d\n", section));
			if (size != 0L)
				bzero(alloc_ptr, size);
			return alloc_ptr;
		}
	}

	ANDROID_ERR(("can't alloc section %d\n", section));
	return NULL;
}

int wifi_get_irq_number(unsigned long *irq_flags_ptr)
{
	if (wifi_irqres) {
		*irq_flags_ptr = wifi_irqres->flags & IRQF_TRIGGER_MASK;
		return (int)wifi_irqres->start;
	}
#ifdef CUSTOM_OOB_GPIO_NUM
	return CUSTOM_OOB_GPIO_NUM;
#else
	return -1;
#endif
}

int wifi_set_power(int on, unsigned long msec)
{
	ANDROID_ERR(("%s = %d\n", __FUNCTION__, on));
	if (wifi_control_data && wifi_control_data->set_power) {
		wifi_control_data->set_power(on);
	}
	if (msec)
		msleep(msec);
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
int wifi_get_mac_addr(unsigned char *buf)
{
	ANDROID_ERR(("%s\n", __FUNCTION__));
	if (!buf)
		return -EINVAL;
	if (wifi_control_data && wifi_control_data->get_mac_addr) {
		return wifi_control_data->get_mac_addr(buf);
	}
	return -EOPNOTSUPP;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
void *wifi_get_country_code(char *ccode)
{
	ANDROID_TRACE(("%s\n", __FUNCTION__));
	if (!ccode)
		return NULL;
	if (wifi_control_data && wifi_control_data->get_country_code) {
		return wifi_control_data->get_country_code(ccode);
	}
	return NULL;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)) */

static int wifi_set_carddetect(int on)
{
	ANDROID_ERR(("%s = %d\n", __FUNCTION__, on));
	if (wifi_control_data && wifi_control_data->set_carddetect) {
		wifi_control_data->set_carddetect(on);
	}
	return 0;
}

static int wifi_probe(struct platform_device *pdev)
{
	struct wifi_platform_data *wifi_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);

	ANDROID_ERR(("## %s\n", __FUNCTION__));
	wifi_irqres = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "bcmdhd_wlan_irq");
	if (wifi_irqres == NULL)
		wifi_irqres = platform_get_resource_byname(pdev,
			IORESOURCE_IRQ, "bcm4329_wlan_irq");
	wifi_control_data = wifi_ctrl;

	wifi_set_power(1, 0);	/* Power On */
	wifi_set_carddetect(1);	/* CardDetect (0->1) */

	up(&wifi_control_sem);
	return 0;
}

static int wifi_remove(struct platform_device *pdev)
{
	struct wifi_platform_data *wifi_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);

	ANDROID_ERR(("## %s\n", __FUNCTION__));
	wifi_control_data = wifi_ctrl;

	wifi_set_power(0, 0);	/* Power Off */
	wifi_set_carddetect(0);	/* CardDetect (1->0) */

	up(&wifi_control_sem);
	return 0;
}

static int wifi_suspend(struct platform_device *pdev, pm_message_t state)
{
	ANDROID_TRACE(("##> %s\n", __FUNCTION__));
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY)
	bcmsdh_oob_intr_set(0);
#endif /* (OOB_INTR_ONLY) */
	return 0;
}

static int wifi_resume(struct platform_device *pdev)
{
	ANDROID_TRACE(("##> %s\n", __FUNCTION__));
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY)
	if (dhd_os_check_if_up(bcmsdh_get_drvdata()))
		bcmsdh_oob_intr_set(1);
#endif /* (OOB_INTR_ONLY) */
	return 0;
}

static struct platform_driver wifi_device = {
	.probe          = wifi_probe,
	.remove         = wifi_remove,
	.suspend        = wifi_suspend,
	.resume         = wifi_resume,
	.driver         = {
	.name   = "bcmdhd_wlan",
	}
};

static struct platform_driver wifi_device_legacy = {
	.probe          = wifi_probe,
	.remove         = wifi_remove,
	.suspend        = wifi_suspend,
	.resume         = wifi_resume,
	.driver         = {
	.name   = "bcm4329_wlan",
	}
};

static int wifi_add_dev(void)
{
	ANDROID_TRACE(("## Calling platform_driver_register\n"));
	platform_driver_register(&wifi_device);
	platform_driver_register(&wifi_device_legacy);
	return 0;
}

static void wifi_del_dev(void)
{
	ANDROID_TRACE(("## Unregister platform_driver_register\n"));
	platform_driver_unregister(&wifi_device);
	platform_driver_unregister(&wifi_device_legacy);
}
#endif /* defined(CONFIG_WIFI_CONTROL_FUNC) */
