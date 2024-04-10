// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 NXP
 *  Author: Robert Chiras <robert.chiras@nxp.com>
 *
 * Implementation of the CAMERAMIX ISP FW comms using MUs (client side).
 *
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/firmware/imx/isp-mu.h>

static DEFINE_MUTEX(cl_mutex);

static struct isp_mu_ipc *isp_ipc_handle;

static void imx_ispmu_rx_callback(struct mbox_client *c, void *msg)
{
	struct isp_mu_chan *chan = container_of(c, struct isp_mu_chan, cl);
	struct isp_mu_dev *mu = chan->mu_dev;

	if (!mu->cl)
		return;

	/* This is an unsolicited message, just redirect it*/
	if (!mu->need_reply) {
		if (mu->cl->rx_callback)
			mu->cl->rx_callback(mu->cl, msg);
		return;
	}

	/* This is a reply to a message request, set the reply */
	memcpy((void *)(&mu->reply[0]), msg, sizeof(mu->reply));
	complete(&mu->done);
}

int imx_ispmu_client_register(struct isp_mu_client *cl)
{
	struct isp_mu_ipc *ipc = isp_ipc_handle;
	struct isp_mu_dev *mu;

	if (WARN_ON(!ipc))
		return -EPROBE_DEFER;

	if (cl->id > ipc->dev_num)
		return -ENODEV;

	if (!cl->rx_callback)
		return -EINVAL;

	mutex_lock(&cl_mutex);

	mu = ipc->devs[cl->id];

	if (mu->cl) {
		mutex_unlock(&cl_mutex);
		return -EBUSY;
	}

	mu->cl = cl;
	cl->mu = mu;

	mutex_unlock(&cl_mutex);

	return 0;
}
EXPORT_SYMBOL(imx_ispmu_client_register);

int imx_ispmu_client_unregister(struct isp_mu_client *cl)
{
	struct isp_mu_ipc *ipc = isp_ipc_handle;
	struct isp_mu_dev *mu;

	if (WARN_ON(!ipc))
		return -EPROBE_DEFER;

	if (cl->id > ipc->dev_num || !cl->dev)
		return -ENODEV;

	mutex_lock(&cl_mutex);

	mu = ipc->devs[cl->id];

	if (!mu->cl) {
		mutex_unlock(&cl_mutex);
		return -ENODEV;
	}

	if (mu->cl->dev != cl->dev) {
		mutex_unlock(&cl_mutex);
		return -EPERM;
	}

	mu->cl = NULL;
	cl->mu = NULL;

	mutex_unlock(&cl_mutex);

	return 0;
}
EXPORT_SYMBOL(imx_ispmu_client_unregister);

int imx_ispmu_send_msg(struct isp_mu_client *cl, void *msg, void **reply)
{
	struct isp_mu_dev *mu = cl->mu;
	struct isp_mu_ipc *ipc;
	struct isp_mu_chan *chan;
	int ret = 0;

	if (!mu)
		return -ENODEV;

	if (!msg)
		return -EINVAL;

	ipc = mu->ipc;

	mutex_lock(&mu->lock);
	reinit_completion(&mu->done);

	chan = &mu->chans[MU_CHAN_TX];

	mu->need_reply = false;
	if (reply) {
		mu->need_reply = true;
		*reply = &mu->reply[0];
	}

	ret = mbox_send_message(chan->ch, msg);

	/* Successful send means a return code greater than 0 */
	if (ret <= 0) {
		mutex_unlock(&mu->lock);
		return ret;
	}

	/* Wait for other end to send us a reply */
	if (mu->need_reply) {
		if (!wait_for_completion_timeout(&mu->done,
						 MAX_RX_TIMEOUT)) {
			dev_err(ipc->dev, "MSG sent, no response\n");
			mutex_unlock(&mu->lock);
			return -ETIMEDOUT;
		}
	}

	mutex_unlock(&mu->lock);

	return ret;
}
EXPORT_SYMBOL(imx_ispmu_send_msg);

static int imx_ispmu_init_mu_dev(struct isp_mu_ipc *ipc, int dev_num)
{
	struct device *dev = ipc->dev;
	struct isp_mu_dev *mu_dev;
	struct isp_mu_chan *mu_chan;
	struct mbox_client *cl;
	char *chan_name;
	int ret;
	int i;

	mu_dev = devm_kzalloc(dev, sizeof(*mu_dev), GFP_KERNEL);
	if (!mu_dev)
		return -ENOMEM;

	mu_dev->ipc = ipc;
	mu_dev->id = dev_num;

	for (i = 0; i < ISP_MU_CHAN_NUM; i++) {
		if (!i)
			chan_name = kasprintf(GFP_KERNEL, "tx%d", dev_num);
		else
			chan_name = kasprintf(GFP_KERNEL, "rx%d", dev_num);

		if (!chan_name)
			return -ENOMEM;

		mu_chan = &mu_dev->chans[i];
		cl = &mu_chan->cl;
		cl->dev = dev;
		cl->tx_block = false;
		cl->knows_txdone = false;
		cl->rx_callback = imx_ispmu_rx_callback;

		mu_chan->mu_dev = mu_dev;
		mu_chan->idx = i;
		mu_chan->ch = mbox_request_channel_byname(cl, chan_name);
		if (IS_ERR(mu_chan->ch)) {
			ret = PTR_ERR(mu_chan->ch);
			dev_err_probe(dev, ret,
				      "Failed to request mbox chan %s\n",
				      chan_name);
			kfree(chan_name);
			return ret;
		}

		dev_dbg(ipc->dev, "request mbox chan %s\n", chan_name);
		/* chan_name is not used anymore by framework */
		kfree(chan_name);
	}

	ipc->devs[dev_num] = mu_dev;
	ipc->dev_num++;

	mutex_init(&mu_dev->lock);
	init_completion(&mu_dev->done);

	return 0;
}

static int imx_ispmu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct isp_mu_ipc *isp_ipc;
	struct of_phandle_args args;
	int ret;
	int i;

	isp_ipc = devm_kzalloc(dev, sizeof(*isp_ipc), GFP_KERNEL);
	if (!isp_ipc)
		return -ENOMEM;

	isp_ipc->dev = dev;
	isp_ipc->dev_num = -1;
	dev_set_drvdata(dev, isp_ipc);

	for (i = 0; i < ISP_MU_DEV_NUM; i++) {
		ret = of_parse_phandle_with_args(pdev->dev.of_node, "mboxes",
						 "#mbox-cells", i, &args);
		if (ret)
			break;

		ret = imx_ispmu_init_mu_dev(isp_ipc, i);
		if (ret)
			return ret;
	}

	isp_ipc_handle = isp_ipc;

	pm_runtime_enable(dev);
	pm_runtime_resume_and_get(dev);

	dev_info(dev, "i.MX ISP-MU initialized with %d MUs\n",
		 isp_ipc->dev_num + 1);

	return devm_of_platform_populate(dev);
}

static int imx_ispmu_remove(struct platform_device *pdev)
{
	struct isp_mu_ipc *ipc = dev_get_drvdata(&pdev->dev);
	int i, j;

	for (i = 0; i < ipc->dev_num; i++) {
		struct isp_mu_dev *mu_dev;

		mu_dev = ipc->devs[i];
		for (j = 0; j < ISP_MU_CHAN_NUM; j++)
			mbox_free_channel(mu_dev->chans[j].ch);
	}

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id imx_ispmu_match[] = {
	{ .compatible = "nxp,imx-isp-mu", },
	{ /* Sentinel */ }
};

static struct platform_driver imx_ispmu_driver = {
	.driver = {
		.name = "imx-isp-mu",
		.of_match_table = imx_ispmu_match,
	},
	.probe = imx_ispmu_probe,
	.remove = imx_ispmu_remove,
};

static int __init imx_ispmu_driver_init(void)
{
	return platform_driver_register(&imx_ispmu_driver);
}
subsys_initcall_sync(imx_ispmu_driver_init);

MODULE_AUTHOR("Robert Chiras <robert.chiras@nxp.com>");
MODULE_DESCRIPTION("IMX ISP firmware MU protocol driver");
MODULE_LICENSE("GPL");
