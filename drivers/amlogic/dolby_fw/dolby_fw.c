/*
 * drivers/amlogic/dolby_fw/dolby_fw.c
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

#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/amlogic/secmon.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <linux/arm-smccc.h>
#include "dolby_fw.h"

#define DOLBY_FW_DEVICE_NAME   "dolby_fw"
#define DOLBY_FW_DRIVER_NAME   "dolby_fw"
#define DOLBY_FW_CLASS_NAME    "dolby_fw"
#define DOLBY_FW_MODULE_NAME   "dolby_fw"

#undef pr_fmt
#define pr_fmt(fmt) "dolbyfw: " fmt

static int major_id;
static struct class *class_dolby_fw;
static struct device *dev_dolby_fw;
static int mem_size;
void __iomem *sharemem_in_base;
void __iomem *sharemem_out_base;

#define IOCTL_SIGNATURE	0x10
#define IOCTL_DECRYPT		0x20
#define IOCTL_VERIFY		0x30
#define IOCTL_GET_SESSION_ID	0x40
#define IOCTL_VERIFY_LIB_RESP	0x41
#define IOCTL_AUDIO_LICENSE_QUERY 0x50
#define IOCTL_CRITICAL_QUERY 0x60

#define AUDIO_LICENSE_QUERY_MAX_SIZE	0x40
#define DOLBY_FW_CRITICAL_MAX_SIZE 0x400

struct dolby_fw_args {
	uint64_t src_addr;
	unsigned int src_len;
	uint64_t dest_addr;
	unsigned int dest_len;
	uint64_t arg1;
	uint64_t arg2;
	uint64_t arg3;
};

static int dolby_fw_smc_call(u64 function_id, u64 arg1, u64 arg2, u64 arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg1, arg2, arg3, 0, 0, 0, 0, &res);

	return res.a0;
}

static int dolby_fw_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int dolby_fw_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int dolby_fw_signature(struct dolby_fw_args *info)
{
	int ret;

	sharemem_mutex_lock();
	ret = copy_from_user(sharemem_in_base,
			(void *)(uintptr_t)info->src_addr,
			info->src_len);
	if (ret != 0) {
		pr_err("%s:%d: copy signature data fail!\n",
					__func__, __LINE__);
		goto sig_err;
	}
	ret = dolby_fw_smc_call(DOLBY_FW_SIGNATURE, info->src_len, 0, 0);
sig_err:
	sharemem_mutex_unlock();

	return ret;
}

static int dolby_fw_decrypt(struct dolby_fw_args *info)
{
	int ret = -1;
	void __iomem *mem_in_virt = NULL, *mem_out_virt = NULL;
	int i, data_size, size, process_size;
	phys_addr_t in_phy, out_phy;

	if (info->src_len != info->dest_len) {
		pr_err("%s:%d: args are error!\n",
					__func__, __LINE__);
		goto err;
	}
	data_size = info->src_len;

	mem_in_virt = kmalloc(mem_size, GFP_KERNEL);
	if (!mem_in_virt)
		goto err;

	mem_out_virt = kmalloc(mem_size, GFP_KERNEL);
	if (!mem_out_virt)
		goto err1;

	in_phy = virt_to_phys(mem_in_virt);
	out_phy = virt_to_phys(mem_out_virt);

	for (i = 0, size = 0; i <= data_size/mem_size; i++) {
		if (size + mem_size <= data_size)
			process_size = mem_size;
		else
			process_size = data_size - size;

		ret = copy_from_user(mem_in_virt,
				(void *)(uintptr_t)(info->src_addr+size),
				process_size);
		if (ret != 0) {
			pr_err("%s:%d: decrypt copy data fail!\n",
					__func__, __LINE__);
			goto err2;
		}

		__dma_map_area(mem_in_virt, process_size, DMA_TO_DEVICE);
		__dma_map_area(mem_out_virt, process_size, DMA_FROM_DEVICE);
		ret = dolby_fw_smc_call(DOLBY_FW_DECRYPT,
						in_phy, out_phy, process_size);
		if (ret) {
			pr_err("%s:%d: audio firmware decrypt fail!\n",
					__func__, __LINE__);
			goto err2;
		}

	  __dma_unmap_area(mem_out_virt, process_size, DMA_FROM_DEVICE);
		ret = copy_to_user(
				(void *)(uintptr_t)(info->dest_addr+size),
				mem_out_virt, process_size);
		if (ret != 0) {
			pr_err("%s:%d: decrypt write data fail!\n",
					__func__, __LINE__);
			goto err2;
		}
		size += process_size;
	}
	ret = 0;

err2:
	kfree(mem_out_virt);
err1:
	kfree(mem_in_virt);
err:
	return ret;
}

static int dolby_fw_verify(struct dolby_fw_args *info)
{
	int ret = -1;
	int i, data_size, size, process_size, cnt;
	void __iomem *mem_in_virt = NULL;
	phys_addr_t in_phy;
	unsigned char pre_shared[32];

	if (info->dest_len < 0x20) {
		pr_err("%s:%d: verify dest len erro!\n",
					__func__, __LINE__);
		goto err;
	}
	if (!info->arg2) {
		pr_err("%s:%d: critical data size error!\n",
				__func__, __LINE__);
		goto err;
	}

	data_size = info->src_len;
	cnt = (data_size/mem_size);

	mem_in_virt = kmalloc(mem_size, GFP_KERNEL);
	if (!mem_in_virt)
		goto err;

	in_phy = virt_to_phys(mem_in_virt);

	for (i = 0, size = 0; i < cnt; i++) {
		ret = copy_from_user(mem_in_virt,
				(void *)(uintptr_t)(info->src_addr+size),
				mem_size);
		if (ret != 0) {
			pr_err("%s:%d: verify copy data fail!\n",
					__func__, __LINE__);
			goto err1;
		}
		__dma_map_area(mem_in_virt, mem_size, DMA_TO_DEVICE);
		ret = dolby_fw_smc_call(DOLBY_FW_VERIFY_UPDATE,
				in_phy, mem_size, 0);
		if (ret) {
			pr_err("%s:%d: audio firmware verify update fail!\n",
				__func__, __LINE__);
			goto err1;
		}
		size += mem_size;
	}

	process_size = data_size - size;
	sharemem_mutex_lock();
	if (process_size) {
		ret = copy_from_user(mem_in_virt,
				(void *)(uintptr_t)(info->src_addr+size),
				process_size);
		if (ret != 0) {
			pr_err("%s:%d: verify copy data fail!\n",
					__func__, __LINE__);
			goto err2;
		}
		__dma_map_area(mem_in_virt, process_size, DMA_TO_DEVICE);
	}

	ret = dolby_fw_smc_call(DOLBY_FW_VERIFY_FINAL, in_phy, process_size, 0);
	if (ret) {
		pr_err("%s:%d: audio firmware verify fail!\n",
				__func__, __LINE__);
		goto err2;
	}
	memcpy(pre_shared, (void *)sharemem_out_base, 0x20);

	/*critical data*/
	ret = copy_from_user(sharemem_in_base,
						(void *)(uintptr_t)(info->arg1),
						info->arg2);
	if (ret != 0) {
		pr_err("%s:%d: critical copy data fail!\n",
				__func__, __LINE__);
		goto err2;
	}
	ret = dolby_fw_smc_call(DOLBY_FW_VERIFY_CRITICAL, info->arg2, 0, 0);
	if (ret) {
		pr_err("%s:%d: verify critical fail!\n",
				__func__, __LINE__);
		goto err2;
	}

	/* copy share memory output for pre_shared_secret */
	ret = copy_to_user((void *)(uintptr_t)(info->dest_addr),
				(void *)pre_shared, 0x20);
	if (ret != 0) {
		pr_err("%s:%d: verify copy data fail!\n",
				__func__, __LINE__);
		ret = -1;
		goto err2;
	}

err2:
	sharemem_mutex_unlock();
err1:
		kfree(mem_in_virt);
err:
	return ret;
}

static int dolby_fw_get_session_id(struct dolby_fw_args *info)
{
	int ret = -1;

	sharemem_mutex_lock();
	dolby_fw_smc_call(DOLBY_FW_GET_SESSION_ID, 0, 0, 0);
	ret = copy_to_user(
				(void *)(uintptr_t)(info->dest_addr),
				sharemem_out_base, 16+4);
	if (ret != 0) {
		pr_err("%s:%d: get session id fail!\n",
				__func__, __LINE__);
		ret = -1;
	}
	sharemem_mutex_unlock();
	return ret;
}

static int dolby_fw_verify_lib_resp(struct dolby_fw_args *info)
{
	int ret = -1;

	if (info->src_len < (16+32) || (info->src_len > 0x1000)) {
		pr_err("verify lib resp input len error!\n");
		goto err;
	}
	if (info->dest_len < 32) {
		pr_err("verify lib resp output len error!\n");
		goto err;
	}

	sharemem_mutex_lock();
	ret = copy_from_user(sharemem_in_base,
				(void *)(uintptr_t)info->src_addr,
				info->src_len);
	if (ret != 0) {
		pr_err("%s:%d: copy verify lib resp fail!\n",
					__func__, __LINE__);
		goto err1;
	}
	ret = dolby_fw_smc_call(DOLBY_FW_VERIFY_LIB_RESP, 0, 0, 0);
	if (ret) {
		pr_err("%s:%d: verify lib resp fail %d!\n",
					__func__, __LINE__, ret);
		ret = -1;
		goto err1;
	}
	ret = copy_to_user((void *)(uintptr_t)(info->dest_addr),
			sharemem_out_base, 32);
	if (ret != 0) {
		pr_err("%s:%d: verify lib resp return fail!\n",
			__func__, __LINE__);
		ret = -1;
	}
err1:
	sharemem_mutex_unlock();
err:
	return ret;
}

static unsigned int dolby_fw_audio_license_query(struct dolby_fw_args *info)
{
	unsigned int ret = 0;
	unsigned int outlen;

	if (info->src_len > AUDIO_LICENSE_QUERY_MAX_SIZE) {
		pr_err("%s:%d: audio license query src length error!\n",
					__func__, __LINE__);
		ret = 0;
		goto err;
	}

	sharemem_mutex_lock();
	ret = copy_from_user(sharemem_in_base,
			(void *)(uintptr_t)info->src_addr,
			info->src_len);
	if (ret != 0) {
		pr_err("%s:%d: copy audio license query fail!\n",
					__func__, __LINE__);
		ret = 0;
		goto err1;
	}
	outlen = dolby_fw_smc_call(DOLBY_FW_AUDIO_LICENSE_QUERY,
			info->src_len, info->dest_len, 0);
	if (!outlen || (info->dest_len < outlen)) {
		pr_err("%s:%d: audio license query fail!\n",
			__func__, __LINE__);
		goto err1;
	}
	ret = copy_to_user((void *)(uintptr_t)(info->dest_addr),
			sharemem_out_base, outlen);
	if (ret != 0) {
		pr_err("%s:%d: audio license query fail!\n",
			__func__, __LINE__);
		ret = 0;
	}
	ret = outlen;
err1:
	sharemem_mutex_unlock();
err:
	return ret;
}

static int dolby_fw_critical_query(struct dolby_fw_args *info)
{
	int ret = 0;
	unsigned int size;

	sharemem_mutex_lock();

	size = dolby_fw_smc_call(DOLBY_FW_CRITICAL_DATA_QUERY, 0, 0, 0);
	if (!size) {
		pr_err("%s:%d: query critical fail!\n",
				__func__, __LINE__);
		goto err;
	}

	if (info->dest_len < size || size > DOLBY_FW_CRITICAL_MAX_SIZE) {
		pr_err("%s:%d: critical size error!\n",
				__func__, __LINE__);
		goto err;
	}
	ret = copy_to_user(
				(void *)(uintptr_t)(info->dest_addr),
				sharemem_out_base, size);
	if (ret != 0) {
		pr_err("%s:%d: get critical fail!\n",
				__func__, __LINE__);
		ret = 0;
	}
	ret = size;

err:
	sharemem_mutex_unlock();
	return ret;
}


static long dolby_fw_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	void __user	*argp = (void __user *)arg;
	struct dolby_fw_args info;
	int			ret;

	if (!argp) {
		pr_err("%s:%d,wrong param\n",
				__func__, __LINE__);
		return -1;
	}

	ret = copy_from_user(&info, (struct dolby_fw_args *)argp,
				sizeof(struct dolby_fw_args));
	if (ret != 0) {
		pr_err("%s:%d,copy_from_user fail\n",
			__func__, __LINE__);
		return ret;
	}

	switch (cmd) {
	case IOCTL_SIGNATURE:
		ret = dolby_fw_signature(&info);
		break;
	case IOCTL_DECRYPT:
		ret = dolby_fw_decrypt(&info);
		break;
	case IOCTL_VERIFY:
		ret = dolby_fw_verify(&info);
		break;
	case IOCTL_GET_SESSION_ID:
		ret = dolby_fw_get_session_id(&info);
		break;
	case IOCTL_VERIFY_LIB_RESP:
		ret = dolby_fw_verify_lib_resp(&info);
		break;
	case IOCTL_AUDIO_LICENSE_QUERY:
		ret = dolby_fw_audio_license_query(&info);
		break;
	case IOCTL_CRITICAL_QUERY:
		ret = dolby_fw_critical_query(&info);
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static struct file_operations const dolby_fw_fops = {
	.owner = THIS_MODULE,
	.open = dolby_fw_open,
	.release = dolby_fw_release,
	.unlocked_ioctl = dolby_fw_ioctl,
	.compat_ioctl = dolby_fw_ioctl,
};

static int dolby_fw_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int val;

	ret = register_chrdev(0, DOLBY_FW_DEVICE_NAME,
						&dolby_fw_fops);
	if (ret < 0) {
		pr_err("Can't register %s divece", DOLBY_FW_DEVICE_NAME);
		ret = -ENODEV;
		goto err;
	}
	major_id = ret;

	class_dolby_fw = class_create(THIS_MODULE,
						DOLBY_FW_DEVICE_NAME);
	if (IS_ERR(class_dolby_fw)) {
		ret = PTR_ERR(class_dolby_fw);
		goto err1;
	}

	dev_dolby_fw = device_create(class_dolby_fw, NULL,
					MKDEV(major_id, 0),
					NULL, DOLBY_FW_DEVICE_NAME);
	if (IS_ERR(dev_dolby_fw)) {
		ret = PTR_ERR(dev_dolby_fw);
		goto err2;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "mem_size", &val);
	if (ret) {
		dev_err(&pdev->dev, "config mem_size in dts\n");
		ret = -1;
		goto err3;
	}
	mem_size = val;

	sharemem_in_base = get_secmon_sharemem_input_base();
	sharemem_out_base = get_secmon_sharemem_output_base();

	return 0;

err3:
	device_destroy(class_dolby_fw, MKDEV(major_id, 0));
err2:
	class_destroy(class_dolby_fw);
err1:
	unregister_chrdev(major_id, DOLBY_FW_DEVICE_NAME);
err:
	return ret;
}

static int dolby_fw_remove(struct platform_device *pdev)
{
	device_destroy(class_dolby_fw, MKDEV(major_id, 0));
	class_destroy(class_dolby_fw);
	unregister_chrdev(major_id, DOLBY_FW_DEVICE_NAME);
	return 0;
}

static const struct of_device_id amlogic_dolby_fw_dt_match[] = {
	{	.compatible = "amlogic, dolby_fw",
	},
	{},
};
static struct platform_driver dolby_fw_driver = {
	.probe = dolby_fw_probe,
	.remove = dolby_fw_remove,
	.driver = {
		.name = DOLBY_FW_DEVICE_NAME,
		.of_match_table = amlogic_dolby_fw_dt_match,
		.owner = THIS_MODULE,
	},
};
static int __init dolby_fw_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&dolby_fw_driver);
	if (ret != 0) {
		pr_err("failed to register dolby fw driver, error %d\n", ret);
		return -ENODEV;
	}
	return ret;
}
/* module unload */
static  void __exit dolby_fw_exit(void)
{
	platform_driver_unregister(&dolby_fw_driver);
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Amlogic DOLBY FW Interface driver");

module_init(dolby_fw_init);
module_exit(dolby_fw_exit);
