/*
 * drivers/amlogic/mtd/aml_dtb.c
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

#include "aml_mtd.h"

#ifndef AML_NAND_UBOOT
#include<linux/cdev.h>
#include <linux/device.h>

#define DTB_NAME "dtb"
static dev_t amlnf_dtb_no;
struct cdev amlnf_dtb;
struct device *dtb_dev_nand;
struct class *amlnf_dtb_class;
#endif  /* AML_NAND_UBOOT */

int dtb_erase_blk = -1;

struct aml_nand_chip *aml_chip_dtb;

int amlnf_dtb_save(u8 *buf, int len)
{
	struct mtd_info *mtd = aml_chip_dtb->mtd;
	u8 *dtb_buf = NULL;
	int ret = 0;

	pr_info("%s: ####\n", __func__);
	if (aml_chip_dtb == NULL) {
		pr_info("%s: amlnf not init yet!\n", __func__);
		return -EFAULT;
	}

	if (len > aml_chip_dtb->dtbsize) {
		pr_info("warnning!!! %s: length too much\n", __func__);
		len = aml_chip_dtb->dtbsize;
		/*return -EFAULT;*/
	}
	dtb_buf = vmalloc(aml_chip_dtb->dtbsize + mtd->writesize);
	if (dtb_buf == NULL) {
		ret = -1;
		goto exit_err;
	}
	memset(dtb_buf, 0, aml_chip_dtb->dtbsize);
	memcpy(dtb_buf, buf, len);
	aml_nand_save_dtb(mtd, dtb_buf);

exit_err:
	if (dtb_buf) {
		/* kfree(dtb_buf); */
		vfree(dtb_buf);
		dtb_buf = NULL;
	}
	return ret;
}

int amlnf_dtb_erase(void)
{
	int ret = 0;

	if (aml_chip_dtb == NULL) {
		pr_info("%s amlnf not ready yet!\n", __func__);
		return -1;
	}
	return ret;
}

int amlnf_dtb_read(u8 *buf, int len)
{
	struct mtd_info *mtd = aml_chip_dtb->mtd;
	size_t offset = 0;
	u8 *dtb_buf = NULL;
	int ret = 0;

	pr_info("%s: ####\n", __func__);

	if (len > aml_chip_dtb->dtbsize) {
		pr_info("warnning!!! %s dtd length too much\n", __func__);
		len = aml_chip_dtb->dtbsize;
		/*return -EFAULT;*/
	}
	if (aml_chip_dtb == NULL) {
		memset(buf, 0x0, len);
		pr_info("%s amlnf not ready yet!\n", __func__);
		return 0;
	}

	dtb_buf = kzalloc(aml_chip_dtb->dtbsize, GFP_KERNEL);
	if (dtb_buf == NULL) {
		ret = -1;
		goto exit_err;
	}
	memset(dtb_buf, 0, aml_chip_dtb->dtbsize);
	aml_nand_read_dtb(mtd, offset, (u8 *)dtb_buf);
	memcpy(buf, dtb_buf, len);
exit_err:
	if (dtb_buf) {
		/* kfree(dtb_buf); */
		kfree(dtb_buf);
		dtb_buf = NULL;
	}
	return ret;
}

/* under kernel */
#ifndef AML_NAND_UBOOT
ssize_t dtb_show(struct class *class, struct class_attribute *attr,
		char *buf)
{
	pr_info("dtb_show : #####\n");
	/* fixme, read back and show in log! */
	return 0;
}

ssize_t dtb_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	u8 *dtb_ptr = NULL;

	pr_info("dtb_store : #####\n");
	dtb_ptr = kzalloc(aml_chip_dtb->dtbsize, GFP_KERNEL);
	if (dtb_ptr == NULL) {
		pr_info("%s: malloc buf failed\n ", __func__);
		return -ENOMEM;
	}
	/* fixme, why read back then write? */
	ret = amlnf_dtb_read(dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		pr_info("%s: read failed\n", __func__);
		kfree(dtb_ptr);
		return -EFAULT;
	}

	ret = amlnf_dtb_save(dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		pr_info("%s: save failed\n", __func__);
		kfree(dtb_ptr);
		return -EFAULT;
	}

	pr_info("dtb_store : OK #####\n");
	return count;
}

static CLASS_ATTR(nfdtb, 0644, dtb_show, dtb_store);

int dtb_open(struct inode *node, struct file *file)
{
	return 0;
}

ssize_t dtb_read(struct file *file,
		char __user *buf,
		size_t count,
		loff_t *ppos)
{
	u8 *dtb_ptr = NULL;
	/*struct nand_flash *flash = &aml_chip_dtb->flash;*/
	struct mtd_info *mtd = aml_chip_dtb->mtd;
	ssize_t read_size = 0;
	int ret = 0;

	if (*ppos == aml_chip_dtb->dtbsize)
		return 0;

	if (*ppos >= aml_chip_dtb->dtbsize) {
		pr_info("%s:data access out of space!\n", __func__);
		return -EFAULT;
	}

	dtb_ptr = vmalloc(aml_chip_dtb->dtbsize + mtd->writesize);
	if (dtb_ptr == NULL)
		return -ENOMEM;

	/*not need nand_get_device here, mtd->_read_xx will done with it*/
	/*nand_get_device(mtd, FL_READING);*/
	ret = amlnf_dtb_read((u8 *)dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		pr_info("%s: read failed:%d\n", __func__, ret);
		ret = -EFAULT;
		goto exit;
	}

	if ((*ppos + count) > aml_chip_dtb->dtbsize)
		read_size = aml_chip_dtb->dtbsize - *ppos;
	else
		read_size = count;

	ret = copy_to_user(buf, (dtb_ptr + *ppos), read_size);
	*ppos += read_size;
exit:
	/*nand_release_device(mtd);*/
	/* kfree(dtb_ptr); */
	vfree(dtb_ptr);
	return read_size;
}

ssize_t dtb_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)
{
	u8 *dtb_ptr = NULL;
	ssize_t write_size = 0;
	/*struct nand_flash *flash = &aml_chip_dtb->flash;*/
	struct mtd_info *mtd = aml_chip_dtb->mtd;
	int ret = 0;

	if (*ppos == aml_chip_dtb->dtbsize)
		return 0;

	if (*ppos >= aml_chip_dtb->dtbsize) {
		pr_info("%s: data access out of space!\n", __func__);
		return -EFAULT;
	}

	dtb_ptr = vmalloc(aml_chip_dtb->dtbsize + mtd->writesize);
	if (dtb_ptr == NULL)
		return -ENOMEM;

	/*not need nand_get_device here, mtd->_read_xx will done with it*/
	/*nand_get_device(mtd, FL_WRITING);*/
	ret = amlnf_dtb_read((u8 *)dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		pr_info("%s: read failed\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	if ((*ppos + count) > aml_chip_dtb->dtbsize)
		write_size = aml_chip_dtb->dtbsize - *ppos;
	else
		write_size = count;

	ret = copy_from_user((dtb_ptr + *ppos), buf, write_size);

	ret = amlnf_dtb_save(dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		pr_info("%s: read failed\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	*ppos += write_size;
exit:
	/*nand_release_device(mtd);*/
	/* kfree(dtb_ptr); */
	vfree(dtb_ptr);
	return write_size;
}

long dtb_ioctl(struct file *file, u32 cmd, unsigned long args)
{
	return 0;
}

static const struct file_operations dtb_ops = {
	.open = dtb_open,
	.read = dtb_read,
	.write = dtb_write,
	.unlocked_ioctl = dtb_ioctl,
};
#endif /* AML_NAND_UBOOT */

int amlnf_dtb_init(struct aml_nand_chip *aml_chip)
{
	int ret = 0;
	u8 *dtb_buf = NULL;

	aml_chip_dtb = aml_chip;

	dtb_buf = kzalloc(aml_chip_dtb->dtbsize, GFP_KERNEL);
	if (dtb_buf == NULL) {
		ret = -1;
		goto exit_err;
	}

	memset(dtb_buf, 0x0, aml_chip_dtb->dtbsize);

#ifndef AML_NAND_UBOOT
	pr_info("%s: register dtb cdev\n", __func__);
	ret = alloc_chrdev_region(&amlnf_dtb_no, 0, 1, DTB_NAME);
	if (ret < 0) {
		pr_info("%s alloc dtb dev_t number failed\n", __func__);
		ret = -1;
		goto exit_err;
	}

	cdev_init(&amlnf_dtb, &dtb_ops);
	amlnf_dtb.owner = THIS_MODULE;
	ret = cdev_add(&amlnf_dtb, amlnf_dtb_no, 1);
	if (ret) {
		pr_info("%s amlnf dtd dev add failed\n", __func__);
		ret = -1;
		goto exit_err1;
	}

	amlnf_dtb_class = class_create(THIS_MODULE, DTB_NAME);
	if (IS_ERR(amlnf_dtb_class)) {
		pr_info("%s: amlnf dtd class add failed\n", __func__);
		ret = -1;
		goto exit_err2;
	}

	ret = class_create_file(amlnf_dtb_class, &class_attr_nfdtb);
	if (ret) {
		pr_info("%s dev add failed\n", __func__);
		ret = -1;
		goto exit_err2;
	}

	dtb_dev_nand = device_create(amlnf_dtb_class,
		NULL,
		amlnf_dtb_no,
		NULL,
		DTB_NAME);
	if (IS_ERR(dtb_dev_nand)) {
		pr_info("%s: device_create failed\n", __func__);
		ret = -1;
		goto exit_err3;
	}

	pr_info("%s: register dtd cdev OK\n", __func__);
	kfree(dtb_buf);
	dtb_buf = NULL;

	return ret;

exit_err3:
	class_remove_file(amlnf_dtb_class, &class_attr_nfdtb);
	class_destroy(amlnf_dtb_class);
exit_err2:
	cdev_del(&amlnf_dtb);
exit_err1:
	unregister_chrdev_region(amlnf_dtb_no, 1);

#endif /* AML_NAND_UBOOT */
exit_err:
	kfree(dtb_buf);
	dtb_buf = NULL;
	return ret;
}

#ifdef AML_NAND_UBOOT
int amlnf_dtb_init_partitions(struct aml_nand_chip *aml_chip)
{
	int ret = 0;
	u8 *dtb_buf = NULL;

	aml_chip_dtb = aml_chip;

	dtb_buf = kzalloc(aml_chip_dtb->dtbsize, GFP_KERNEL);
	if (dtb_buf == NULL) {
		ret = -1;
		goto exit_err;
	}
	memset(dtb_buf, 0x0, aml_chip_dtb->dtbsize);
	/*parse partitions table */
	ret = get_partition_from_dts(dtb_buf);
	if (ret)
		pr_info("%s get_partition_from_dts failed\n",  __func__);

exit_err:
	kfree(dtb_buf);
	dtb_buf = NULL;

	return ret;
}


/*****************************************************************************
 * Prototype    : amlnf_detect_dtb_partitions
 *Description  : if 'dtb, write the bad block, we can't erase this block.
 *		So we have to find the 'dtb' address in flash and flag it.
 *Input        : struct amlnand_chip *aml_chip
 *Output       : NULL
 *Return Value :	ret
 *Called By    : amlnand_get_partition_table

 *History        :
 *1.Date         : 2015/10/15
 *	Author       : Fly Mo
 *	Modification : Created function
 *****************************************************************************/
int amlnf_detect_dtb_partitions(struct aml_nand_chip *aml_chip)
{
	return 0;
}

/* for blank positions... */
int aml_nand_update_dtb(struct aml_nand_chip *aml_chip, char *dtb_ptr)
{
	return 0;
}
#endif


