// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/mailbox_controller.h>
#include <linux/of.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#define CMDQ_WRITE_ENABLE_MASK	BIT(0)
#define CMDQ_POLL_ENABLE_MASK	BIT(0)
#define CMDQ_EOC_IRQ_EN		BIT(0)
#define CMDQ_IMMEDIATE_VALUE	0
#define CMDQ_REG_TYPE		1
#define CMDQ_JUMP_RELATIVE	1

#define CMDQ_GET_ARG_B(arg)		(((arg) & GENMASK(31, 16)) >> 16)
#define CMDQ_GET_ARG_C(arg)		((arg) & GENMASK(15, 0))

#define CMDQ_OPERAND_GET_IDX_VALUE(operand) \
	((operand)->reg ? (operand)->idx : (operand)->value)
#define CMDQ_OPERAND_TYPE(operand) \
	((operand)->reg ? CMDQ_REG_TYPE : CMDQ_IMMEDIATE_VALUE)

struct cmdq_instruction {
	union {
		u32 value;
		u32 mask;
		struct {
			u16 arg_c;
			u16 arg_b;
		};
	};
	union {
		u16 offset;
		u16 event;
		u16 reg_dst;
		u16 arg_a;
	};
	union {
		u8 subsys;
		struct {
			u8 s_op:5;
			u8 arg_c_type:1;
			u8 arg_b_type:1;
			u8 arg_a_type:1;
		};
	};
	u8 op;
};

int cmdq_dev_get_client_reg(struct device *dev,
			    struct cmdq_client_reg *client_reg, int idx)
{
	struct of_phandle_args spec;
	int err;

	if (!client_reg)
		return -ENOENT;

	err = of_parse_phandle_with_fixed_args(dev->of_node,
					       "mediatek,gce-client-reg",
					       3, idx, &spec);
	if (err < 0) {
		dev_err(dev,
			"error %d can't parse gce-client-reg property (%d)",
			err, idx);

		return err;
	}

	client_reg->subsys = (u8)spec.args[0];
	client_reg->offset = (u16)spec.args[1];
	client_reg->size = (u16)spec.args[2];
	of_node_put(spec.np);

	return 0;
}
EXPORT_SYMBOL(cmdq_dev_get_client_reg);

struct cmdq_client *cmdq_mbox_create(struct device *dev, int index)
{
	struct cmdq_client *client;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return (struct cmdq_client *)-ENOMEM;

	client->client.dev = dev;
	client->client.tx_block = false;
	client->client.knows_txdone = true;
	client->chan = mbox_request_channel(&client->client, index);

	if (IS_ERR(client->chan)) {
		long err;

		dev_err(dev, "failed to request channel\n");
		err = PTR_ERR(client->chan);
		kfree(client);

		return ERR_PTR(err);
	}

	return client;
}
EXPORT_SYMBOL(cmdq_mbox_create);

void cmdq_mbox_destroy(struct cmdq_client *client)
{
	mbox_free_channel(client->chan);
	kfree(client);
}
EXPORT_SYMBOL(cmdq_mbox_destroy);

struct cmdq_pkt *cmdq_pkt_create(struct cmdq_client *client, size_t size)
{
	struct cmdq_pkt *pkt;
	struct device *dev;
	dma_addr_t dma_addr;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt)
		return ERR_PTR(-ENOMEM);
	pkt->va_base = kzalloc(size, GFP_KERNEL);
	if (!pkt->va_base) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->buf_size = size;
	pkt->cl = (void *)client;

	dev = client->chan->mbox->dev;
	dma_addr = dma_map_single(dev, pkt->va_base, pkt->buf_size,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(dev, dma_addr)) {
		dev_err(dev, "dma map failed, size=%u\n", (u32)(u64)size);
		kfree(pkt->va_base);
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}

	pkt->pa_base = dma_addr;

	return pkt;
}
EXPORT_SYMBOL(cmdq_pkt_create);

void cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	struct cmdq_client *client = (struct cmdq_client *)pkt->cl;

	dma_unmap_single(client->chan->mbox->dev, pkt->pa_base, pkt->buf_size,
			 DMA_TO_DEVICE);
	kfree(pkt->va_base);
	kfree(pkt);
}
EXPORT_SYMBOL(cmdq_pkt_destroy);

static int cmdq_pkt_append_command(struct cmdq_pkt *pkt,
				   struct cmdq_instruction inst)
{
	struct cmdq_instruction *cmd_ptr;

	if (unlikely(pkt->cmd_buf_size + CMDQ_INST_SIZE > pkt->buf_size)) {
		/*
		 * In the case of allocated buffer size (pkt->buf_size) is used
		 * up, the real required size (pkt->cmdq_buf_size) is still
		 * increased, so that the user knows how much memory should be
		 * ultimately allocated after appending all commands and
		 * flushing the command packet. Therefor, the user can call
		 * cmdq_pkt_create() again with the real required buffer size.
		 */
		pkt->cmd_buf_size += CMDQ_INST_SIZE;
		WARN_ONCE(1, "%s: buffer size %u is too small !\n",
			__func__, (u32)pkt->buf_size);
		return -ENOMEM;
	}

	cmd_ptr = pkt->va_base + pkt->cmd_buf_size;
	*cmd_ptr = inst;
	pkt->cmd_buf_size += CMDQ_INST_SIZE;

	return 0;
}

int cmdq_pkt_move(struct cmdq_pkt *pkt, u8 gpr_idx, u32 value)
{
	struct cmdq_instruction inst = { {0} };

	inst.op = CMDQ_CODE_MOVE;
	inst.value = value;
	inst.subsys = gpr_idx;
	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_move);

int cmdq_pkt_write(struct cmdq_pkt *pkt, u8 subsys, u16 offset, u32 value)
{
	struct cmdq_instruction inst;

	inst.op = CMDQ_CODE_WRITE;
	inst.value = value;
	inst.offset = offset;
	inst.subsys = subsys;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_write);

int cmdq_pkt_write_mask(struct cmdq_pkt *pkt, u8 subsys,
			u16 offset, u32 value, u32 mask)
{
	struct cmdq_instruction inst = { {0} };
	u16 offset_mask = offset;
	int err;

	if (mask != 0xffffffff) {
		inst.op = CMDQ_CODE_MASK;
		inst.mask = ~mask;
		err = cmdq_pkt_append_command(pkt, inst);
		if (err < 0)
			return err;

		offset_mask |= CMDQ_WRITE_ENABLE_MASK;
	}
	err = cmdq_pkt_write(pkt, subsys, offset_mask, value);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_write_mask);

int cmdq_pkt_read_s(struct cmdq_pkt *pkt, u16 high_addr_reg_idx, u16 addr_low,
		    u16 reg_idx)
{
	struct cmdq_instruction inst = {};

	inst.op = CMDQ_CODE_READ_S;
	inst.arg_a_type = CMDQ_REG_TYPE;
	inst.s_op = high_addr_reg_idx;
	inst.reg_dst = reg_idx;
	inst.arg_b = addr_low;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_read_s);

int cmdq_pkt_read_reg(struct cmdq_pkt *pkt, u8 subsys, u16 offset,
	u16 dst_reg_idx)
{
	struct cmdq_instruction inst = {};

	inst.arg_c = 0;
	inst.arg_b = offset;
	inst.arg_a = dst_reg_idx;
	inst.s_op = subsys;
	inst.arg_c_type = 0;
	inst.arg_b_type = 0;
	inst.arg_a_type = CMDQ_REG_TYPE;
	inst.op = CMDQ_CODE_READ_S;
	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_read_reg);

int cmdq_pkt_read_addr(struct cmdq_pkt *pkt, dma_addr_t addr, u16 dst_reg_idx)
{
	s32 err;
	const u16 src_reg_idx = CMDQ_SPR_FOR_TEMP;

	err = cmdq_pkt_assign(pkt, src_reg_idx, CMDQ_ADDR_HIGH(addr));
	if (err < 0)
		return err;

	return cmdq_pkt_read_s(pkt, src_reg_idx, CMDQ_ADDR_LOW(addr),
			       dst_reg_idx);
}
EXPORT_SYMBOL(cmdq_pkt_read_addr);

int cmdq_pkt_write_s(struct cmdq_pkt *pkt, u16 high_addr_reg_idx,
		     u16 addr_low, u16 src_reg_idx)
{
	struct cmdq_instruction inst = {};

	inst.op = CMDQ_CODE_WRITE_S;
	inst.arg_b_type = CMDQ_REG_TYPE;
	inst.s_op = high_addr_reg_idx;
	inst.offset = addr_low;
	inst.arg_b = src_reg_idx;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_write_s);

int cmdq_pkt_write_s_mask(struct cmdq_pkt *pkt, u16 high_addr_reg_idx,
			  u16 addr_low, u16 src_reg_idx, u32 mask)
{
	struct cmdq_instruction inst = {};
	int err;

	inst.op = CMDQ_CODE_MASK;
	inst.mask = ~mask;
	err = cmdq_pkt_append_command(pkt, inst);
	if (err < 0)
		return err;

	inst.mask = 0;
	inst.op = CMDQ_CODE_WRITE_S_MASK;
	inst.arg_b_type = CMDQ_REG_TYPE;
	inst.s_op = high_addr_reg_idx;
	inst.offset = addr_low;
	inst.arg_b = src_reg_idx;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_write_s_mask);

int cmdq_pkt_write_s_value(struct cmdq_pkt *pkt, u8 high_addr_reg_idx,
			   u16 addr_low, u32 value)
{
	struct cmdq_instruction inst = {};

	inst.op = CMDQ_CODE_WRITE_S;
	inst.s_op = high_addr_reg_idx;
	inst.offset = addr_low;
	inst.value = value;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_write_s_value);

int cmdq_pkt_write_s_mask_value(struct cmdq_pkt *pkt, u8 high_addr_reg_idx,
				u16 addr_low, u32 value, u32 mask)
{
	struct cmdq_instruction inst = {};
	int err;

	inst.op = CMDQ_CODE_MASK;
	inst.mask = ~mask;
	err = cmdq_pkt_append_command(pkt, inst);
	if (err < 0)
		return err;

	inst.op = CMDQ_CODE_WRITE_S_MASK;
	inst.s_op = high_addr_reg_idx;
	inst.offset = addr_low;
	inst.value = value;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_write_s_mask_value);

int cmdq_pkt_write_reg_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
			    u16 src_reg_idx, u32 mask)
{
	s32 err;
	const u16 dst_reg_idx = CMDQ_SPR_FOR_TEMP;

	err = cmdq_pkt_assign(pkt, dst_reg_idx,
			      CMDQ_ADDR_HIGH(addr));
	if (err < 0)
		return err;

	if (mask != CMDQ_NO_MASK)
		return cmdq_pkt_write_s(pkt, dst_reg_idx,
						   CMDQ_ADDR_LOW(addr), src_reg_idx);

	return cmdq_pkt_write_s_mask(pkt, dst_reg_idx,
				     CMDQ_ADDR_LOW(addr), src_reg_idx, mask);
}
EXPORT_SYMBOL(cmdq_pkt_write_reg_addr);

int cmdq_pkt_write_value_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
			      u32 value, u32 mask)
{
	s32 err;
	const u16 dst_reg_idx = CMDQ_SPR_FOR_TEMP;

	/* assign high bit to spr temp */
	err = cmdq_pkt_assign(pkt, dst_reg_idx,
		CMDQ_ADDR_HIGH(addr));
	if (err < 0)
		return err;

	if (mask != CMDQ_NO_MASK)
		return cmdq_pkt_write_s_mask_value(pkt, dst_reg_idx,
						   CMDQ_ADDR_LOW(addr), value, mask);

	return cmdq_pkt_write_s_value(pkt, dst_reg_idx,
				      CMDQ_ADDR_LOW(addr), value);
}
EXPORT_SYMBOL(cmdq_pkt_write_value_addr);

s32 cmdq_pkt_mem_move(struct cmdq_pkt *pkt, dma_addr_t src_addr,
		      dma_addr_t dst_addr, u16 swap_reg_idx)
{
	s32 err;

	err = cmdq_pkt_read_addr(pkt, src_addr, swap_reg_idx);
	if (err != 0)
		return err;

	return cmdq_pkt_write_reg_addr(pkt, dst_addr,
				       swap_reg_idx, CMDQ_NO_MASK);
}
EXPORT_SYMBOL(cmdq_pkt_mem_move);

int cmdq_pkt_wfe(struct cmdq_pkt *pkt, u16 event, bool clear)
{
	struct cmdq_instruction inst = { {0} };
	u32 clear_option = clear ? CMDQ_WFE_UPDATE : 0;

	if (event >= CMDQ_MAX_EVENT)
		return -EINVAL;

	inst.op = CMDQ_CODE_WFE;
	inst.value = CMDQ_WFE_OPTION | clear_option;
	inst.event = event;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_wfe);

int cmdq_pkt_clear_event(struct cmdq_pkt *pkt, u16 event)
{
	struct cmdq_instruction inst = { {0} };

	if (event >= CMDQ_MAX_EVENT)
		return -EINVAL;

	inst.op = CMDQ_CODE_WFE;
	inst.value = CMDQ_WFE_UPDATE;
	inst.event = event;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_clear_event);

int cmdq_pkt_set_event(struct cmdq_pkt *pkt, u16 event)
{
	struct cmdq_instruction inst = {};

	if (event >= CMDQ_MAX_EVENT)
		return -EINVAL;

	inst.op = CMDQ_CODE_WFE;
	inst.value = CMDQ_WFE_UPDATE | CMDQ_WFE_UPDATE_VALUE;
	inst.event = event;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_set_event);

int cmdq_pkt_acquire_event(struct cmdq_pkt *pkt, u16 event)
{
	struct cmdq_instruction inst = {};

	if (event >= CMDQ_MAX_EVENT)
		return -EINVAL;

	inst.op = CMDQ_CODE_WFE;
	inst.value = CMDQ_WFE_UPDATE | CMDQ_WFE_UPDATE_VALUE | CMDQ_WFE_WAIT;
	inst.event = event;

	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_acquire_event);

int cmdq_pkt_poll(struct cmdq_pkt *pkt, u8 subsys,
		  u16 offset, u32 value)
{
	struct cmdq_instruction inst = { {0} };
	int err;

	inst.op = CMDQ_CODE_POLL;
	inst.value = value;
	inst.offset = offset;
	inst.subsys = subsys;
	err = cmdq_pkt_append_command(pkt, inst);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_poll);

int cmdq_pkt_poll_mask(struct cmdq_pkt *pkt, u8 subsys,
		       u16 offset, u32 value, u32 mask)
{
	struct cmdq_instruction inst = { {0} };
	int err;

	inst.op = CMDQ_CODE_MASK;
	inst.mask = ~mask;
	err = cmdq_pkt_append_command(pkt, inst);
	if (err < 0)
		return err;

	offset = offset | CMDQ_POLL_ENABLE_MASK;
	err = cmdq_pkt_poll(pkt, subsys, offset, value);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_poll_mask);

int cmdq_pkt_poll_addr(struct cmdq_pkt *pkt, u32 value, u32 addr,
		       u32 mask, u8 reg_gpr)
{
	struct cmdq_instruction inst = { {0} };
	int err;
	u8 use_mask = 0;

	if (mask != CMDQ_NO_MASK) {
		inst.arg_c = CMDQ_GET_ARG_C(~mask);
		inst.arg_b = CMDQ_GET_ARG_B(~mask);
		inst.arg_a = 0;
		inst.s_op = 0;
		inst.arg_c = 0;
		inst.arg_b = 0;
		inst.arg_a = 1;
		inst.op = CMDQ_CODE_MASK;
		err = cmdq_pkt_append_command(pkt, inst);
		if (err != 0)
			return err;

		use_mask = 1;
	}

	/* Move extra handle APB address to GPR */
	inst.arg_c = CMDQ_GET_ARG_C(addr);
	inst.arg_b = CMDQ_GET_ARG_B(addr);
	inst.arg_a = 0;
	inst.s_op = reg_gpr;
	inst.arg_c = 0;
	inst.arg_b = 0;
	inst.arg_a = 1;
	inst.op = CMDQ_CODE_MOVE;
	err = cmdq_pkt_append_command(pkt, inst);
	if (err < 0)
		return err;

	inst.arg_c = CMDQ_GET_ARG_C(value);
	inst.arg_b = CMDQ_GET_ARG_B(value);
	inst.arg_a = use_mask;
	inst.s_op = reg_gpr;
	inst.arg_c = 0;
	inst.arg_b = 0;
	inst.arg_a = 1;
	inst.op = CMDQ_CODE_POLL;
	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_poll_addr);

int cmdq_pkt_logic_command(struct cmdq_pkt *pkt, enum CMDQ_LOGIC_ENUM s_op,
	u16 result_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand)
{
	struct cmdq_instruction inst = { {0} };
	u32 left_idx_value;
	u32 right_idx_value;

	if (!left_operand || !right_operand)
		return -EINVAL;

	left_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(left_operand);
	right_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(right_operand);

	inst.op = CMDQ_CODE_LOGIC;
	inst.arg_a_type = CMDQ_REG_TYPE;
	inst.arg_b_type = CMDQ_OPERAND_TYPE(left_operand);
	inst.arg_c_type = CMDQ_OPERAND_TYPE(right_operand);
	inst.s_op = s_op;
	inst.arg_c = right_idx_value;
	inst.arg_b = left_idx_value;
	inst.reg_dst = result_reg_idx;
	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_logic_command);

int cmdq_pkt_assign(struct cmdq_pkt *pkt, u16 reg_idx, u32 value)
{
	struct cmdq_instruction inst = {};

	inst.op = CMDQ_CODE_LOGIC;
	inst.arg_a_type = CMDQ_REG_TYPE;
	inst.reg_dst = reg_idx;
	inst.value = value;
	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_assign);

int cmdq_pkt_jump(struct cmdq_pkt *pkt, dma_addr_t addr)
{
	struct cmdq_instruction inst = {};

	inst.op = CMDQ_CODE_JUMP;
	inst.offset = CMDQ_JUMP_RELATIVE;
	inst.value = addr >>
		cmdq_get_shift_pa(((struct cmdq_client *)pkt->cl)->chan);
	return cmdq_pkt_append_command(pkt, inst);
}
EXPORT_SYMBOL(cmdq_pkt_jump);

int cmdq_pkt_finalize(struct cmdq_pkt *pkt)
{
	struct cmdq_instruction inst = { {0} };
	int err;

	/* insert EOC and generate IRQ for each command iteration */
	inst.op = CMDQ_CODE_EOC;
	inst.value = CMDQ_EOC_IRQ_EN;
	err = cmdq_pkt_append_command(pkt, inst);
	if (err < 0)
		return err;

	/* JUMP to end */
	inst.op = CMDQ_CODE_JUMP;
	inst.value = CMDQ_JUMP_PASS >>
		cmdq_get_shift_pa(((struct cmdq_client *)pkt->cl)->chan);
	err = cmdq_pkt_append_command(pkt, inst);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_finalize);

int cmdq_pkt_flush_async(struct cmdq_pkt *pkt)
{
	int err;
	struct cmdq_client *client = (struct cmdq_client *)pkt->cl;

	err = mbox_send_message(client->chan, pkt);
	if (err < 0)
		return err;
	/* We can send next packet immediately, so just call txdone. */
	mbox_client_txdone(client->chan, 0);

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_flush_async);

MODULE_LICENSE("GPL v2");
