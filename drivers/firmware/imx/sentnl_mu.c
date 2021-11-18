// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 NXP
 * Author: Alice Guo <alice.guo@nxp.com>
 * Author: Pankaj Gupta <pankaj.gupta@nxp.com>
 */

#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/dev_printk.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/firmware/imx/senclave_base_msg.h>
#include <linux/firmware/imx/sentnl_mu_ioctl.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/mailbox_client.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

#include "sentnl_mu.h"

struct sentnl_mu_priv *sentnl_priv_export;

int get_sentnl_mu_priv(struct sentnl_mu_priv **export)
{
	if (!sentnl_priv_export)
		return -EPROBE_DEFER;

	*export = sentnl_priv_export;
	return 0;
}
EXPORT_SYMBOL_GPL(get_sentnl_mu_priv);


static void sentnl_receive_message(struct mbox_client *rx_cl, void *mssg)
{
	struct sentnl_mu_priv *priv;

	priv = container_of(rx_cl, struct sentnl_mu_priv, rx_cl);

	spin_lock(&priv->lock);
	if (mssg) {
		priv->rx_msg = *(struct sentnl_api_msg *)mssg;
		complete(&priv->done);
	}
	spin_unlock(&priv->lock);
}

struct device *imx_soc_device_register(void)
{
	struct soc_device_attribute *attr;
	struct soc_device *dev;
	u32 v[4];
	int err;

	err = read_common_fuse(OTP_UNIQ_ID, v);
	if (err)
		return NULL;

	attr = kzalloc(sizeof(*attr), GFP_KERNEL);
	if (!attr)
		return NULL;

	err = of_property_read_string(of_root, "model", &attr->machine);
	if (err) {
		kfree(attr);
		return NULL;
	}
	attr->family = kasprintf(GFP_KERNEL, "Freescale i.MX");
	attr->revision = kasprintf(GFP_KERNEL, "1.0");
	attr->serial_number = kasprintf(GFP_KERNEL, "%016llX", (u64)v[3] << 32 | v[0]);
	attr->soc_id = kasprintf(GFP_KERNEL, "i.MX8ULP");

	dev = soc_device_register(attr);
	if (IS_ERR(dev)) {
		kfree(attr->soc_id);
		kfree(attr->serial_number);
		kfree(attr->revision);
		kfree(attr->family);
		kfree(attr->machine);
		kfree(attr);
		return ERR_CAST(dev);
	}

	return soc_device_to_device(dev);
}

/*
 * File operations for user-space
 */

/* Write a message to the MU. */
static ssize_t sentnl_mu_fops_write(struct file *fp, const char __user *buf,
				  size_t size, loff_t *ppos)
{
	int ret = 0;

	return ret;
}

/*
 * Read a message from the MU.
 * Blocking until a message is available.
 */
static ssize_t sentnl_mu_fops_read(struct file *fp, char __user *buf,
				 size_t size, loff_t *ppos)
{
	int ret = 0;

	return ret;
}

/* Give access to S40x to the memory we want to share */
static int sentnl_mu_setup_sentnl_mem_access(struct sentnl_mu_device_ctx *dev_ctx,
					     u64 addr, u32 len)
{
	/* Assuming S400 has access to all the memory regions */
	int ret = 0;

	if (ret) {
		devctx_err(dev_ctx, "Fail find memreg\n");
		goto exit;
	}

	if (ret) {
		devctx_err(dev_ctx, "Fail set permission for resource\n");
		goto exit;
	}

exit:
	return ret;
}

/* Open a char device. */
static int sentnl_mu_fops_open(struct inode *nd, struct file *fp)
{
	struct sentnl_mu_device_ctx *dev_ctx = container_of(fp->private_data,
			struct sentnl_mu_device_ctx, miscdev);
	int err;

	/* Avoid race if opened at the same time */
	if (down_trylock(&dev_ctx->fops_lock))
		return -EBUSY;

	/* Authorize only 1 instance. */
	if (dev_ctx->status != MU_FREE) {
		err = -EBUSY;
		goto exit;
	}

	/*
	 * Allocate some memory for data exchanges with S40x.
	 * This will be used for data not requiring secure memory.
	 */
	dev_ctx->non_secure_mem.ptr = dmam_alloc_coherent(dev_ctx->dev,
					MAX_DATA_SIZE_PER_USER,
					&dev_ctx->non_secure_mem.dma_addr,
					GFP_KERNEL);
	if (!dev_ctx->non_secure_mem.ptr) {
		err = -ENOMEM;
		devctx_err(dev_ctx, "Failed to map shared memory with S40x\n");
		goto exit;
	}

	err = sentnl_mu_setup_sentnl_mem_access(dev_ctx,
						dev_ctx->non_secure_mem.dma_addr,
						MAX_DATA_SIZE_PER_USER);
	if (err) {
		err = -EPERM;
		devctx_err(dev_ctx,
			   "Failed to share access to shared memory\n");
		goto free_coherent;
	}

	dev_ctx->non_secure_mem.size = MAX_DATA_SIZE_PER_USER;
	dev_ctx->non_secure_mem.pos = 0;
	dev_ctx->status = MU_OPENED;

	dev_ctx->pending_hdr = 0;

	goto exit;

free_coherent:
	dmam_free_coherent(dev_ctx->priv->dev, MAX_DATA_SIZE_PER_USER,
			   dev_ctx->non_secure_mem.ptr,
			   dev_ctx->non_secure_mem.dma_addr);

exit:
	up(&dev_ctx->fops_lock);
	return err;
}

/* Close a char device. */
static int sentnl_mu_fops_close(struct inode *nd, struct file *fp)
{
	struct sentnl_mu_device_ctx *dev_ctx = container_of(fp->private_data,
					struct sentnl_mu_device_ctx, miscdev);
	struct sentnl_mu_priv *priv = dev_ctx->priv;
	struct sentnl_obuf_desc *out_buf_desc;

	/* Avoid race if closed at the same time */
	if (down_trylock(&dev_ctx->fops_lock))
		return -EBUSY;

	/* The device context has not been opened */
	if (dev_ctx->status != MU_OPENED)
		goto exit;

	/* check if this device was registered as command receiver. */
	if (priv->cmd_receiver_dev == dev_ctx)
		priv->cmd_receiver_dev = NULL;

	/* check if this device was registered as waiting response. */
	if (priv->waiting_rsp_dev == dev_ctx) {
		priv->waiting_rsp_dev = NULL;
		mutex_unlock(&priv->mu_cmd_lock);
	}

	/* Unmap secure memory shared buffer. */
	if (dev_ctx->secure_mem.ptr)
		devm_iounmap(dev_ctx->dev, dev_ctx->secure_mem.ptr);

	dev_ctx->secure_mem.ptr = NULL;
	dev_ctx->secure_mem.dma_addr = 0;
	dev_ctx->secure_mem.size = 0;
	dev_ctx->secure_mem.pos = 0;

	/* Free non-secure shared buffer. */
	dmam_free_coherent(dev_ctx->priv->dev, MAX_DATA_SIZE_PER_USER,
			   dev_ctx->non_secure_mem.ptr,
			   dev_ctx->non_secure_mem.dma_addr);

	dev_ctx->non_secure_mem.ptr = NULL;
	dev_ctx->non_secure_mem.dma_addr = 0;
	dev_ctx->non_secure_mem.size = 0;
	dev_ctx->non_secure_mem.pos = 0;

	while (!list_empty(&dev_ctx->pending_out)) {
		out_buf_desc = list_first_entry_or_null(&dev_ctx->pending_out,
						struct sentnl_obuf_desc,
						link);
		__list_del_entry(&out_buf_desc->link);
		devm_kfree(dev_ctx->dev, out_buf_desc);
	}

	dev_ctx->status = MU_FREE;

exit:
	up(&dev_ctx->fops_lock);
	return 0;
}

/* IOCTL entry point of a char device */
static long sentnl_mu_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct sentnl_mu_device_ctx *dev_ctx = container_of(fp->private_data,
					struct sentnl_mu_device_ctx, miscdev);
	int err = -EINVAL;

	/* Prevent race during change of device context */
	if (down_interruptible(&dev_ctx->fops_lock))
		return -EBUSY;

	switch (cmd) {
	case SENTNL_MU_IOCTL_SIGNED_MSG_CMD:
		devctx_err(dev_ctx, "IOCTL %.8x not supported\n", cmd);
		break;
	default:
		err = -EINVAL;
		devctx_dbg(dev_ctx, "IOCTL %.8x not supported\n", cmd);
	}

	up(&dev_ctx->fops_lock);
	return (long)err;
}

/* Char driver setup */
static const struct file_operations sentnl_mu_fops = {
	.open		= sentnl_mu_fops_open,
	.owner		= THIS_MODULE,
	.release	= sentnl_mu_fops_close,
	.unlocked_ioctl = sentnl_mu_ioctl,
	.read		= sentnl_mu_fops_read,
	.write		= sentnl_mu_fops_write,
};

/* interface for managed res to free a mailbox channel */
static void if_mbox_free_channel(void *mbox_chan)
{
	mbox_free_channel(mbox_chan);
}

/* interface for managed res to unregister a char device */
static void if_misc_deregister(void *miscdevice)
{
	misc_deregister(miscdevice);
}

static int sentnl_mu_request_channel(struct device *dev,
				 struct mbox_chan **chan,
				 struct mbox_client *cl,
				 const char *name)
{
	struct mbox_chan *t_chan;
	int ret = 0;

	t_chan = mbox_request_channel_byname(cl, name);
	if (IS_ERR(t_chan)) {
		ret = PTR_ERR(t_chan);
		if (ret != -EPROBE_DEFER)
			dev_err(dev,
				"Failed to request chan %s ret %d\n", name,
				ret);
		goto exit;
	}

	ret = devm_add_action(dev, if_mbox_free_channel, t_chan);
	if (ret) {
		dev_err(dev, "failed to add devm removal of mbox %s\n", name);
		goto exit;
	}

	*chan = t_chan;

exit:
	return ret;
}

static int sentnl_probe(struct platform_device *pdev)
{
	struct sentnl_mu_device_ctx *dev_ctx;
	struct device *dev = &pdev->dev;
	struct sentnl_mu_priv *priv;
	struct device_node *np;
	int max_nb_users = 0;
	char *devname;
	struct device *soc;
	int ret;
	int i;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		dev_err(dev, "Fail allocate mem for private data\n");
		goto exit;
	}
	priv->dev = dev;
	dev_set_drvdata(dev, priv);

	/*
	 * Get the address of MU to be used for communication with the SCU
	 */
	np = pdev->dev.of_node;
	if (!np) {
		dev_err(dev, "Cannot find MU User entry in device tree\n");
		ret = -ENOTSUPP;
		goto exit;
	}

	/* Initialize the mutex. */
	mutex_init(&priv->mu_cmd_lock);
	mutex_init(&priv->mu_lock);

	/* TBD */
	priv->cmd_receiver_dev = NULL;
	priv->waiting_rsp_dev = NULL;

	ret = of_property_read_u32(np, "fsl,sentnl_mu_id", &priv->sentnl_mu_id);
	if (ret) {
		dev_warn(dev, "%s: Not able to read mu_id", __func__);
		priv->sentnl_mu_id = S4_DEFAULT_MUAP_INDEX;
	}

	ret = of_property_read_u32(np, "fsl,sentnl_mu_max_users", &max_nb_users);
	if (ret) {
		dev_warn(dev, "%s: Not able to read mu_max_user", __func__);
		max_nb_users = S4_MUAP_DEFAULT_MAX_USERS;
	}

	ret = of_property_read_u8(np, "fsl,cmd_tag", &priv->cmd_tag);
	if (ret) {
		dev_warn(dev, "%s: Not able to read cmd_tag", __func__);
		priv->cmd_tag = DEFAULT_MESSAGING_TAG_COMMAND;
	}

	ret = of_property_read_u8(np, "fsl,rsp_tag", &priv->rsp_tag);
	if (ret) {
		dev_warn(dev, "%s: Not able to read rsp_tag", __func__);
		priv->rsp_tag = DEFAULT_MESSAGING_TAG_RESPONSE;
	}

	/* Mailbox client configuration */
	priv->tx_cl.dev			= dev;
	priv->tx_cl.tx_block		= false;
	priv->tx_cl.knows_txdone	= true;
	priv->tx_cl.rx_callback		= sentnl_receive_message;

	priv->rx_cl.dev			= dev;
	priv->rx_cl.tx_block		= false;
	priv->rx_cl.knows_txdone	= true;
	priv->rx_cl.rx_callback		= sentnl_receive_message;

	ret = sentnl_mu_request_channel(dev, &priv->tx_chan, &priv->tx_cl, "tx");
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request tx channel\n");

		goto exit;
	}

	ret = sentnl_mu_request_channel(dev, &priv->rx_chan, &priv->rx_cl, "rx");
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request rx channel\n");

		goto exit;
	}

	/* Create users */
	for (i = 0; i < max_nb_users; i++) {
		dev_ctx = devm_kzalloc(dev, sizeof(*dev_ctx), GFP_KERNEL);
		if (!dev_ctx) {
			ret = -ENOMEM;
			dev_err(dev,
				"Fail to allocate memory for device context\n");
			goto exit;
		}

		dev_ctx->dev = dev;
		dev_ctx->status = MU_FREE;
		dev_ctx->priv = priv;
		/* Default value invalid for an header. */
		init_waitqueue_head(&dev_ctx->wq);

		INIT_LIST_HEAD(&dev_ctx->pending_out);
		sema_init(&dev_ctx->fops_lock, 1);

		devname = devm_kasprintf(dev, GFP_KERNEL, "sentnl_mu%d_ch%d",
					 priv->sentnl_mu_id, i);
		if (!devname) {
			ret = -ENOMEM;
			dev_err(dev,
				"Fail to allocate memory for misc dev name\n");
			goto exit;
		}

		dev_ctx->miscdev.name = devname;
		dev_ctx->miscdev.minor = MISC_DYNAMIC_MINOR;
		dev_ctx->miscdev.fops = &sentnl_mu_fops;
		dev_ctx->miscdev.parent = dev;
		ret = misc_register(&dev_ctx->miscdev);
		if (ret) {
			dev_err(dev, "failed to register misc device %d\n",
				ret);
			goto exit;
		}

		ret = devm_add_action(dev, if_misc_deregister,
				      &dev_ctx->miscdev);

	}

	init_completion(&priv->done);
	spin_lock_init(&priv->lock);

	sentnl_priv_export = priv;

	soc = imx_soc_device_register();
	if (IS_ERR(soc)) {
		pr_err("failed to register SoC device: %ld\n", PTR_ERR(soc));
		return PTR_ERR(soc);
	}

	dev_set_drvdata(dev, priv);
	return devm_of_platform_populate(dev);

exit:
	return ret;
}

static int sentnl_remove(struct platform_device *pdev)
{
	struct sentnl_mu_priv *priv;

	priv = dev_get_drvdata(&pdev->dev);
	mbox_free_channel(priv->tx_chan);
	mbox_free_channel(priv->rx_chan);

	return 0;
}

static const struct of_device_id sentnl_match[] = {
	{ .compatible = "fsl,imx-sentnl", },
	{},
};

static struct platform_driver sentnl_driver = {
	.driver = {
		.name = "fsl-sentnl-mu",
		.of_match_table = sentnl_match,
	},
	.probe = sentnl_probe,
	.remove = sentnl_remove,
};
module_platform_driver(sentnl_driver);

MODULE_AUTHOR("Pankaj Gupta <pankaj.gupta@nxp.com>");
MODULE_DESCRIPTION("iMX Secure Enclave MU Driver.");
MODULE_LICENSE("GPL v2");
