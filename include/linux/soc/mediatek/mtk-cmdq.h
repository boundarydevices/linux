/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 */

#ifndef __MTK_CMDQ_H__
#define __MTK_CMDQ_H__

#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/timer.h>

#define CMDQ_SPR_FOR_TEMP		0
#define CMDQ_THR_SPR_IDX0		0
#define CMDQ_THR_SPR_IDX1		1
#define CMDQ_THR_SPR_IDX2		2
#define CMDQ_THR_SPR_IDX3		3

#define CMDQ_GPR_IDX			32
#define CMDQ_TPR_IDX			56
#define CMDQ_CPR_IDX			0x8000

#define SUBSYS_NO_SUPPORT		99

#define CMDQ_NO_MASK		GENMASK(31, 0)
#define CMDQ_ADDR_HIGH(addr)	((u32)(((addr) >> 16) & GENMASK(31, 0)))
#define CMDQ_ADDR_LOW(addr)	((u16)(addr) | BIT(1))

/* GCE provide 32/64 bit General Purpose Register (GPR)
 * use as data cache or address register
 *	 32bit: R0-R15
 *	 64bit: P0-P7
 * Note:
 *	R0-R15 and P0-P7 actullay share same memory
 *	R0 use as mask in instruction, thus be care of use R1/P0.
 */
enum cmdq_gpr {
	/* 32bit R0 to R15 */
	CMDQ_GPR_R0 = 0x0,
	CMDQ_GPR_R1 = 0x1,
	CMDQ_GPR_R2 = 0x2,
	CMDQ_GPR_R3 = 0x3,
	CMDQ_GPR_R4 = 0x4,
	CMDQ_GPR_R5 = 0x5,
	CMDQ_GPR_R6 = 0x6,
	CMDQ_GPR_R7 = 0x7,
	CMDQ_GPR_R8 = 0x8,
	CMDQ_GPR_R9 = 0x9,
	CMDQ_GPR_R10 = 0xa,
	CMDQ_GPR_R11 = 0xb,
	CMDQ_GPR_R12 = 0xc,
	CMDQ_GPR_R13 = 0xd,
	CMDQ_GPR_R14 = 0xe,
	CMDQ_GPR_R15 = 0xf,

	/* 64bit P0 to P7 */
	CMDQ_GPR_P0 = 0x10,
	CMDQ_GPR_P1 = 0x11,
	CMDQ_GPR_P2 = 0x12,
	CMDQ_GPR_P3 = 0x13,
	CMDQ_GPR_P4 = 0x14,
	CMDQ_GPR_P5 = 0x15,
	CMDQ_GPR_P6 = 0x16,
	CMDQ_GPR_P7 = 0x17,
};
struct cmdq_pkt;

enum CMDQ_CONDITION_ENUM {
	CMDQ_EQUAL = 0,
	CMDQ_NOT_EQUAL = 1,
	CMDQ_GREATER_THAN_AND_EQUAL = 2,
	CMDQ_LESS_THAN_AND_EQUAL = 3,
	CMDQ_GREATER_THAN = 4,
	CMDQ_LESS_THAN = 5,

	CMDQ_CONDITION_MAX,
};

enum CMDQ_LOGIC_ENUM {
	CMDQ_LOGIC_ASSIGN = 0,
	CMDQ_LOGIC_ADD = 1,
	CMDQ_LOGIC_SUBTRACT = 2,
	CMDQ_LOGIC_MULTIPLY = 3,
	CMDQ_LOGIC_XOR = 8,
	CMDQ_LOGIC_NOT = 9,
	CMDQ_LOGIC_OR = 10,
	CMDQ_LOGIC_AND = 11,
	CMDQ_LOGIC_LEFT_SHIFT = 12,
	CMDQ_LOGIC_RIGHT_SHIFT = 13,
};

struct cmdq_operand {
	/* register type */
	bool reg;
	union {
		/* index */
		u16 idx;
		/* value */
		u16 value;
	};
};

struct cmdq_client_reg {
	u8 subsys;
	u16 offset;
	u16 size;
};

struct cmdq_client {
	struct mbox_client client;
	struct mbox_chan *chan;
};

/**
 * cmdq_dev_get_client_reg() - parse cmdq client reg from the device
 *			       node of CMDQ client
 * @dev:	device of CMDQ mailbox client
 * @client_reg: CMDQ client reg pointer
 * @idx:	the index of desired reg
 *
 * Return: 0 for success; else the error code is returned
 *
 * Help CMDQ client parsing the cmdq client reg
 * from the device node of CMDQ client.
 */
int cmdq_dev_get_client_reg(struct device *dev,
			    struct cmdq_client_reg *client_reg, int idx);

/**
 * cmdq_mbox_create() - create CMDQ mailbox client and channel
 * @dev:	device of CMDQ mailbox client
 * @index:	index of CMDQ mailbox channel
 *
 * Return: CMDQ mailbox client pointer
 */
struct cmdq_client *cmdq_mbox_create(struct device *dev, int index);

/**
 * cmdq_mbox_destroy() - destroy CMDQ mailbox client and channel
 * @client:	the CMDQ mailbox client
 */
void cmdq_mbox_destroy(struct cmdq_client *client);

/**
 * cmdq_pkt_create() - create a CMDQ packet
 * @client:	the CMDQ mailbox client
 * @size:	required CMDQ buffer size
 *
 * Return: CMDQ packet pointer
 */
struct cmdq_pkt *cmdq_pkt_create(struct cmdq_client *client, size_t size);

/**
 * cmdq_pkt_destroy() - destroy the CMDQ packet
 * @pkt:	the CMDQ packet
 */
void cmdq_pkt_destroy(struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_mem_move() - append memory move command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @src_addr:	source address
 * @dma_addr_t:	destination address
 * @swap_reg_idx:	the temp register idx for swapping
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_mem_move(struct cmdq_pkt *pkt, dma_addr_t src_addr,
		      dma_addr_t dst_addr, u16 swap_reg_idx);

/**
 * cmdq_pkt_move() - append move command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @gpr_idx:	the GCE GPR register index
 * @value:	move extra handle APB address to GPR
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_move(struct cmdq_pkt *pkt, u8 gpr_idx, u32 value);

/**
 * cmdq_pkt_write() - append write command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @subsys:	the CMDQ sub system code
 * @offset:	register offset from CMDQ sub system
 * @value:	the specified target register value
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write(struct cmdq_pkt *pkt, u8 subsys, u16 offset, u32 value);

/**
 * cmdq_pkt_write_mask() - append write command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @subsys:	the CMDQ sub system code
 * @offset:	register offset from CMDQ sub system
 * @value:	the specified target register value
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write_mask(struct cmdq_pkt *pkt, u8 subsys,
			u16 offset, u32 value, u32 mask);

/*
 * cmdq_pkt_read_s() - append read_s command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @high_addr_reg_idx:	internal register ID which contains high address of pa
 * @addr_low:	low address of pa
 * @reg_idx:	the CMDQ internal register ID to cache read data
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_read_s(struct cmdq_pkt *pkt, u16 high_addr_reg_idx, u16 addr_low,
		    u16 reg_idx);

/*
 * cmdq_pkt_read_reg() - append read_s command to the CMDQ packet
 *			      which read the value of subsys code to a CMDQ
 *			      internal register ID
 * @pkt:	the CMDQ packet
 * @subsys:	the CMDQ sub system code
 * @offset:	register offset from CMDQ sub system
 * @dst_reg_idx:	the CMDQ internal register ID to cache read data
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_read_reg(struct cmdq_pkt *pkt, u8 subsys, u16 offset,
		      u16 dst_reg_idx);

/*
 * cmdq_pkt_read_addr() - append read_s command to the CMDQ packet
 *			      which read the value of physical address to a CMDQ
 *			      internal register ID
 * @pkt:	the CMDQ packet
 * @addr:	address of pa
 * @dst_reg_idx:	the CMDQ internal register ID to cache read data
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_read_addr(struct cmdq_pkt *pkt, dma_addr_t addr, u16 dst_reg_idx);

/**
 * cmdq_pkt_write_s() - append write_s command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @high_addr_reg_idx:	internal register ID which contains high address of pa
 * @addr_low:	low address of pa
 * @src_reg_idx:	the CMDQ internal register ID which cache source value
 *
 * Return: 0 for success; else the error code is returned
 *
 * Support write value to physical address without subsys. Use CMDQ_ADDR_HIGH()
 * to get high address and call cmdq_pkt_assign() to assign value into internal
 * reg. Also use CMDQ_ADDR_LOW() to get low address for addr_low parameter when
 * call to this function.
 */
int cmdq_pkt_write_s(struct cmdq_pkt *pkt, u16 high_addr_reg_idx,
		     u16 addr_low, u16 src_reg_idx);

/**
 * cmdq_pkt_write_s_mask() - append write_s with mask command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @high_addr_reg_idx:	internal register ID which contains high address of pa
 * @addr_low:	low address of pa
 * @src_reg_idx:	the CMDQ internal register ID which cache source value
 * @mask:	the specified target address mask, use U32_MAX if no need
 *
 * Return: 0 for success; else the error code is returned
 *
 * Support write value to physical address without subsys. Use CMDQ_ADDR_HIGH()
 * to get high address and call cmdq_pkt_assign() to assign value into internal
 * reg. Also use CMDQ_ADDR_LOW() to get low address for addr_low parameter when
 * call to this function.
 */
int cmdq_pkt_write_s_mask(struct cmdq_pkt *pkt, u16 high_addr_reg_idx,
			  u16 addr_low, u16 src_reg_idx, u32 mask);

/**
 * cmdq_pkt_write_s_value() - append write_s command to the CMDQ packet which
 *			      write value to a physical address
 * @pkt:	the CMDQ packet
 * @high_addr_reg_idx:	internal register ID which contains high address of pa
 * @addr_low:	low address of pa
 * @value:	the specified target value
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write_s_value(struct cmdq_pkt *pkt, u8 high_addr_reg_idx,
			   u16 addr_low, u32 value);

/**
 * cmdq_pkt_write_s_mask_value() - append write_s command with mask to the CMDQ
 *				   packet which write value to a physical address
 * @pkt:	the CMDQ packet
 * @high_addr_reg_idx:	internal register ID which contains high address of pa
 * @addr_low:	low address of pa
 * @value:	the specified target value
 * @mask:	the specified target mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write_s_mask_value(struct cmdq_pkt *pkt, u8 high_addr_reg_idx,
				u16 addr_low, u32 value, u32 mask);

/**
 * cmdq_pkt_write_reg_addr() - append write_s command w/o mask to the CMDQ
 *				packet which write the value in CMDQ internal source register
 *				to a physical address
 * @pkt:	the CMDQ packet
 * @addr	address of pa
 * @src_reg_idx:	the CMDQ internal register ID which cache source value
 * @mask:	the specified target mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write_reg_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
				u16 src_reg_idx, u32 mask);

/**
 * cmdq_pkt_write_value_addr() - append write_s command w/o mask to the CMDQ
 *				   packet which write value to a physical address
 * @pkt:	the CMDQ packet
 * @addr	address of pa
 * @value:	the specified target value
 * @mask:	the specified target mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write_value_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
				  u32 value, u32 mask);

/**
 * cmdq_pkt_wfe() - append wait for event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event type to wait
 * @clear:	clear event or not after event arrive
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_wfe(struct cmdq_pkt *pkt, u16 event, bool clear);

/**
 * cmdq_pkt_clear_event() - append clear event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event to be cleared
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_clear_event(struct cmdq_pkt *pkt, u16 event);

/**
 * cmdq_pkt_acquire_event() - append acquire event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event to be acquired
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_acquire_event(struct cmdq_pkt *pkt, u16 event);

/**
 * cmdq_pkt_set_event() - append set event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event to be set
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_set_event(struct cmdq_pkt *pkt, u16 event);

/**
 * cmdq_pkt_poll() - Append polling command to the CMDQ packet, ask GCE to
 *		     execute an instruction that wait for a specified
 *		     hardware register to check for the value w/o mask.
 *		     All GCE hardware threads will be blocked by this
 *		     instruction.
 * @pkt:	the CMDQ packet
 * @subsys:	the CMDQ sub system code
 * @offset:	register offset from CMDQ sub system
 * @value:	the specified target register value
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll(struct cmdq_pkt *pkt, u8 subsys,
		  u16 offset, u32 value);

/**
 * cmdq_pkt_poll_mask() - Append polling command to the CMDQ packet, ask GCE to
 *		          execute an instruction that wait for a specified
 *		          hardware register to check for the value w/ mask.
 *		          All GCE hardware threads will be blocked by this
 *		          instruction.
 * @pkt:	the CMDQ packet
 * @subsys:	the CMDQ sub system code
 * @offset:	register offset from CMDQ sub system
 * @value:	the specified target register value
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll_mask(struct cmdq_pkt *pkt, u8 subsys,
		       u16 offset, u32 value, u32 mask);

/**
 * cmdq_pkt_poll_addr() - Append polling command to the CMDQ packet, ask GCE to
 *				 execute an instruction that wait for a specified
 *				 hardware register address to check for the value
 *				 w/ mask.
 *				 All GCE hardware threads will be blocked by this
 *				 instruction.
 * @pkt:    the CMDQ packet
 * @value:  the specified target register value
 * @addr: the hardward register address
 * @mask:   the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll_addr(struct cmdq_pkt *pkt, u32 value, u32 addr,
		       u32 mask, u8 reg_gpr);

/**
 * cmdq_pkt_logic_command() - Append logic command to the CMDQ packet, ask GCE to
 *		          execute an instruction that perform logic operation with
 *		          left and right side operands and store result into the
 *		          specified register.
 * @pkt:	the CMDQ packet
 * @s_op:	the logic operation to execute.
 * @result_reg_idx:	the register index for storing result.
 * @left_operand:	the left side operand for the specified logic operation.
 * @right_operand: the right side operand for the specified logic operation.
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_logic_command(struct cmdq_pkt *pkt, enum CMDQ_LOGIC_ENUM s_op,
			   u16 result_reg_idx,
			   struct cmdq_operand *left_operand,
			   struct cmdq_operand *right_operand);

/**
 * cmdq_pkt_poll_timeout() - Append polling command to the CMDQ packet, ask GCE to
 *		          execute an instruction that wait for a specified
 *		          hardware register to check for the value w/ mask
 *		          and it's limited in a specific number of gce-timer tick.
 *		          All GCE hardware threads will be blocked by this
 *		          instruction.
 * @pkt:	the CMDQ packet
 * @subsys:	the CMDQ sub system code
 * @addr:	the specified target address to read.
 * @mask:	the specified target register mask
 * @count:	the count limit for polling the target address.
 * @reg_gpr:	the specified target register value used for storing polling data.
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll_timeout(struct cmdq_pkt *pkt, u32 value, u8 subsys,
	u32 addr, u32 mask, u16 count, u16 reg_gpr);

/**
 * cmdq_pkt_assign() - Append logic assign command to the CMDQ packet, ask GCE
 *		       to execute an instruction that set a constant value into
 *		       internal register and use as value, mask or address in
 *		       read/write instruction.
 * @pkt:	the CMDQ packet
 * @reg_idx:	the CMDQ internal register ID
 * @value:	the specified value
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_assign(struct cmdq_pkt *pkt, u16 reg_idx, u32 value);

/**
 * cmdq_pkt_jump() - Append jump command to the CMDQ packet, ask GCE
 *		     to execute an instruction that change current thread PC to
 *		     a physical address which should contains more instruction.
 * @pkt:        the CMDQ packet
 * @addr:       physical address of target instruction buffer
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_jump(struct cmdq_pkt *pkt, dma_addr_t addr);

/**
 * cmdq_pkt_jump_offset() - Append jump command to the CMDQ packet, ask GCE
 *             to execute an instruction that change current thread PC to
 *             an offset address which should contains more instruction.
 * @pkt:        the CMDQ packet
 * @offset:     offset address of target instruction buffer
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_jump_offset(struct cmdq_pkt *pkt, s32 offset);

/**
 * cmdq_pkt_finalize() - Append EOC and jump command to pkt.
 * @pkt:	the CMDQ packet
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_finalize(struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_flush_async() - trigger CMDQ to asynchronously execute the CMDQ
 *                          packet and call back at the end of done packet
 * @pkt:	the CMDQ packet
 * @cb:		called at the end of done packet
 * @data:	this data will pass back to cb
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to asynchronously execute the CMDQ packet and call back
 * at the end of done packet. Note that this is an ASYNC function. When the
 * function returned, it may or may not be finished.
 */
int cmdq_pkt_flush_async(struct cmdq_pkt *pkt, cmdq_async_flush_cb cb,
			 void *data);

#endif	/* __MTK_CMDQ_H__ */
