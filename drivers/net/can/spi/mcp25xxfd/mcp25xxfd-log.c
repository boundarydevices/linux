// SPDX-License-Identifier: GPL-2.0
//
// mcp25xxfd - Microchip MCP25xxFD Family CAN controller driver
//
// Copyright (c) 2019 Pengutronix,
//                    Marc Kleine-Budde <kernel@pengutronix.de>
//

#include "mcp25xxfd.h"
#include "mcp25xxfd-log.h"

struct mcp25xxfd_log *___mcp25xxfd_log(struct mcp25xxfd_priv *priv, const char *func, canid_t can_id)
{
	struct mcp25xxfd_log *log;
	int cnt;

	cnt = atomic_add_return(1, &priv->cnt);
	cnt &= ARRAY_SIZE(priv->log) - 1;

	log = &priv->log[cnt];
	log->func = func;
	log->can_id = can_id;
	log->tef_head = priv->tef.head;
	log->tef_tail = priv->tef.tail;
	log->tx_head = priv->tx.head;
	log->tx_tail = priv->tx.tail;
	log->flags = 0;

	return log;
}

static void mcp25xxfd_log_dump_one(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_log *last_log, const struct mcp25xxfd_log *log, int n)
{
	pr_info("%04d: %30s: ",
		n, log->func);

	if (log->can_id != -1 &&
	    last_log->can_id != log->can_id)
		pr_cont("id=%03x ", log->can_id);
	else
		pr_cont(" ---   ");

	if (last_log->tef_head != log->tef_head)
		pr_cont("tef_h=%08x/%02x ", log->tef_head, log->tef_head & (priv->tx.obj_num - 1));
	else
		pr_cont("    ---           ");

	if (log->hw_tx_ci != -1 &&
	    last_log->hw_tx_ci != log->hw_tx_ci)
		pr_cont("hw_tx_ci=%02x ", log->hw_tx_ci);
	else
		pr_cont("       ---  ");

	if (last_log->tef_tail != log->tef_tail)
		pr_cont("tef_t=%08x/%02x ", log->tef_tail, log->tef_tail & (priv->tx.obj_num - 1));
	else
		pr_cont("    ---           ");

	if (log->hw_tef_tail != -1 &&
	    last_log->hw_tef_tail != log->hw_tef_tail)
		pr_cont("hw_tef_t=%02x ", log->hw_tef_tail);
	else
		pr_cont("       ---  ");

	if (last_log->tx_head != log->tx_head)
		pr_cont("tx_h=%08x/%02x ", log->tx_head, log->tx_head & (priv->tx.obj_num - 1));
	else
		pr_cont("   ---           ");

	if (last_log->tx_tail != log->tx_tail)
		pr_cont("tx_t=%08x/%02x ", log->tx_tail, log->tx_tail & (priv->tx.obj_num - 1));
	else
		pr_cont("   ---           ");

	pr_cont("%s%s%s\n",
		log->flags & MCP25XXFD_LOG_STOP ? "s" : " ",
		log->flags & MCP25XXFD_LOG_WAKE ? "w" : " ",
		log->flags & MCP25XXFD_LOG_BUSY ? "b" : " ");
}

void mcp25xxfd_log_dump(const struct mcp25xxfd_priv *priv)
{
	int cnt;
	int i, n = 0;

	cnt = atomic_read(&priv->cnt);
	cnt &= ARRAY_SIZE(priv->log) - 1;

	i = cnt;

	if (cnt == 0) {
		mcp25xxfd_log_dump_one(priv, &priv->log[ARRAY_SIZE(priv->log) - 2], &priv->log[0], n++);
		i++;
	}

	for (/* nix */; i < ARRAY_SIZE(priv->log); i++)
		mcp25xxfd_log_dump_one(priv, &priv->log[i - 1], &priv->log[i], n++);

	if (cnt) {
		mcp25xxfd_log_dump_one(priv, &priv->log[ARRAY_SIZE(priv->log) - 2], &priv->log[0], n++);
		if (cnt > 1)
			for (i = 1; i < cnt; i++)
				mcp25xxfd_log_dump_one(priv, &priv->log[i - 1], &priv->log[i], n++);
	}
}
