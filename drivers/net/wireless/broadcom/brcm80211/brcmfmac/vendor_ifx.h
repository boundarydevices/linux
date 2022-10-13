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
	SCMD(RSV11)		= 11,
	SCMD(RSV12)		= 12,
	SCMD(RSV13)		= 13,
	SCMD(RSV14)		= 14,
	SCMD(RSV15)		= 15,
	SCMD(MAX)		= 16
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

