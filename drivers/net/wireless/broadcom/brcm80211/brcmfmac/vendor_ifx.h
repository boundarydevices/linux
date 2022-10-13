/* SPDX-License-Identifier: ISC
 *
 * Infineon Technologies OUI and vendor specific assignments
 *
 * $ Copyright Infineon $
 */

#ifndef IFX_VENDOR_H
#define IFX_VENDOR_H

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
enum ifx_nl80211_vendor_subcmds {
	/* TODO: IFX Vendor subcmd enum IDs between 1-15 are reserved
	 * to be filled later with BRCM Vendor subcmds that are
	 * already used by IFX.
	 */
	IFX_VENDOR_SCMD_UNSPEC		= 0,
	/* Reserved 1-5 */
	IFX_VENDOR_SCMD_FRAMEBURST	= 6,
	/* Reserved 7-15 */
	IFX_VENDOR_SCMD_DCMD		= 16,
	IFX_VENDOR_SCMD_MAX		= 17
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

#endif /* IFX_VENDOR_H */

