// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 NXP
 * Author: Alice Guo <alice.guo@nxp.com>
 */

#include <linux/completion.h>
#include <linux/dev_printk.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/firmware/imx/s400-api.h>
#include <linux/init.h>
#include <linux/mailbox_client.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

/* macro to log operation of a misc device */
#define miscdev_dbg(p_miscdev, fmt, va_args...)                                \
	({                                                                     \
		struct miscdevice *_p_miscdev = p_miscdev;                     \
		dev_dbg((_p_miscdev)->parent, "%s: " fmt, (_p_miscdev)->name,  \
		##va_args);                                                    \
	})

#define miscdev_info(p_miscdev, fmt, va_args...)                               \
	({                                                                     \
		struct miscdevice *_p_miscdev = p_miscdev;                     \
		dev_info((_p_miscdev)->parent, "%s: " fmt, (_p_miscdev)->name, \
		##va_args);                                                    \
	})

#define miscdev_err(p_miscdev, fmt, va_args...)                                \
	({                                                                     \
		struct miscdevice *_p_miscdev = p_miscdev;                     \
		dev_err((_p_miscdev)->parent, "%s: " fmt, (_p_miscdev)->name,  \
		##va_args);                                                    \
	})
/* macro to log operation of a device context */
#define devctx_dbg(p_devctx, fmt, va_args...) \
	miscdev_dbg(&((p_devctx)->miscdev), fmt, ##va_args)
#define devctx_info(p_devctx, fmt, va_args...) \
	miscdev_info(&((p_devctx)->miscdev), fmt, ##va_args)
#define devctx_err(p_devctx, fmt, va_args...) \
	miscdev_err((&(p_devctx)->miscdev), fmt, ##va_args)

#define MSG_TAG(x)			(((x) & 0xff000000) >> 24)
#define MSG_COMMAND(x)			(((x) & 0x00ff0000) >> 16)
#define MSG_SIZE(x)			(((x) & 0x0000ff00) >> 8)
#define MSG_VER(x)			((x) & 0x000000ff)
#define RES_STATUS(x)			((x) & 0x000000ff)
#define MAX_DATA_SIZE_PER_USER		(65 * 1024)
#define S4_DEFAULT_MUAP_INDEX		(0)
#define S4_MUAP_DEFAULT_MAX_USERS	(4)

#define DEFAULT_MESSAGING_TAG_COMMAND           (0x17u)
#define DEFAULT_MESSAGING_TAG_RESPONSE          (0xe1u)

struct imx_s400_api *s400_api_export;

int get_imx_s400_api(struct imx_s400_api **export)
{
	if (!s400_api_export)
		return -EPROBE_DEFER;

	*export = s400_api_export;
	return 0;
}
EXPORT_SYMBOL_GPL(get_imx_s400_api);

static int s400_api_send_command(struct imx_s400_api *s400_api)
{
	int err;

	err = mbox_send_message(s400_api->tx_chan, &s400_api->tx_msg);
	if (err < 0)
		return err;

	return 0;
}

static int read_otp_uniq_id(struct imx_s400_api *s400_api, u32 *value)
{
	unsigned int tag, command, size, ver, status;

	tag = MSG_TAG(s400_api->rx_msg.header);
	command = MSG_COMMAND(s400_api->rx_msg.header);
	size = MSG_SIZE(s400_api->rx_msg.header);
	ver = MSG_VER(s400_api->rx_msg.header);
	status = RES_STATUS(s400_api->rx_msg.data[0]);

	if (tag == 0xe1 && command == S400_READ_FUSE_REQ &&
	    size == 0x07 && ver == 0x06 && status == S400_SUCCESS_IND) {
		value[0] = s400_api->rx_msg.data[1];
		value[1] = s400_api->rx_msg.data[2];
		value[2] = s400_api->rx_msg.data[3];
		value[3] = s400_api->rx_msg.data[4];
		return 0;
	}

	return -EINVAL;
}

static int read_fuse_word(struct imx_s400_api *s400_api, u32 *value)
{
	unsigned int tag, command, size, ver, status;

	tag = MSG_TAG(s400_api->rx_msg.header);
	command = MSG_COMMAND(s400_api->rx_msg.header);
	size = MSG_SIZE(s400_api->rx_msg.header);
	ver = MSG_VER(s400_api->rx_msg.header);
	status = RES_STATUS(s400_api->rx_msg.data[0]);

	if (tag == 0xe1 && command == S400_READ_FUSE_REQ &&
	    size == 0x03 && ver == 0x06 && status == S400_SUCCESS_IND) {
		value[0] = s400_api->rx_msg.data[1];
		return 0;
	}

	return -EINVAL;
}

static int read_common_fuse(struct imx_s400_api *s400_api, u32 *value)
{
	unsigned int size = MSG_SIZE(s400_api->tx_msg.header);
	unsigned int wait, fuse_id;
	int err;

	err = -EINVAL;
	if (size == 2) {
		err = s400_api_send_command(s400_api);
		if (err < 0)
			return err;

		wait = msecs_to_jiffies(1000);
		if (!wait_for_completion_timeout(&s400_api->done, wait))
			return -ETIMEDOUT;

		fuse_id = s400_api->tx_msg.data[0] & 0xff;
		switch (fuse_id) {
		case OTP_UNIQ_ID:
			err = read_otp_uniq_id(s400_api, value);
			break;
		default:
			err = read_fuse_word(s400_api, value);
			break;
		}
	}

	return err;
}

int imx_s400_api_call(struct imx_s400_api *s400_api, void *value)
{
	unsigned int tag, command, ver;
	int err;

	err = -EINVAL;
	tag = MSG_TAG(s400_api->tx_msg.header);
	command = MSG_COMMAND(s400_api->tx_msg.header);
	ver = MSG_VER(s400_api->tx_msg.header);

	if (tag == 0x17 && ver == 0x06) {
		reinit_completion(&s400_api->done);

		switch (command) {
		case S400_READ_FUSE_REQ:
			err = read_common_fuse(s400_api, value);
			break;
		default:
			return -EINVAL;
		}
	}

	return err;
}
EXPORT_SYMBOL_GPL(imx_s400_api_call);

static void s400_api_receive_message(struct mbox_client *rx_cl, void *mssg)
{
	struct imx_s400_api *s400_api;

	s400_api = container_of(rx_cl, struct imx_s400_api, rx_cl);

	spin_lock(&s400_api->lock);
	if (mssg) {
		s400_api->rx_msg = *(struct s400_api_msg *)mssg;
		complete(&s400_api->done);
	}
	spin_unlock(&s400_api->lock);
}

struct device *imx_soc_device_register(void)
{
	struct soc_device_attribute *attr;
	struct soc_device *dev;
	u32 v[4];
	int err;

	s400_api_export->tx_msg.header = 0x17970206;
	s400_api_export->tx_msg.data[0] = 0x1;
	err = imx_s400_api_call(s400_api_export, v);
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

/* Char driver setup */
static const struct file_operations s4_muap_fops = {
	.open		= NULL,
	.owner		= THIS_MODULE,
	.release	= NULL,
	.unlocked_ioctl = NULL,
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

static int s4_mu_request_channel(struct device *dev,
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

static int s400_api_probe(struct platform_device *pdev)
{
	struct s4_mu_device_ctx *dev_ctx;
	struct device *dev = &pdev->dev;
	struct imx_s400_api *priv;
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

	ret = of_property_read_u32(np, "fsl,s4_muap_id", &priv->s4_muap_id);
	if (ret) {
		dev_warn(dev, "%s: Not able to read mu_id", __func__);
		priv->s4_muap_id = S4_DEFAULT_MUAP_INDEX;
	}

	ret = of_property_read_u32(np, "fsl,fsl,s4muap_max_users", &max_nb_users);
	if (ret) {
		dev_warn(dev, "%s: Not able to read mu_max_user", __func__);
		max_nb_users = S4_MUAP_DEFAULT_MAX_USERS;
	}

	ret = of_property_read_u8(np, "fsl,cmd_tag", &priv->cmd_tag);
	if (ret)
		priv->cmd_tag = DEFAULT_MESSAGING_TAG_COMMAND;

	ret = of_property_read_u8(np, "fsl,rsp_tag", &priv->rsp_tag);
	if (ret)
		priv->rsp_tag = DEFAULT_MESSAGING_TAG_RESPONSE;

	/* Mailbox client configuration */
	priv->tx_cl.dev			= dev;
	priv->tx_cl.tx_block		= false;
	priv->tx_cl.knows_txdone	= true;
	priv->tx_cl.rx_callback		= s400_api_receive_message;

	priv->rx_cl.dev			= dev;
	priv->rx_cl.tx_block		= false;
	priv->rx_cl.knows_txdone	= true;
	priv->rx_cl.rx_callback		= s400_api_receive_message;

	ret = s4_mu_request_channel(dev, &priv->tx_chan, &priv->tx_cl, "tx");
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request tx channel\n");

		goto exit;
	}

	ret = s4_mu_request_channel(dev, &priv->rx_chan, &priv->rx_cl, "rx");
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
		dev_ctx->s400_muap_priv = priv;
		/* Default value invalid for an header. */
		init_waitqueue_head(&dev_ctx->wq);

		INIT_LIST_HEAD(&dev_ctx->pending_out);
		sema_init(&dev_ctx->fops_lock, 1);

		devname = devm_kasprintf(dev, GFP_KERNEL, "s4muap%d_ch%d",
					 priv->s4_muap_id, i);
		if (!devname) {
			ret = -ENOMEM;
			dev_err(dev,
				"Fail to allocate memory for misc dev name\n");
			goto exit;
		}

		dev_ctx->miscdev.name = devname;
		dev_ctx->miscdev.minor = MISC_DYNAMIC_MINOR;
		dev_ctx->miscdev.fops = &s4_muap_fops;
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

	s400_api_export = priv;

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

static int s400_api_remove(struct platform_device *pdev)
{
	struct imx_s400_api *s400_api;

	s400_api = dev_get_drvdata(&pdev->dev);
	mbox_free_channel(s400_api->tx_chan);
	mbox_free_channel(s400_api->rx_chan);

	return 0;
}

static const struct of_device_id s400_api_match[] = {
	{ .compatible = "fsl,imx8ulp-s400", },
	{},
};

static struct platform_driver s400_api_driver = {
	.driver = {
		.name = "fsl-s400-api",
		.of_match_table = s400_api_match,
	},
	.probe = s400_api_probe,
	.remove = s400_api_remove,
};
module_platform_driver(s400_api_driver);

MODULE_AUTHOR("Alice Guo <alice.guo@nxp.com>");
MODULE_DESCRIPTION("S400 Baseline API");
MODULE_LICENSE("GPL v2");
