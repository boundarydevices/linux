/*
 * drivers/amlogic/wifi/dhd_static_buf.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt)	"Wifi: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/amlogic/dhd_buf.h>

#define CONFIG_BROADCOM_WIFI_RESERVED_MEM

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_PKT_BUF			4
#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO		7
#define WLAN_STATIC_DHD_WLFC_INFO		8
#define PREALLOC_WLAN_SEC_NUM		6
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)
#define WLAN_SECTION_SIZE_7	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_8	(PREALLOC_WLAN_BUF_NUM * 512)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_7 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_8 + PREALLOC_WLAN_SECTION_HEADER)}
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
void *bcmdhd_mem_prealloc(int section, unsigned long size)
{
	if (section == WLAN_STATIC_PKT_BUF) {
		pr_info("1 %s: section=%d, wlan_static_skb=%p\n",
			__func__, section, wlan_static_skb);
		return wlan_static_skb;
	}
	if (section == WLAN_STATIC_SCAN_BUF0) {
		pr_info("2 %s: section=%d, wlan_static_scan_buf0=%p\n",
			__func__, section, wlan_static_scan_buf0);
		return wlan_static_scan_buf0;
	}
	if (section == WLAN_STATIC_SCAN_BUF1) {
		pr_info("3 %s: section=%d, wlan_static_scan_buf1=%p\n",
			__func__, section, wlan_static_scan_buf1);
		return wlan_static_scan_buf1;
	}
	if (section == WLAN_STATIC_DHD_INFO) {
		pr_info("4 %s: section=%d, wlan_mem_array[4]=%p\n",
			__func__, section, wlan_mem_array[4].mem_ptr);
		return wlan_mem_array[4].mem_ptr;
	}
	if (section == WLAN_STATIC_DHD_WLFC_INFO) {
		pr_info("5 %s: section=%d, wlan_mem_array[5]=%p\n",
			__func__, section, wlan_mem_array[5].mem_ptr);
		return wlan_mem_array[5].mem_ptr;
	}
	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM)) {
		pr_info("5 %s: out of section %d\n", __func__, section);
		return NULL;
	}

	if (wlan_mem_array[section].size < size) {
		pr_info("6 %s: wlan_mem_array[section].size=%lu, size=%lu\n",
			__func__, wlan_mem_array[section].size, size);
		return NULL;
	}
	pr_info("7 %s: wlan_mem_array[section].mem_ptr=%p, size=%lu\n",
		__func__, &wlan_mem_array[section], size);

	return wlan_mem_array[section].mem_ptr;
}
EXPORT_SYMBOL(bcmdhd_mem_prealloc);

int bcmdhd_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
		pr_debug("1 %s: wlan_static_skb[%d]=%p, size=%lu\n",
			__func__, i, wlan_static_skb[i], DHD_SKB_1PAGE_BUFSIZE);
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
		pr_debug("2 %s: wlan_static_skb[%d]=%p, size=%lu\n",
			__func__, i, wlan_static_skb[i], DHD_SKB_2PAGE_BUFSIZE);
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;
	pr_debug("3 %s: wlan_static_skb[%d]=%p, size=%lu\n",
		__func__, i, wlan_static_skb[i], DHD_SKB_4PAGE_BUFSIZE);

	for (i = 0; i < PREALLOC_WLAN_SEC_NUM; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
		pr_debug("4 %s: wlan_mem_array[%d]=%p, size=%lu\n",
			__func__, i, wlan_static_skb[i],
			wlan_mem_array[i].size);
	}

	wlan_static_scan_buf0 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;
	pr_debug("5 %s: wlan_static_scan_buf0=%p, size=%d\n",
		__func__, wlan_static_scan_buf0, 65536);

	wlan_static_scan_buf1 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;
	pr_debug("6 %s: wlan_static_scan_buf1=%p, size=%d\n",
		__func__, wlan_static_scan_buf1, 65536);

	pr_info("%s: WIFI MEM Allocated\n", __func__);
	return 0;

err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
EXPORT_SYMBOL(bcmdhd_init_wlan_mem);
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("wifi device tree driver");
