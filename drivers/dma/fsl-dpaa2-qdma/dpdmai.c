// SPDX-License-Identifier: GPL-2.0
// Copyright 2019-2021 NXP

#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/fsl/mc.h>
#include "dpdmai.h"

/**
 * dpdmai_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpdmai_id:	DPDMAI unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpdmai_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_open(struct fsl_mc_io *mc_io, u32 cmd_flags,
		int dpdmai_id, u16 *token)
{
	struct dpdmai_cmd_open *cmd_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_OPEN,
					  cmd_flags, 0);

	cmd_params = (struct dpdmai_cmd_open *)cmd.params;
	cmd_params->dpdmai_id = cpu_to_le32(dpdmai_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_open);

/**
 * dpdmai_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_close(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_CLOSE,
					  cmd_flags, token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_close);

/**
 * dpdmai_create() - Create the DPDMAI object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @cfg:	Configuration structure
 * @token:	Returned token; use in subsequent API calls
 *
 * Create the DPDMAI object, allocate required resources and
 * perform required initialization.
 *
 * The object can be created either by declaring it in the
 * DPL file, or by calling this function.
 *
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent calls to
 * this specific object. For objects that are created using the
 * DPL file, call dpdmai_open() function to get an authentication
 * token first.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_create(struct fsl_mc_io *mc_io, u32 cmd_flags,
		  const struct dpdmai_cfg *cfg, u16 token,
		  u32 *dpdmai_id)
{
	struct dpdmai_cmd_create *cmd_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_CREATE,
					  cmd_flags, token);
	cmd_params = (struct dpdmai_cmd_create *)cmd.params;
	cmd_params->num_queues = cfg->num_queues;
	cmd_params->priorities[0] = cfg->priorities[0];
	cmd_params->priorities[1] = cfg->priorities[1];

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*dpdmai_id = mc_cmd_read_object_id(&cmd);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_create);

/**
 * dpdmai_destroy() - Destroy the DPDMAI object and release all its resources.
 * @mc_io:	Pointer to MC portal's I/O object
 * @token:	Parent container token; '0' for default container
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @object_id:	The object id; it must be a valid id within the container that
 *		created this object;
 *
 * The function accepts the authentication token of the parent container that
 * created the object (not the one that currently owns the object). The object
 * is searched within parent using the provided 'object_id'.
 * All tokens to the object must be closed before calling destroy.
 *
 * Return:	'0' on Success; error code otherwise.
 */
int dpdmai_destroy(struct fsl_mc_io *mc_io, u32 cmd_flags,
		   u32 dpdmai_id, u16 token)
{
	struct dpdmai_cmd_destroy *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_DESTROY,
					  cmd_flags, token);
	cmd_params = (struct dpdmai_cmd_destroy *)cmd.params;
	cmd_params->dpdmai_id = cpu_to_le32(dpdmai_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_destroy);

/**
 * dpdmai_enable() - Enable the DPDMAI, allow sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_ENABLE,
					  cmd_flags, token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_enable);

/**
 * dpdmai_disable() - Disable the DPDMAI, stop sending and receiving frames.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_DISABLE,
					  cmd_flags, token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_disable);

/**
 * dpdmai_is_enabled() - Check if the DPDMAI is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_is_enabled(struct fsl_mc_io *mc_io, u32 cmd_flags,
		      u16 token, int *en)
{
	struct dpdmai_rsp_is_enabled *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_IS_ENABLED,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmai_rsp_is_enabled *)cmd.params;
	*en = dpdmai_get_field(rsp_params->en, ENABLE);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_is_enabled);

/**
 * dpdmai_reset() - Reset the DPDMAI, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_reset(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_RESET,
					  cmd_flags, token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_reset);

/**
 * dpdmai_get_attributes() - Retrieve DPDMAI attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 * @attr:	Returned object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags,
			  u16 token, struct dpdmai_attr *attr)
{
	struct dpdmai_rsp_get_attributes *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_GET_ATTR,
					  cmd_flags, token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmai_rsp_get_attributes *)cmd.params;
	attr->id = le32_to_cpu(rsp_params->id);
	attr->num_of_priorities = rsp_params->num_of_priorities;
	attr->num_of_queues = rsp_params->num_of_queues;

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_get_attributes);

/**
 * dpdmai_set_rx_queue() - Set Rx queue configuration
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 * @priority:	Select the queue relative to number of
 *		priorities configured at DPDMAI creation
 * @cfg:	Rx queue configuration
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_set_rx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags,
			u16 token, u8 queue_idx, u8 priority,
			const struct dpdmai_rx_queue_cfg *cfg)
{
	struct dpdmai_cmd_set_rx_queue *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_SET_RX_QUEUE,
					  cmd_flags, token);

	cmd_params = (struct dpdmai_cmd_set_rx_queue *)cmd.params;
	cmd_params->dest_id = cpu_to_le32(cfg->dest_cfg.dest_id);
	cmd_params->dest_priority = cfg->dest_cfg.priority;
	cmd_params->priority = priority;
	cmd_params->queue_idx = queue_idx;
	cmd_params->user_ctx = cpu_to_le64(cfg->user_ctx);
	cmd_params->options = cpu_to_le32(cfg->options);
	dpdmai_set_field(cmd_params->dest_type,
			 DEST_TYPE,
			 cfg->dest_cfg.dest_type);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_set_rx_queue);

/**
 * dpdmai_get_rx_queue() - Retrieve Rx queue attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 * @priority:	Select the queue relative to number of
 *				priorities configured at DPDMAI creation
 * @attr:	Returned Rx queue attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_get_rx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags,
			u16 token, u8 queue_idx, u8 priority,
			struct dpdmai_rx_queue_attr *attr)
{
	struct dpdmai_cmd_get_queue *cmd_params;
	struct dpdmai_rsp_get_rx_queue *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_GET_RX_QUEUE,
					  cmd_flags, token);

	cmd_params = (struct dpdmai_cmd_get_queue *)cmd.params;
	cmd_params->priority = priority;
	cmd_params->queue_idx = queue_idx;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	rsp_params = (struct dpdmai_rsp_get_rx_queue *)cmd.params;
	attr->user_ctx = le64_to_cpu(rsp_params->user_ctx);
	attr->fqid = le32_to_cpu(rsp_params->fqid);
	attr->dest_cfg.dest_id = le32_to_cpu(rsp_params->dest_id);
	attr->dest_cfg.priority = le32_to_cpu(rsp_params->dest_priority);
	attr->dest_cfg.dest_type = dpdmai_get_field(rsp_params->dest_type,
						    DEST_TYPE);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_get_rx_queue);

/**
 * dpdmai_get_tx_queue() - Retrieve Tx queue attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPDMAI object
 * @priority:	Select the queue relative to number of
 *			priorities configured at DPDMAI creation
 * @fqid:	Returned Tx queue
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpdmai_get_tx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags,
			u16 token, u8 queue_idx, u8 priority,
			struct dpdmai_tx_queue_attr *attr)
{
	struct dpdmai_cmd_get_queue *cmd_params;
	struct dpdmai_rsp_get_tx_queue *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_GET_TX_QUEUE,
					  cmd_flags, token);

	cmd_params = (struct dpdmai_cmd_get_queue *)cmd.params;
	cmd_params->priority = priority;
	cmd_params->queue_idx = queue_idx;

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */

	rsp_params = (struct dpdmai_rsp_get_tx_queue *)cmd.params;
	attr->fqid = le32_to_cpu(rsp_params->fqid);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_get_tx_queue);

MODULE_LICENSE("GPL v2");
