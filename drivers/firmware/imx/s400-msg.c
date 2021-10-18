// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 NXP
 * Author: Alice Guo <alice.guo@nxp.com>
 */

#include <linux/types.h>

#include <linux/firmware/imx/s400-api.h>
#include <linux/firmware/imx/s4_muap_ioctl.h>

#include "s4_muap.h"

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
	    size == 0x07 && ver == S400_VERSION && status == S400_SUCCESS_IND) {
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

static int s4_auth_cntr_hdr(struct imx_s400_api *s400_api, void *value)
{
	int ret;

	ret = s400_api_send_command(s400_api);
	if (ret < 0)
		return ret;

	return ret;
}

static int s4_verify_img(struct imx_s400_api *s400_api, void *value)
{
	int ret;

	ret = s400_api_send_command(s400_api);
	if (ret < 0)
		return ret;

	return ret;
}

static int s4_release_cntr(struct imx_s400_api *s400_api, void *value)
{
	int ret;

	ret = s400_api_send_command(s400_api);
	if (ret < 0)
		return ret;

	return ret;
}

int imx_s400_api_call(struct imx_s400_api *s400_api, void *value)
{
	unsigned int tag, command, ver;
	int err;

	err = -EINVAL;
	tag = MSG_TAG(s400_api->tx_msg.header);
	command = MSG_COMMAND(s400_api->tx_msg.header);
	ver = MSG_VER(s400_api->tx_msg.header);

	if (tag == s400_api->cmd_tag && ver == S400_VERSION) {
		reinit_completion(&s400_api->done);

		switch (command) {
		case S400_READ_FUSE_REQ:
			err = read_common_fuse(s400_api, value);
			break;
		case S400_OEM_CNTN_AUTH_REQ:
			err = s4_auth_cntr_hdr(s400_api, value);
			break;
		case S400_VERIFY_IMAGE_REQ:
			err = s4_verify_img(s400_api, value);
			break;
		case S400_RELEASE_CONTAINER_REQ:
			err = s4_release_cntr(s400_api, value);
			break;
		default:
			return -EINVAL;
		}
	}

	return err;
}
EXPORT_SYMBOL_GPL(imx_s400_api_call);

#if 0
static int s4_muap_ioctl_img_auth_cmd_handler(struct s4_mu_device_ctx *dev_ctx,
					      unsigned long arg)
{
	struct imx_s400_api *s400_muap_priv = dev_ctx->s400_muap_priv;
	struct s4_muap_auth_image s4_muap_auth_image = {0};
	struct container_hdr *phdr = &s4_muap_auth_image.chdr;
	struct image_info *img = &s4_muap_auth_image.img_info[0];
	unsigned long base_addr = (unsigned long) &s4_muap_auth_image;

	int i;
	u16 length;
	unsigned long s, e;
	int ret = -EINVAL;

	/* Check if not already configured. */
	if (dev_ctx->secure_mem.dma_addr != 0u) {
		devctx_err(dev_ctx, "Shared memory not configured\n");
		goto exit;
	}

	ret = (int)copy_from_user(&s4_muap_auth_image, (u8 *)arg,
			sizeof(s4_muap_auth_image));
	if (ret) {
		devctx_err(dev_ctx, "Fail copy shared memory config to user\n");
		ret = -EFAULT;
		goto exit;
	}


	if (!IS_ALIGNED(base_addr, 4)) {
		devctx_err(dev_ctx, "Error: Image's address is not 4 byte aligned\n");
		return -EINVAL;
	}

	if (phdr->tag != 0x87 && phdr->version != 0x0) {
		devctx_err(dev_ctx, "Error: Wrong container header\n");
		return -EFAULT;
	}

	if (!phdr->num_images) {
		devctx_err(dev_ctx, "Error: Wrong container, no image found\n");
		return -EFAULT;
	}
	length = phdr->length_lsb + (phdr->length_msb << 8);

	devctx_dbg(dev_ctx, "container length %u\n", length);

	s400_muap_priv->tx_msg.header = (s400_muap_priv->cmd_tag << 24) |
					(S400_OEM_CNTN_AUTH_REQ << 16) |
					(S400_OEM_CNTN_AUTH_REQ_SIZE << 8) |
					S400_VERSION;
	s400_muap_priv->tx_msg.data[0] = ((u32)(((base_addr) >> 16) >> 16));
	s400_muap_priv->tx_msg.data[1] = ((u32)(base_addr));

	ret = imx_s400_api_call(s400_muap_priv, (void *) &s4_muap_auth_image.resp);
	if (ret || (s4_muap_auth_image.resp != S400_SUCCESS_IND)) {
		devctx_err(dev_ctx, "Error: Container Authentication failed.\n");
		ret = -EIO;
		goto exit;
	}

	/* Copy images to dest address */
	for (i = 0; i < phdr->num_images; i++) {
		img = img + i;

		//devctx_dbg(dev_ctx, "img %d, dst 0x%x, src 0x%lux, size 0x%x\n",
		//		i, (u32) img->dst,
		//		(unsigned long)img->offset + phdr, img->size);

		memcpy((void *)img->dst, (const void *)(img->offset + phdr),
				img->size);

		s = img->dst & ~(CACHELINE_SIZE - 1);
		e = ALIGN(img->dst + img->size, CACHELINE_SIZE) - 1;

#ifdef CONFIG_ARM64
		__flush_dcache_area((void *) s, e);
#else
		__cpuc_flush_dcache_area((void *) s, e);
#endif
		s400_muap_priv->tx_msg.header = (s400_muap_priv->cmd_tag << 24) |
						(S400_VERIFY_IMAGE_REQ << 16) |
						(S400_VERIFY_IMAGE_REQ_SIZE << 8) |
						S400_VERSION;
		s400_muap_priv->tx_msg.data[0] = 1 << i;
		ret = imx_s400_api_call(s400_muap_priv, (void *) &s4_muap_auth_image.resp);
		if (ret || (s4_muap_auth_image.resp != S400_SUCCESS_IND)) {
			devctx_err(dev_ctx, "Error: Image Verification failed.\n");
			ret = -EIO;
			goto exit;
		}
	}

exit:
	s400_muap_priv->tx_msg.header = (s400_muap_priv->cmd_tag << 24) |
					(S400_RELEASE_CONTAINER_REQ << 16) |
					(S400_RELEASE_CONTAINER_REQ_SIZE << 8) |
					S400_VERSION;
	ret = imx_s400_api_call(s400_muap_priv, (void *) &s4_muap_auth_image.resp);
	if (ret || (s4_muap_auth_image.resp != S400_SUCCESS_IND)) {
		devctx_err(dev_ctx, "Error: Release Container failed.\n");
		ret = -EIO;
	}

	ret = (int)copy_to_user((u8 *)arg, &s4_muap_auth_image,
		sizeof(s4_muap_auth_image));
	if (ret) {
		devctx_err(dev_ctx, "Failed to copy iobuff setup to user\n");
		ret = -EFAULT;
	}
	return ret;
}
#endif

