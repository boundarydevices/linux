// SPDX-License-Identifier: GPL-2.0
//
// mcp25xxfd - Microchip MCP25xxFD Family CAN controller driver
//
// Copyright (c) 2019, 2020 Pengutronix,
//                          Marc Kleine-Budde <kernel@pengutronix.de>
//

#include "mcp25xxfd.h"

static int mcp25xxfd_regmap_write(void *context, const void *data, size_t count)
{
	struct spi_device *spi = context;

	return spi_write(spi, data, count);
}

static int mcp25xxfd_regmap_gather_write(void *context,
					 const void *reg, size_t reg_len,
					 const void *val, size_t val_len)
{
	struct spi_device *spi = context;
	struct spi_transfer xfer[] = {
		{
			.tx_buf = reg,
			.len = reg_len,
		}, {
			.tx_buf = val,
			.len = val_len,
		},
	};

	return spi_sync_transfer(spi, xfer, ARRAY_SIZE(xfer));
}

static inline bool mcp25xxfd_update_bits_read_reg(unsigned int reg)
{
	switch (reg) {
	case MCP25XXFD_CAN_INT:
	case MCP25XXFD_CAN_TEFCON:
	case MCP25XXFD_CAN_FIFOCON(MCP25XXFD_RX_FIFO(0)):
	case MCP25XXFD_ECCSTAT:
	case MCP25XXFD_CRC:
		return false;
	case MCP25XXFD_CAN_BDIAG1:
	case MCP25XXFD_CAN_CON:
	case MCP25XXFD_CAN_FIFOSTA(MCP25XXFD_RX_FIFO(0)):
	case MCP25XXFD_OSC:
		return true;
	default:
		WARN(1, "Status of reg=%03x unknown.\n", reg);
	}

	return true;
}

static int mcp25xxfd_regmap_update_bits(void *context, unsigned int reg,
					unsigned int mask, unsigned int val)

{
	struct spi_device *spi = context;
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_reg_write_buf *buf = &priv->update_bits_buf;
	__be16 cmd;
	__le32 orig_le32 = 0, mask_le32, val_le32, tmp_le32;
	u8 first_byte, last_byte, len;
	int err;

	first_byte = mcp25xxfd_first_byte_set(mask);
	last_byte = mcp25xxfd_last_byte_set(mask);
	len = last_byte - first_byte + 1;

	if (mcp25xxfd_update_bits_read_reg(reg)) {
		cmd = mcp25xxfd_cmd_read(reg + first_byte);
		/* spi_write_then_read() works with non DMA-safe buffers */
		err = spi_write_then_read(priv->spi,
					  &cmd, sizeof(cmd), &orig_le32, len);

		if (err)
			return err;
	}

	mask_le32 = cpu_to_le32(mask >> 8 * first_byte);
	val_le32 = cpu_to_le32(val >> 8 * first_byte);

	tmp_le32 = orig_le32 & ~mask_le32;
	tmp_le32 |= val_le32 & mask_le32;

	buf->cmd = mcp25xxfd_cmd_write(reg + first_byte);
	memcpy(buf->data, &tmp_le32, len);

	return spi_write(spi, buf, sizeof(buf->cmd) + len);
}

static int mcp25xxfd_regmap_read(void *context,
				 const void *reg, size_t reg_len,
				 void *val_buf, size_t val_len)
{
	struct spi_device *spi = context;
	struct spi_transfer xfer[] = {
		{
			.tx_buf = reg,
			.len = reg_len,
		}, {
			.rx_buf = val_buf,
			.len = val_len,
		},
	};

	return spi_sync_transfer(spi, xfer, ARRAY_SIZE(xfer));
}

static const struct regmap_range mcp25xxfd_reg_table_yes_range[] = {
	regmap_reg_range(0x000, 0x2ec),	/* CAN FD Controller Module SFR */
	regmap_reg_range(0x400, 0xbfc),	/* RAM */
	regmap_reg_range(0xe00, 0xe14),	/* MCP2517/18FD SFR */
};

static const struct regmap_access_table mcp25xxfd_reg_table = {
	.yes_ranges = mcp25xxfd_reg_table_yes_range,
	.n_yes_ranges = ARRAY_SIZE(mcp25xxfd_reg_table_yes_range),
};

static const struct regmap_config mcp25xxfd_regmap = {
	.reg_bits = 16,
	.reg_stride = 4,
	.pad_bits = 0,
	.val_bits = 32,
	.max_register = 0xffc,
	.wr_table = &mcp25xxfd_reg_table,
	.rd_table = &mcp25xxfd_reg_table,
	.cache_type = REGCACHE_NONE,
	.read_flag_mask = (__force unsigned long)
		cpu_to_be16(MCP25XXFD_INSTRUCTION_READ),
	.write_flag_mask = (__force unsigned long)
		cpu_to_be16(MCP25XXFD_INSTRUCTION_WRITE),
};

static const struct regmap_bus mcp25xxfd_bus = {
	.write = mcp25xxfd_regmap_write,
	.gather_write = mcp25xxfd_regmap_gather_write,
	.reg_update_bits = mcp25xxfd_regmap_update_bits,
	.read = mcp25xxfd_regmap_read,
	.reg_format_endian_default = REGMAP_ENDIAN_BIG,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

int mcp25xxfd_regmap_init(struct mcp25xxfd_priv *priv)
{
	priv->map = devm_regmap_init(&priv->spi->dev, &mcp25xxfd_bus,
				     priv->spi, &mcp25xxfd_regmap);

	return PTR_ERR_OR_ZERO(priv->map);
}
