// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2012 Simon Budig, <simon.budig@kernelconcepts.de>
 * Daniel Wagener <daniel.wagener@kernelconcepts.de> (M09 firmware support)
 * Lothar Waßmann <LW@KARO-electronics.de> (DT support)
 */

/*
 * This is a driver for the EDT "Polytouch" family of touch controllers
 * based on the FocalTech FT5x06 line of chips.
 *
 * Development of this driver has been sponsored by Glyn:
 *    http://www.glyn.com/Products/Displays
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <asm/unaligned.h>

#define WORK_REGISTER_THRESHOLD		0x00
#define WORK_REGISTER_REPORT_RATE	0x08
#define WORK_REGISTER_GAIN		0x30
#define WORK_REGISTER_OFFSET		0x31
#define WORK_REGISTER_NUM_X		0x33
#define WORK_REGISTER_NUM_Y		0x34

#define PMOD_REGISTER_ACTIVE		0x00
#define PMOD_REGISTER_HIBERNATE		0x03

#define M09_REGISTER_THRESHOLD		0x80
#define M09_REGISTER_GAIN		0x92
#define M09_REGISTER_OFFSET		0x93
#define M09_REGISTER_NUM_X		0x94
#define M09_REGISTER_NUM_Y		0x95

#define EV_REGISTER_THRESHOLD		0x40
#define EV_REGISTER_GAIN		0x41
#define EV_REGISTER_OFFSET_Y		0x45
#define EV_REGISTER_OFFSET_X		0x46

#define NO_REGISTER			0xff

#define WORK_REGISTER_OPMODE		0x3c
#define FACTORY_REGISTER_OPMODE		0x01
#define PMOD_REGISTER_OPMODE		0xa5

#define TOUCH_EVENT_DOWN		0x00
#define TOUCH_EVENT_UP			0x01
#define TOUCH_EVENT_ON			0x02
#define TOUCH_EVENT_RESERVED		0x03

#define EDT_NAME_LEN			23
#define EDT_SWITCH_MODE_RETRIES		10
#define EDT_SWITCH_MODE_DELAY		5 /* msec */
#define EDT_RAW_DATA_RETRIES		100
#define EDT_RAW_DATA_DELAY		1000 /* usec */

#define EDT_DEFAULT_NUM_X		1024
#define EDT_DEFAULT_NUM_Y		1024

#define EDT_MAX_FW_SIZE			(60 * 1024)
#define EDT_MAX_NB_TRIES		30
#define EDT_CMD_RESET			0x07
#define EDT_CMD_READ_ID			0x90
#define EDT_CMD_ERASE_APP		0x61
#define EDT_CMD_WRITE			0xBF
#define EDT_CMD_FLASH_STATUS		0x6A
#define EDT_CMD_CHIP_ID			0xA3
#define EDT_CMD_CHIP_ID2		0x9F
#define EDT_CMD_START1			0x55
#define EDT_CMD_START2			0xAA
#define EDT_CMD_START_DELAY		12

#define EDT_UPGRADE_AA			0xAA
#define EDT_UPGRADE_55			0x55
#define EDT_ID				0xA6

#define EDT_RETRIES_WRITE		100
#define EDT_RETRIES_DELAY_WRITE		1
#define EDT_UPGRADE_LOOP		30

#define EDT_DELAY_UPGRADE_RESET		80
#define EDT_DELAY_UPGRADE_AA		10
#define EDT_DELAY_UPGRADE_55		80
#define EDT_DELAY_READ_ID		20
#define EDT_DELAY_ERASE			500
#define EDT_INTERVAL_READ_REG		200
#define EDT_TIMEOUT_READ_REG		1000

#define EDT_FLASH_PACKET_LENGTH		32
#define EDT_CMD_WRITE_LEN		6

#define BYTE_OFF_0(x)			(u8)((x) & 0xFF)
#define BYTE_OFF_8(x)			(u8)(((x) >> 8) & 0xFF)
#define BYTE_OFF_16(x)			(u8)(((x) >> 16) & 0xFF)

enum edt_fw_status {
	EDT_RUN_IN_ERROR,
	EDT_RUN_IN_APP,
	EDT_RUN_IN_ROM,
	EDT_RUN_IN_PRAM,
	EDT_RUN_IN_BOOTLOADER,
};

struct edt_fw_param {
	u8 model;
	u16 bootloader_id;
	u16 chip_id;
	u16 flash_status_ok;
	u16 fw_len_offset;
	u8 cmd_upgrade;
	u16 start_addr;
};

#define EDT_FW_PARAMS(_model, _bid, _cid, _fs, _fo, _cu, _sa) \
	{ \
		.model = _model, \
		.bootloader_id = _bid, \
		.chip_id = _cid, \
		.flash_status_ok = _fs, \
		.fw_len_offset = _fo,  \
		.cmd_upgrade = _cu, \
		.start_addr = _sa, \
	}

static struct edt_fw_param edt_fw_params[] = {
	EDT_FW_PARAMS(0x5F, 0x7918, 0x3600, 0xB002, 0x100, 0xBC, 0),
};

enum edt_pmode {
	EDT_PMODE_NOT_SUPPORTED,
	EDT_PMODE_HIBERNATE,
	EDT_PMODE_POWEROFF,
};

enum edt_ver {
	EDT_M06,
	EDT_M09,
	EDT_M12,
	EV_FT,
	GENERIC_FT,
};

struct edt_reg_addr {
	int reg_threshold;
	int reg_report_rate;
	int reg_gain;
	int reg_offset;
	int reg_offset_x;
	int reg_offset_y;
	int reg_num_x;
	int reg_num_y;
};

struct edt_ft5x06_ts_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct touchscreen_properties prop;
	u16 num_x;
	u16 num_y;
	struct regulator *vcc;
	struct regulator *iovcc;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *wake_gpio;

#if defined(CONFIG_DEBUG_FS)
	struct dentry *debug_dir;
	u8 *raw_buffer;
	size_t raw_bufsize;
#endif

	struct mutex mutex;
	bool factory_mode;
	enum edt_pmode suspend_mode;
	int threshold;
	int gain;
	int offset;
	int offset_x;
	int offset_y;
	int report_rate;
	int max_support_points;

	char name[EDT_NAME_LEN];

	struct edt_reg_addr reg_addr;
	enum edt_ver version;
	struct edt_fw_param *fw_param;
};

struct edt_i2c_chip_data {
	int  max_support_points;
};

static int edt_ft5x06_ts_readwrite(struct i2c_client *client,
				   u16 wr_len, u8 *wr_buf,
				   u16 rd_len, u8 *rd_buf)
{
	struct i2c_msg wrmsg[2];
	int i = 0;
	int ret;

	if (wr_len) {
		wrmsg[i].addr  = client->addr;
		wrmsg[i].flags = 0;
		wrmsg[i].len = wr_len;
		wrmsg[i].buf = wr_buf;
		i++;
	}
	if (rd_len) {
		wrmsg[i].addr  = client->addr;
		wrmsg[i].flags = I2C_M_RD;
		wrmsg[i].len = rd_len;
		wrmsg[i].buf = rd_buf;
		i++;
	}

	ret = i2c_transfer(client->adapter, wrmsg, i);
	if (ret < 0)
		return ret;
	if (ret != i)
		return -EIO;

	return 0;
}

static bool edt_ft5x06_ts_check_crc(struct edt_ft5x06_ts_data *tsdata,
				    u8 *buf, int buflen)
{
	int i;
	u8 crc = 0;

	for (i = 0; i < buflen - 1; i++)
		crc ^= buf[i];

	if (crc != buf[buflen-1]) {
		dev_err_ratelimited(&tsdata->client->dev,
				    "crc error: 0x%02x expected, got 0x%02x\n",
				    crc, buf[buflen-1]);
		return false;
	}

	return true;
}

static irqreturn_t edt_ft5x06_ts_isr(int irq, void *dev_id)
{
	struct edt_ft5x06_ts_data *tsdata = dev_id;
	struct device *dev = &tsdata->client->dev;
	u8 cmd;
	u8 rdbuf[63];
	int i, type, x, y, id;
	int offset, tplen, datalen, crclen;
	int error;

	switch (tsdata->version) {
	case EDT_M06:
		cmd = 0xf9; /* tell the controller to send touch data */
		offset = 5; /* where the actual touch data starts */
		tplen = 4;  /* data comes in so called frames */
		crclen = 1; /* length of the crc data */
		break;

	case EDT_M09:
	case EDT_M12:
	case EV_FT:
	case GENERIC_FT:
		cmd = 0x0;
		offset = 3;
		tplen = 6;
		crclen = 0;
		break;

	default:
		goto out;
	}

	memset(rdbuf, 0, sizeof(rdbuf));
	datalen = tplen * tsdata->max_support_points + offset + crclen;

	error = edt_ft5x06_ts_readwrite(tsdata->client,
					sizeof(cmd), &cmd,
					datalen, rdbuf);
	if (error) {
		dev_err_ratelimited(dev, "Unable to fetch data, error: %d\n",
				    error);
		goto out;
	}

	/* M09/M12 does not send header or CRC */
	if (tsdata->version == EDT_M06) {
		if (rdbuf[0] != 0xaa || rdbuf[1] != 0xaa ||
			rdbuf[2] != datalen) {
			dev_err_ratelimited(dev,
					"Unexpected header: %02x%02x%02x!\n",
					rdbuf[0], rdbuf[1], rdbuf[2]);
			goto out;
		}

		if (!edt_ft5x06_ts_check_crc(tsdata, rdbuf, datalen))
			goto out;
	}

	for (i = 0; i < tsdata->max_support_points; i++) {
		u8 *buf = &rdbuf[i * tplen + offset];

		type = buf[0] >> 6;
		/* ignore Reserved events */
		if (type == TOUCH_EVENT_RESERVED)
			continue;

		/* M06 sometimes sends bogus coordinates in TOUCH_DOWN */
		if (tsdata->version == EDT_M06 && type == TOUCH_EVENT_DOWN)
			continue;

		x = get_unaligned_be16(buf) & 0x0fff;
		y = get_unaligned_be16(buf + 2) & 0x0fff;
		/* The FT5x26 send the y coordinate first */
		if (tsdata->version == EV_FT)
			swap(x, y);

		id = (buf[2] >> 4) & 0x0f;

		input_mt_slot(tsdata->input, id);
		if (input_mt_report_slot_state(tsdata->input, MT_TOOL_FINGER,
					       type != TOUCH_EVENT_UP))
			touchscreen_report_pos(tsdata->input, &tsdata->prop,
					       x, y, true);
	}

	input_mt_report_pointer_emulation(tsdata->input, true);
	input_sync(tsdata->input);

out:
	return IRQ_HANDLED;
}

static int edt_ft5x06_register_write(struct edt_ft5x06_ts_data *tsdata,
				     u8 addr, u8 value)
{
	u8 wrbuf[4];

	switch (tsdata->version) {
	case EDT_M06:
		wrbuf[0] = tsdata->factory_mode ? 0xf3 : 0xfc;
		wrbuf[1] = tsdata->factory_mode ? addr & 0x7f : addr & 0x3f;
		wrbuf[2] = value;
		wrbuf[3] = wrbuf[0] ^ wrbuf[1] ^ wrbuf[2];
		return edt_ft5x06_ts_readwrite(tsdata->client, 4,
					wrbuf, 0, NULL);

	case EDT_M09:
	case EDT_M12:
	case EV_FT:
	case GENERIC_FT:
		wrbuf[0] = addr;
		wrbuf[1] = value;

		return edt_ft5x06_ts_readwrite(tsdata->client, 2,
					wrbuf, 0, NULL);

	default:
		return -EINVAL;
	}
}

static int edt_ft5x06_register_read(struct edt_ft5x06_ts_data *tsdata,
				    u8 addr)
{
	u8 wrbuf[2], rdbuf[2];
	int error;

	switch (tsdata->version) {
	case EDT_M06:
		wrbuf[0] = tsdata->factory_mode ? 0xf3 : 0xfc;
		wrbuf[1] = tsdata->factory_mode ? addr & 0x7f : addr & 0x3f;
		wrbuf[1] |= tsdata->factory_mode ? 0x80 : 0x40;

		error = edt_ft5x06_ts_readwrite(tsdata->client, 2, wrbuf, 2,
						rdbuf);
		if (error)
			return error;

		if ((wrbuf[0] ^ wrbuf[1] ^ rdbuf[0]) != rdbuf[1]) {
			dev_err(&tsdata->client->dev,
				"crc error: 0x%02x expected, got 0x%02x\n",
				wrbuf[0] ^ wrbuf[1] ^ rdbuf[0],
				rdbuf[1]);
			return -EIO;
		}
		break;

	case EDT_M09:
	case EDT_M12:
	case EV_FT:
	case GENERIC_FT:
		wrbuf[0] = addr;
		error = edt_ft5x06_ts_readwrite(tsdata->client, 1,
						wrbuf, 1, rdbuf);
		if (error)
			return error;
		break;

	default:
		return -EINVAL;
	}

	return rdbuf[0];
}

struct edt_ft5x06_attribute {
	struct device_attribute dattr;
	size_t field_offset;
	u8 limit_low;
	u8 limit_high;
	u8 addr_m06;
	u8 addr_m09;
	u8 addr_ev;
};

#define EDT_ATTR(_field, _mode, _addr_m06, _addr_m09, _addr_ev,		\
		_limit_low, _limit_high)				\
	struct edt_ft5x06_attribute edt_ft5x06_attr_##_field = {	\
		.dattr = __ATTR(_field, _mode,				\
				edt_ft5x06_setting_show,		\
				edt_ft5x06_setting_store),		\
		.field_offset = offsetof(struct edt_ft5x06_ts_data, _field), \
		.addr_m06 = _addr_m06,					\
		.addr_m09 = _addr_m09,					\
		.addr_ev  = _addr_ev,					\
		.limit_low = _limit_low,				\
		.limit_high = _limit_high,				\
	}

static ssize_t edt_ft5x06_setting_show(struct device *dev,
				       struct device_attribute *dattr,
				       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct edt_ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	struct edt_ft5x06_attribute *attr =
			container_of(dattr, struct edt_ft5x06_attribute, dattr);
	u8 *field = (u8 *)tsdata + attr->field_offset;
	int val;
	size_t count = 0;
	int error = 0;
	u8 addr;

	mutex_lock(&tsdata->mutex);

	if (tsdata->factory_mode) {
		error = -EIO;
		goto out;
	}

	switch (tsdata->version) {
	case EDT_M06:
		addr = attr->addr_m06;
		break;

	case EDT_M09:
	case EDT_M12:
	case GENERIC_FT:
		addr = attr->addr_m09;
		break;

	case EV_FT:
		addr = attr->addr_ev;
		break;

	default:
		error = -ENODEV;
		goto out;
	}

	if (addr != NO_REGISTER) {
		val = edt_ft5x06_register_read(tsdata, addr);
		if (val < 0) {
			error = val;
			dev_err(&tsdata->client->dev,
				"Failed to fetch attribute %s, error %d\n",
				dattr->attr.name, error);
			goto out;
		}
	} else {
		val = *field;
	}

	if (val != *field) {
		dev_warn(&tsdata->client->dev,
			 "%s: read (%d) and stored value (%d) differ\n",
			 dattr->attr.name, val, *field);
		*field = val;
	}

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
out:
	mutex_unlock(&tsdata->mutex);
	return error ?: count;
}

static ssize_t edt_ft5x06_setting_store(struct device *dev,
					struct device_attribute *dattr,
					const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct edt_ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	struct edt_ft5x06_attribute *attr =
			container_of(dattr, struct edt_ft5x06_attribute, dattr);
	u8 *field = (u8 *)tsdata + attr->field_offset;
	unsigned int val;
	int error;
	u8 addr;

	mutex_lock(&tsdata->mutex);

	if (tsdata->factory_mode) {
		error = -EIO;
		goto out;
	}

	error = kstrtouint(buf, 0, &val);
	if (error)
		goto out;

	if (val < attr->limit_low || val > attr->limit_high) {
		error = -ERANGE;
		goto out;
	}

	switch (tsdata->version) {
	case EDT_M06:
		addr = attr->addr_m06;
		break;

	case EDT_M09:
	case EDT_M12:
	case GENERIC_FT:
		addr = attr->addr_m09;
		break;

	case EV_FT:
		addr = attr->addr_ev;
		break;

	default:
		error = -ENODEV;
		goto out;
	}

	if (addr != NO_REGISTER) {
		error = edt_ft5x06_register_write(tsdata, addr, val);
		if (error) {
			dev_err(&tsdata->client->dev,
				"Failed to update attribute %s, error: %d\n",
				dattr->attr.name, error);
			goto out;
		}
	}
	*field = val;

out:
	mutex_unlock(&tsdata->mutex);
	return error ?: count;
}

static inline int edt_ft5x06_write_delay(struct edt_ft5x06_ts_data *tsdata,
				   u16 wr_len, u8 *wr_buf,
				   u16 delay)
{
	struct i2c_client *client = tsdata->client;
	int ret;

	ret = edt_ft5x06_ts_readwrite(client, wr_len, wr_buf, 0, NULL);
	if (ret < 0) {
		dev_err(&client->dev, "write cmd failed\n");
		return ret;
	}

	if (delay > 0)
		msleep(delay);

	return 0;
}

static inline bool edt_ft5x06_wait_tp_to_valid(
			struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	int cnt = 0;
	u8 idh;
	u8 idl;
	u8 cmd;
	u16 chip_id;

	do {
		cmd = EDT_CMD_CHIP_ID;
		ret = edt_ft5x06_ts_readwrite(client, 1, &cmd, 1, &idh);
		if (ret < 0) {
			dev_err(&client->dev, "read chip failed\n");
			return false;
		}
		cmd = EDT_CMD_CHIP_ID2;
		ret = edt_ft5x06_ts_readwrite(client, 1, &cmd, 1, &idl);

		if (ret < 0) {
			dev_err(&client->dev, "read chip2 failed\n");
			return false;
		}

		chip_id = (((u16)idh) << 8) + idl;

		if (tsdata->fw_param->chip_id == chip_id)
			return true;

		cnt++;
		msleep(EDT_INTERVAL_READ_REG);
	} while ((cnt * EDT_INTERVAL_READ_REG) < EDT_TIMEOUT_READ_REG);

	return false;
}

static int edt_ft5x06_fwupg_get_boot_state(struct edt_ft5x06_ts_data *tsdata,
				    enum edt_fw_status *fw_sts)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	u8 cmd[4];
	u8 val[2];
	u16 bootloader_id;

	*fw_sts = EDT_RUN_IN_ERROR;
	cmd[0] = EDT_CMD_START1;
	cmd[1] = EDT_CMD_START2;
	ret = edt_ft5x06_write_delay(tsdata, 2,
					cmd, EDT_CMD_START_DELAY);
	if (ret < 0) {
		dev_err(&client->dev, "write 55 aa failed\n");
		return ret;
	}

	cmd[0] = EDT_CMD_READ_ID;
	cmd[1] = cmd[2] = cmd[3] = 0x00;
	ret = edt_ft5x06_ts_readwrite(client, 4, cmd, 2, val);
	if (ret < 0) {
		dev_err(&client->dev, "write 90 failed\n");
		return ret;
	}
	bootloader_id = (((u16)val[0]) << 8) + val[1];

	if (tsdata->fw_param->bootloader_id == bootloader_id)
		*fw_sts = EDT_RUN_IN_BOOTLOADER;

	return 0;
}

static int edt_ft5x06_reset(struct edt_ft5x06_ts_data *tsdata)
{
	u8 cmd = EDT_CMD_RESET;

	return edt_ft5x06_write_delay(tsdata, sizeof(cmd),
					&cmd, EDT_DELAY_UPGRADE_RESET);
}

static int edt_ft5x06_fwupg_reset_to_boot(struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	u8 buf[2];

	buf[0] = tsdata->fw_param->cmd_upgrade;
	buf[1] = EDT_UPGRADE_AA;
	ret = edt_ft5x06_write_delay(tsdata, sizeof(buf),
					buf, EDT_DELAY_UPGRADE_AA);

	if (ret < 0) {
		dev_err(&client->dev, "write AA failed\n");
		return ret;
	}

	buf[1] = EDT_UPGRADE_55;
	ret = edt_ft5x06_write_delay(tsdata, sizeof(buf),
					buf, EDT_DELAY_UPGRADE_55);

	if (ret < 0) {
		dev_err(&client->dev, "write 55 failed\n");
		return ret;
	}

	return 0;
}

static int edt_ft5x06_fwupg_reset_in_boot(struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	u8 cmd = EDT_CMD_RESET;

	ret = edt_ft5x06_write_delay(tsdata, sizeof(cmd),
					&cmd, EDT_DELAY_UPGRADE_RESET);

	if (ret < 0) {
		dev_err(&client->dev, "write cmd reset failed\n");
		return ret;
	}

	return ret;
}

static bool edt_ft5x06_fwupg_check_state(struct edt_ft5x06_ts_data *tsdata,
					enum edt_fw_status rstate)
{
	int ret;
	int i;
	enum edt_fw_status cstate;

	for (i = 0; i < EDT_UPGRADE_LOOP; i++) {
		ret = edt_ft5x06_fwupg_get_boot_state(tsdata, &cstate);
		if (cstate == rstate)
			return true;
		msleep(EDT_DELAY_READ_ID);
	}

	return false;
}

static bool edt_ft5x06_check_flash_status(
	struct edt_ft5x06_ts_data *tsdata,
	u16 flash_status,
	int retries,
	int retries_delay)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	int i;
	u8 cmd[4] = { 0 };
	u8 val[2];
	u16 read_status;

	cmd[0] = EDT_CMD_FLASH_STATUS;
	for (i = 0; i < retries; i++) {
		ret = edt_ft5x06_ts_readwrite(client, 4, cmd, 2, val);
		if (ret < 0) {
			dev_err(&client->dev, "cmd flash status failed\n");
			return 0;
		}
		read_status = (((u16)val[0]) << 8) + val[1];
		if (flash_status == read_status)
			return 1;

		msleep(retries_delay);
	}

	return 0;
}

static int edt_ft5x06_enter_bl(struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	bool fwvalid;
	enum edt_fw_status state;

	fwvalid = edt_ft5x06_wait_tp_to_valid(tsdata);
	if (fwvalid) {
		ret = edt_ft5x06_fwupg_reset_to_boot(tsdata);
		if (ret < 0) {
			dev_err(&client->dev, "enter into bootloader fail\n");
			return ret;
		}
	} else {
		ret = edt_ft5x06_fwupg_reset_in_boot(tsdata);
		if (ret < 0) {
			dev_err(&client->dev, "boot id when fw invalid fail\n");
			return ret;
		}
	}

	state = edt_ft5x06_fwupg_check_state(tsdata, EDT_RUN_IN_BOOTLOADER);
	if (!state) {
		dev_err(&client->dev, "fw not in bootloader, fail\n");
		return -EIO;
	}

	return ret;
}
static int edt_ft5x06_erase(struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	u8 cmd;
	bool flag;

	cmd = EDT_CMD_ERASE_APP;
	ret = edt_ft5x06_write_delay(tsdata, sizeof(cmd),
					&cmd, EDT_DELAY_ERASE);
	if (ret < 0) {
		dev_err(&client->dev, "erase failed\n");
		return ret;
	}
	flag = edt_ft5x06_check_flash_status(tsdata,
			tsdata->fw_param->flash_status_ok, 50, 100);
	if (!flag) {
		dev_err(&client->dev, "ecc flash status check fail\n");
		return -EIO;
	}

	return 0;
}

static int edt_ft5x06_write_fw(struct edt_ft5x06_ts_data *tsdata,
				u32 start_addr, const u8 *buf, u32 len)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	u32 i;
	u32 j;
	u32 packet_number;
	u32 packet_len;
	u32 addr;
	u32 offset;
	u32 remainder;
	u8 packet_buf[EDT_FLASH_PACKET_LENGTH + EDT_CMD_WRITE_LEN];
	u8 ecc_in_host = 0;
	u8 cmd[4];
	u8 val[2];
	u16 read_status;
	u16 wr_ok;

	packet_number = len / EDT_FLASH_PACKET_LENGTH;
	remainder = len % EDT_FLASH_PACKET_LENGTH;
	if (remainder > 0)
		packet_number++;
	packet_len = EDT_FLASH_PACKET_LENGTH;

	packet_buf[0] = EDT_CMD_WRITE;
	for (i = 0; i < packet_number; i++) {
		offset = i * EDT_FLASH_PACKET_LENGTH;
		addr = start_addr + offset;
		packet_buf[1] = BYTE_OFF_16(addr);
		packet_buf[2] = BYTE_OFF_8(addr);
		packet_buf[3] = BYTE_OFF_0(addr);

		/* last packet */
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;

		packet_buf[4] = BYTE_OFF_8(packet_len);
		packet_buf[5] = BYTE_OFF_0(packet_len);

		for (j = 0; j < packet_len; j++) {
			packet_buf[EDT_CMD_WRITE_LEN + j] = buf[offset + j];
			ecc_in_host ^= packet_buf[EDT_CMD_WRITE_LEN + j];
		}

		ret = edt_ft5x06_write_delay(tsdata,
						packet_len + EDT_CMD_WRITE_LEN,
						packet_buf, 0);
		if (ret < 0) {
			dev_err(&client->dev, "write packet failed\n");
			return ret;
		}
		mdelay(1);

		/* read status */
		cmd[0] = EDT_CMD_FLASH_STATUS;
		cmd[1] = 0x00;
		cmd[2] = 0x00;
		cmd[3] = 0x00;
		wr_ok = tsdata->fw_param->flash_status_ok + i + 1;
		for (j = 0; j < EDT_RETRIES_WRITE; j++) {
			ret = edt_ft5x06_ts_readwrite(client, 4, cmd, 2, val);
			read_status = (((u16)val[0]) << 8) + val[1];

			if (wr_ok == read_status)
				break;

			mdelay(EDT_RETRIES_DELAY_WRITE);
		}

	}

	return (int)ecc_in_host;
}

static int edt_ft5x06_ecc_cal(struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int ret;
	u8 reg_val = 0;
	u8 cmd = 0xCC;

	ret = edt_ft5x06_ts_readwrite(client, 1, &cmd, 1, &reg_val);
	if (ret < 0) {
		dev_err(&client->dev, "write cmd ecc failed\n");
		return ret;
	}

	return reg_val;
}

static int edt_ft5x06_i2c_do_update_firmware(struct edt_ft5x06_ts_data *ts,
					u32 start_addr,
					const struct firmware *fw)
{
	struct i2c_client *client = ts->client;
	int ecc_in_host;
	int ecc_in_tp;
	u32 fw_length;
	int ret;

	if (fw->size == 0 || fw->size > EDT_MAX_FW_SIZE) {
		dev_err(&client->dev, "Invalid firmware length\n");
		return -EINVAL;
	}
	fw_length = ((u32)fw->data[ts->fw_param->fw_len_offset] << 8) +
			fw->data[ts->fw_param->fw_len_offset+1];

	ret = edt_ft5x06_enter_bl(ts);
	if (ret) {
		dev_err(&client->dev, "Unable to enter bl %d\n", ret);
		return ret;
	}

	ret = edt_ft5x06_erase(ts);
	if (ret < 0) {
		dev_err(&client->dev, "Error erase %d\n", ret);
		goto fw_reset;
	}

	ecc_in_host = edt_ft5x06_write_fw(ts, start_addr, fw->data, fw_length);
	if (ecc_in_host < 0) {
		dev_err(&client->dev, "Error write fw %d\n", ecc_in_host);
		goto fw_reset;
	}

	ecc_in_tp = edt_ft5x06_ecc_cal(ts);
	if (ecc_in_tp < 0) {
		dev_err(&client->dev, "Error read ecc %d\n", ecc_in_tp);
		goto fw_reset;
	}

	if (ecc_in_tp != ecc_in_host) {
		dev_err(&client->dev, "Error check ecc\n");
		goto fw_reset;
	}

	ret = edt_ft5x06_reset(ts);
	if (ret < 0) {
		dev_err(&client->dev, "Error reset normal\n");
		return -EIO;
	}

	msleep(200);

	return 0;

fw_reset:
	ret = edt_ft5x06_reset(ts);
	if (ret < 0)
		dev_err(&client->dev, "Error reset normal\n");

	return -EIO;
}

static int edt_ft5x06_i2c_fw_update(struct edt_ft5x06_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	const struct firmware *fw = NULL;
	char *fw_file;
	int error;

	if (!ts->fw_param) {
		dev_dbg(&client->dev, "firmware update not supported\n");
		return -EINVAL;
	}

	fw_file = kasprintf(GFP_KERNEL, "ft5x06_%02X.bin", ts->fw_param->model);
	if (!fw_file)
		return -ENOMEM;

	dev_dbg(&client->dev, "firmware name: %s\n", fw_file);

	error = request_firmware(&fw, fw_file, &client->dev);
	if (error) {
		dev_err(&client->dev, "Unable to open firmware %s\n", fw_file);
		goto out_free_fw_file;
	}

	disable_irq(client->irq);

	error = edt_ft5x06_i2c_do_update_firmware(ts,
						ts->fw_param->start_addr, fw);
	if (error) {
		dev_err(&client->dev, "firmware update failed: %d\n", error);
		goto out_enable_irq;
	}

out_enable_irq:
	enable_irq(client->irq);
	msleep(100);

	release_firmware(fw);

out_free_fw_file:
	kfree(fw_file);

	return error;
}

static ssize_t edt_ft5x06_update_fw_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct edt_ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&tsdata->mutex);

	ret = edt_ft5x06_i2c_fw_update(tsdata);

	mutex_unlock(&tsdata->mutex);
	return ret ?: count;
}

static ssize_t fw_version_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct edt_ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	int ret = edt_ft5x06_register_read(tsdata, EDT_ID);
	if (ret < 0)
		return ret;
	else
		return sprintf(buf, "0x%02x\n", ret);
}

/* m06, m09: range 0-31, m12: range 0-5 */
static EDT_ATTR(gain, S_IWUSR | S_IRUGO, WORK_REGISTER_GAIN,
		M09_REGISTER_GAIN, EV_REGISTER_GAIN, 0, 31);
/* m06, m09: range 0-31, m12: range 0-16 */
static EDT_ATTR(offset, S_IWUSR | S_IRUGO, WORK_REGISTER_OFFSET,
		M09_REGISTER_OFFSET, NO_REGISTER, 0, 31);
/* m06, m09, m12: no supported, ev_ft: range 0-80 */
static EDT_ATTR(offset_x, S_IWUSR | S_IRUGO, NO_REGISTER, NO_REGISTER,
		EV_REGISTER_OFFSET_X, 0, 80);
/* m06, m09, m12: no supported, ev_ft: range 0-80 */
static EDT_ATTR(offset_y, S_IWUSR | S_IRUGO, NO_REGISTER, NO_REGISTER,
		EV_REGISTER_OFFSET_Y, 0, 80);
/* m06: range 20 to 80, m09: range 0 to 30, m12: range 1 to 255... */
static EDT_ATTR(threshold, S_IWUSR | S_IRUGO, WORK_REGISTER_THRESHOLD,
		M09_REGISTER_THRESHOLD, EV_REGISTER_THRESHOLD, 0, 255);
/* m06: range 3 to 14, m12: (0x64: 100Hz) */
static EDT_ATTR(report_rate, S_IWUSR | S_IRUGO, WORK_REGISTER_REPORT_RATE,
		NO_REGISTER, NO_REGISTER, 0, 255);

static DEVICE_ATTR(update_fw, S_IWUSR, NULL, edt_ft5x06_update_fw_store);
static DEVICE_ATTR_RO(fw_version);

static struct attribute *edt_ft5x06_attrs[] = {
	&edt_ft5x06_attr_gain.dattr.attr,
	&edt_ft5x06_attr_offset.dattr.attr,
	&edt_ft5x06_attr_offset_x.dattr.attr,
	&edt_ft5x06_attr_offset_y.dattr.attr,
	&edt_ft5x06_attr_threshold.dattr.attr,
	&edt_ft5x06_attr_report_rate.dattr.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_fw_version.attr,
	NULL
};

static const struct attribute_group edt_ft5x06_attr_group = {
	.attrs = edt_ft5x06_attrs,
};

static void edt_ft5x06_restore_reg_parameters(struct edt_ft5x06_ts_data *tsdata)
{
	struct edt_reg_addr *reg_addr = &tsdata->reg_addr;

	edt_ft5x06_register_write(tsdata, reg_addr->reg_threshold,
				  tsdata->threshold);
	edt_ft5x06_register_write(tsdata, reg_addr->reg_gain,
				  tsdata->gain);
	if (reg_addr->reg_offset != NO_REGISTER)
		edt_ft5x06_register_write(tsdata, reg_addr->reg_offset,
					  tsdata->offset);
	if (reg_addr->reg_offset_x != NO_REGISTER)
		edt_ft5x06_register_write(tsdata, reg_addr->reg_offset_x,
					  tsdata->offset_x);
	if (reg_addr->reg_offset_y != NO_REGISTER)
		edt_ft5x06_register_write(tsdata, reg_addr->reg_offset_y,
					  tsdata->offset_y);
	if (reg_addr->reg_report_rate != NO_REGISTER)
		edt_ft5x06_register_write(tsdata, reg_addr->reg_report_rate,
				  tsdata->report_rate);

}

#ifdef CONFIG_DEBUG_FS
static int edt_ft5x06_factory_mode(struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int retries = EDT_SWITCH_MODE_RETRIES;
	int ret;
	int error;

	if (tsdata->version != EDT_M06) {
		dev_err(&client->dev,
			"No factory mode support for non-M06 devices\n");
		return -EINVAL;
	}

	disable_irq(client->irq);

	if (!tsdata->raw_buffer) {
		tsdata->raw_bufsize = tsdata->num_x * tsdata->num_y *
				      sizeof(u16);
		tsdata->raw_buffer = kzalloc(tsdata->raw_bufsize, GFP_KERNEL);
		if (!tsdata->raw_buffer) {
			error = -ENOMEM;
			goto err_out;
		}
	}

	/* mode register is 0x3c when in the work mode */
	error = edt_ft5x06_register_write(tsdata, WORK_REGISTER_OPMODE, 0x03);
	if (error) {
		dev_err(&client->dev,
			"failed to switch to factory mode, error %d\n", error);
		goto err_out;
	}

	tsdata->factory_mode = true;
	do {
		mdelay(EDT_SWITCH_MODE_DELAY);
		/* mode register is 0x01 when in factory mode */
		ret = edt_ft5x06_register_read(tsdata, FACTORY_REGISTER_OPMODE);
		if (ret == 0x03)
			break;
	} while (--retries > 0);

	if (retries == 0) {
		dev_err(&client->dev, "not in factory mode after %dms.\n",
			EDT_SWITCH_MODE_RETRIES * EDT_SWITCH_MODE_DELAY);
		error = -EIO;
		goto err_out;
	}

	return 0;

err_out:
	kfree(tsdata->raw_buffer);
	tsdata->raw_buffer = NULL;
	tsdata->factory_mode = false;
	enable_irq(client->irq);

	return error;
}

static int edt_ft5x06_work_mode(struct edt_ft5x06_ts_data *tsdata)
{
	struct i2c_client *client = tsdata->client;
	int retries = EDT_SWITCH_MODE_RETRIES;
	int ret;
	int error;

	/* mode register is 0x01 when in the factory mode */
	error = edt_ft5x06_register_write(tsdata, FACTORY_REGISTER_OPMODE, 0x1);
	if (error) {
		dev_err(&client->dev,
			"failed to switch to work mode, error: %d\n", error);
		return error;
	}

	tsdata->factory_mode = false;

	do {
		mdelay(EDT_SWITCH_MODE_DELAY);
		/* mode register is 0x01 when in factory mode */
		ret = edt_ft5x06_register_read(tsdata, WORK_REGISTER_OPMODE);
		if (ret == 0x01)
			break;
	} while (--retries > 0);

	if (retries == 0) {
		dev_err(&client->dev, "not in work mode after %dms.\n",
			EDT_SWITCH_MODE_RETRIES * EDT_SWITCH_MODE_DELAY);
		tsdata->factory_mode = true;
		return -EIO;
	}

	kfree(tsdata->raw_buffer);
	tsdata->raw_buffer = NULL;

	edt_ft5x06_restore_reg_parameters(tsdata);
	enable_irq(client->irq);

	return 0;
}

static int edt_ft5x06_debugfs_mode_get(void *data, u64 *mode)
{
	struct edt_ft5x06_ts_data *tsdata = data;

	*mode = tsdata->factory_mode;

	return 0;
};

static int edt_ft5x06_debugfs_mode_set(void *data, u64 mode)
{
	struct edt_ft5x06_ts_data *tsdata = data;
	int retval = 0;

	if (mode > 1)
		return -ERANGE;

	mutex_lock(&tsdata->mutex);

	if (mode != tsdata->factory_mode) {
		retval = mode ? edt_ft5x06_factory_mode(tsdata) :
				edt_ft5x06_work_mode(tsdata);
	}

	mutex_unlock(&tsdata->mutex);

	return retval;
};

DEFINE_SIMPLE_ATTRIBUTE(debugfs_mode_fops, edt_ft5x06_debugfs_mode_get,
			edt_ft5x06_debugfs_mode_set, "%llu\n");

static ssize_t edt_ft5x06_debugfs_raw_data_read(struct file *file,
				char __user *buf, size_t count, loff_t *off)
{
	struct edt_ft5x06_ts_data *tsdata = file->private_data;
	struct i2c_client *client = tsdata->client;
	int retries  = EDT_RAW_DATA_RETRIES;
	int val, i, error;
	size_t read = 0;
	int colbytes;
	char wrbuf[3];
	u8 *rdbuf;

	if (*off < 0 || *off >= tsdata->raw_bufsize)
		return 0;

	mutex_lock(&tsdata->mutex);

	if (!tsdata->factory_mode || !tsdata->raw_buffer) {
		error = -EIO;
		goto out;
	}

	error = edt_ft5x06_register_write(tsdata, 0x08, 0x01);
	if (error) {
		dev_dbg(&client->dev,
			"failed to write 0x08 register, error %d\n", error);
		goto out;
	}

	do {
		usleep_range(EDT_RAW_DATA_DELAY, EDT_RAW_DATA_DELAY + 100);
		val = edt_ft5x06_register_read(tsdata, 0x08);
		if (val < 1)
			break;
	} while (--retries > 0);

	if (val < 0) {
		error = val;
		dev_dbg(&client->dev,
			"failed to read 0x08 register, error %d\n", error);
		goto out;
	}

	if (retries == 0) {
		dev_dbg(&client->dev,
			"timed out waiting for register to settle\n");
		error = -ETIMEDOUT;
		goto out;
	}

	rdbuf = tsdata->raw_buffer;
	colbytes = tsdata->num_y * sizeof(u16);

	wrbuf[0] = 0xf5;
	wrbuf[1] = 0x0e;
	for (i = 0; i < tsdata->num_x; i++) {
		wrbuf[2] = i;  /* column index */
		error = edt_ft5x06_ts_readwrite(tsdata->client,
						sizeof(wrbuf), wrbuf,
						colbytes, rdbuf);
		if (error)
			goto out;

		rdbuf += colbytes;
	}

	read = min_t(size_t, count, tsdata->raw_bufsize - *off);
	if (copy_to_user(buf, tsdata->raw_buffer + *off, read)) {
		error = -EFAULT;
		goto out;
	}

	*off += read;
out:
	mutex_unlock(&tsdata->mutex);
	return error ?: read;
};

static const struct file_operations debugfs_raw_data_fops = {
	.open = simple_open,
	.read = edt_ft5x06_debugfs_raw_data_read,
};

static void edt_ft5x06_ts_prepare_debugfs(struct edt_ft5x06_ts_data *tsdata,
					  const char *debugfs_name)
{
	tsdata->debug_dir = debugfs_create_dir(debugfs_name, NULL);

	debugfs_create_u16("num_x", S_IRUSR, tsdata->debug_dir, &tsdata->num_x);
	debugfs_create_u16("num_y", S_IRUSR, tsdata->debug_dir, &tsdata->num_y);

	debugfs_create_file("mode", S_IRUSR | S_IWUSR,
			    tsdata->debug_dir, tsdata, &debugfs_mode_fops);
	debugfs_create_file("raw_data", S_IRUSR,
			    tsdata->debug_dir, tsdata, &debugfs_raw_data_fops);
}

static void edt_ft5x06_ts_teardown_debugfs(struct edt_ft5x06_ts_data *tsdata)
{
	debugfs_remove_recursive(tsdata->debug_dir);
	kfree(tsdata->raw_buffer);
}

#else

static int edt_ft5x06_factory_mode(struct edt_ft5x06_ts_data *tsdata)
{
	return -ENOSYS;
}

static void edt_ft5x06_ts_prepare_debugfs(struct edt_ft5x06_ts_data *tsdata,
					  const char *debugfs_name)
{
}

static void edt_ft5x06_ts_teardown_debugfs(struct edt_ft5x06_ts_data *tsdata)
{
}

#endif /* CONFIG_DEBUGFS */

static int edt_ft5x06_ts_identify(struct i2c_client *client,
					struct edt_ft5x06_ts_data *tsdata,
					char *fw_version)
{
	u8 rdbuf[EDT_NAME_LEN];
	char *p;
	int error;
	char *model_name = tsdata->name;
	int i;

	/* see what we find if we assume it is a M06 *
	 * if we get less than EDT_NAME_LEN, we don't want
	 * to have garbage in there
	 */
	memset(rdbuf, 0, sizeof(rdbuf));
	error = edt_ft5x06_ts_readwrite(client, 1, "\xBB",
					EDT_NAME_LEN - 1, rdbuf);
	if (error)
		return error;

	/* Probe content for something consistent.
	 * M06 starts with a response byte, M12 gives the data directly.
	 * M09/Generic does not provide model number information.
	 */
	if (!strncasecmp(rdbuf + 1, "EP0", 3)) {
		tsdata->version = EDT_M06;

		/* remove last '$' end marker */
		rdbuf[EDT_NAME_LEN - 1] = '\0';
		if (rdbuf[EDT_NAME_LEN - 2] == '$')
			rdbuf[EDT_NAME_LEN - 2] = '\0';

		/* look for Model/Version separator */
		p = strchr(rdbuf, '*');
		if (p)
			*p++ = '\0';
		strlcpy(model_name, rdbuf + 1, EDT_NAME_LEN);
		strlcpy(fw_version, p ? p : "", EDT_NAME_LEN);
	} else if (!strncasecmp(rdbuf, "EP0", 3)) {
		tsdata->version = EDT_M12;

		/* remove last '$' end marker */
		rdbuf[EDT_NAME_LEN - 2] = '\0';
		if (rdbuf[EDT_NAME_LEN - 3] == '$')
			rdbuf[EDT_NAME_LEN - 3] = '\0';

		/* look for Model/Version separator */
		p = strchr(rdbuf, '*');
		if (p)
			*p++ = '\0';
		strlcpy(model_name, rdbuf, EDT_NAME_LEN);
		strlcpy(fw_version, p ? p : "", EDT_NAME_LEN);
	} else {
		/* If it is not an EDT M06/M12 touchscreen, then the model
		 * detection is a bit hairy. The different ft5x06
		 * firmares around don't reliably implement the
		 * identification registers. Well, we'll take a shot.
		 *
		 * The main difference between generic focaltec based
		 * touches and EDT M09 is that we know how to retrieve
		 * the max coordinates for the latter.
		 */
		tsdata->version = GENERIC_FT;

		error = edt_ft5x06_ts_readwrite(client, 1, "\xA6",
						2, rdbuf);
		if (error)
			return error;

		strlcpy(fw_version, rdbuf, 2);

		error = edt_ft5x06_ts_readwrite(client, 1, "\xA8",
						1, rdbuf);
		if (error)
			return error;

		/* This "model identification" is not exact. Unfortunately
		 * not all firmwares for the ft5x06 put useful values in
		 * the identification registers.
		 */
		switch (rdbuf[0]) {
		case 0x11:   /* EDT EP0110M09 */
		case 0x35:   /* EDT EP0350M09 */
		case 0x43:   /* EDT EP0430M09 */
		case 0x50:   /* EDT EP0500M09 */
		case 0x57:   /* EDT EP0570M09 */
		case 0x70:   /* EDT EP0700M09 */
			tsdata->version = EDT_M09;
			snprintf(model_name, EDT_NAME_LEN, "EP0%i%i0M09",
				rdbuf[0] >> 4, rdbuf[0] & 0x0F);
			break;
		case 0xa1:   /* EDT EP1010ML00 */
			tsdata->version = EDT_M09;
			snprintf(model_name, EDT_NAME_LEN, "EP%i%i0ML00",
				rdbuf[0] >> 4, rdbuf[0] & 0x0F);
			break;
		case 0x5a:   /* Solomon Goldentek Display */
			snprintf(model_name, EDT_NAME_LEN, "GKTW50SCED1R0");
			break;
		case 0x59:  /* Evervision Display with FT5xx6 TS */
			tsdata->version = EV_FT;
			error = edt_ft5x06_ts_readwrite(client, 1, "\x53",
							1, rdbuf);
			if (error)
				return error;
			strlcpy(fw_version, rdbuf, 1);
			snprintf(model_name, EDT_NAME_LEN,
				 "EVERVISION-FT5726NEi");
			break;
		default:
			snprintf(model_name, EDT_NAME_LEN,
				 "generic ft5x06 (%02x)",
				 rdbuf[0]);

			for (i = 0; i < ARRAY_SIZE(edt_fw_params); i++) {
				if (edt_fw_params[i].model == rdbuf[0]) {
					tsdata->fw_param = &edt_fw_params[i];
					break;
				}
			}
			break;
		}
	}

	return 0;
}

static void edt_ft5x06_ts_get_defaults(struct device *dev,
				       struct edt_ft5x06_ts_data *tsdata)
{
	struct edt_reg_addr *reg_addr = &tsdata->reg_addr;
	u32 val;
	int error;

	error = device_property_read_u32(dev, "threshold", &val);
	if (!error) {
		edt_ft5x06_register_write(tsdata, reg_addr->reg_threshold, val);
		tsdata->threshold = val;
	}

	error = device_property_read_u32(dev, "gain", &val);
	if (!error) {
		edt_ft5x06_register_write(tsdata, reg_addr->reg_gain, val);
		tsdata->gain = val;
	}

	error = device_property_read_u32(dev, "offset", &val);
	if (!error) {
		if (reg_addr->reg_offset != NO_REGISTER)
			edt_ft5x06_register_write(tsdata,
						  reg_addr->reg_offset, val);
		tsdata->offset = val;
	}

	error = device_property_read_u32(dev, "offset-x", &val);
	if (!error) {
		if (reg_addr->reg_offset_x != NO_REGISTER)
			edt_ft5x06_register_write(tsdata,
						  reg_addr->reg_offset_x, val);
		tsdata->offset_x = val;
	}

	error = device_property_read_u32(dev, "offset-y", &val);
	if (!error) {
		if (reg_addr->reg_offset_y != NO_REGISTER)
			edt_ft5x06_register_write(tsdata,
						  reg_addr->reg_offset_y, val);
		tsdata->offset_y = val;
	}
}

static void edt_ft5x06_ts_get_parameters(struct edt_ft5x06_ts_data *tsdata)
{
	struct edt_reg_addr *reg_addr = &tsdata->reg_addr;

	tsdata->threshold = edt_ft5x06_register_read(tsdata,
						     reg_addr->reg_threshold);
	tsdata->gain = edt_ft5x06_register_read(tsdata, reg_addr->reg_gain);
	if (reg_addr->reg_offset != NO_REGISTER)
		tsdata->offset =
			edt_ft5x06_register_read(tsdata, reg_addr->reg_offset);
	if (reg_addr->reg_offset_x != NO_REGISTER)
		tsdata->offset_x = edt_ft5x06_register_read(tsdata,
						reg_addr->reg_offset_x);
	if (reg_addr->reg_offset_y != NO_REGISTER)
		tsdata->offset_y = edt_ft5x06_register_read(tsdata,
						reg_addr->reg_offset_y);
	if (reg_addr->reg_report_rate != NO_REGISTER)
		tsdata->report_rate = edt_ft5x06_register_read(tsdata,
						reg_addr->reg_report_rate);
	tsdata->num_x = EDT_DEFAULT_NUM_X;
	if (reg_addr->reg_num_x != NO_REGISTER)
		tsdata->num_x = edt_ft5x06_register_read(tsdata,
							 reg_addr->reg_num_x);
	tsdata->num_y = EDT_DEFAULT_NUM_Y;
	if (reg_addr->reg_num_y != NO_REGISTER)
		tsdata->num_y = edt_ft5x06_register_read(tsdata,
							 reg_addr->reg_num_y);
}

static void edt_ft5x06_ts_set_regs(struct edt_ft5x06_ts_data *tsdata)
{
	struct edt_reg_addr *reg_addr = &tsdata->reg_addr;

	switch (tsdata->version) {
	case EDT_M06:
		reg_addr->reg_threshold = WORK_REGISTER_THRESHOLD;
		reg_addr->reg_report_rate = WORK_REGISTER_REPORT_RATE;
		reg_addr->reg_gain = WORK_REGISTER_GAIN;
		reg_addr->reg_offset = WORK_REGISTER_OFFSET;
		reg_addr->reg_offset_x = NO_REGISTER;
		reg_addr->reg_offset_y = NO_REGISTER;
		reg_addr->reg_num_x = WORK_REGISTER_NUM_X;
		reg_addr->reg_num_y = WORK_REGISTER_NUM_Y;
		break;

	case EDT_M09:
	case EDT_M12:
		reg_addr->reg_threshold = M09_REGISTER_THRESHOLD;
		reg_addr->reg_report_rate = NO_REGISTER;
		reg_addr->reg_gain = M09_REGISTER_GAIN;
		reg_addr->reg_offset = M09_REGISTER_OFFSET;
		reg_addr->reg_offset_x = NO_REGISTER;
		reg_addr->reg_offset_y = NO_REGISTER;
		reg_addr->reg_num_x = M09_REGISTER_NUM_X;
		reg_addr->reg_num_y = M09_REGISTER_NUM_Y;
		break;

	case EV_FT:
		reg_addr->reg_threshold = EV_REGISTER_THRESHOLD;
		reg_addr->reg_report_rate = NO_REGISTER;
		reg_addr->reg_gain = EV_REGISTER_GAIN;
		reg_addr->reg_offset = NO_REGISTER;
		reg_addr->reg_offset_x = EV_REGISTER_OFFSET_X;
		reg_addr->reg_offset_y = EV_REGISTER_OFFSET_Y;
		reg_addr->reg_num_x = NO_REGISTER;
		reg_addr->reg_num_y = NO_REGISTER;
		break;

	case GENERIC_FT:
		/* this is a guesswork */
		reg_addr->reg_threshold = M09_REGISTER_THRESHOLD;
		reg_addr->reg_report_rate = NO_REGISTER;
		reg_addr->reg_gain = M09_REGISTER_GAIN;
		reg_addr->reg_offset = M09_REGISTER_OFFSET;
		reg_addr->reg_offset_x = NO_REGISTER;
		reg_addr->reg_offset_y = NO_REGISTER;
		reg_addr->reg_num_x = NO_REGISTER;
		reg_addr->reg_num_y = NO_REGISTER;
		break;
	}
}

static void edt_ft5x06_disable_regulators(void *arg)
{
	struct edt_ft5x06_ts_data *data = arg;

	regulator_disable(data->vcc);
	regulator_disable(data->iovcc);
}

static int edt_ft5x06_ts_probe(struct i2c_client *client,
					 const struct i2c_device_id *id)
{
	const struct edt_i2c_chip_data *chip_data;
	struct edt_ft5x06_ts_data *tsdata;
	u8 buf[2] = { 0xfc, 0x00 };
	struct input_dev *input;
	unsigned long irq_flags;
	int error;
	char fw_version[EDT_NAME_LEN];

	dev_dbg(&client->dev, "probing for EDT FT5x06 I2C\n");

	tsdata = devm_kzalloc(&client->dev, sizeof(*tsdata), GFP_KERNEL);
	if (!tsdata) {
		dev_err(&client->dev, "failed to allocate driver data.\n");
		return -ENOMEM;
	}

	chip_data = device_get_match_data(&client->dev);
	if (!chip_data)
		chip_data = (const struct edt_i2c_chip_data *)id->driver_data;
	if (!chip_data || !chip_data->max_support_points) {
		dev_err(&client->dev, "invalid or missing chip data\n");
		return -EINVAL;
	}

	tsdata->max_support_points = chip_data->max_support_points;

	tsdata->vcc = devm_regulator_get(&client->dev, "vcc");
	if (IS_ERR(tsdata->vcc)) {
		error = PTR_ERR(tsdata->vcc);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
				"failed to request regulator: %d\n", error);
		return error;
	}

	tsdata->iovcc = devm_regulator_get(&client->dev, "iovcc");
	if (IS_ERR(tsdata->iovcc)) {
		error = PTR_ERR(tsdata->iovcc);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
				"failed to request iovcc regulator: %d\n", error);
		return error;
	}

	error = regulator_enable(tsdata->iovcc);
	if (error < 0) {
		dev_err(&client->dev, "failed to enable iovcc: %d\n", error);
		return error;
	}

	/* Delay enabling VCC for > 10us (T_ivd) after IOVCC */
	usleep_range(10, 100);

	error = regulator_enable(tsdata->vcc);
	if (error < 0) {
		dev_err(&client->dev, "failed to enable vcc: %d\n", error);
		regulator_disable(tsdata->iovcc);
		return error;
	}

	error = devm_add_action_or_reset(&client->dev,
					 edt_ft5x06_disable_regulators,
					 tsdata);
	if (error)
		return error;

	tsdata->reset_gpio = devm_gpiod_get_optional(&client->dev,
						     "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(tsdata->reset_gpio)) {
		error = PTR_ERR(tsdata->reset_gpio);
		dev_err(&client->dev,
			"Failed to request GPIO reset pin, error %d\n", error);
		return error;
	}

	tsdata->wake_gpio = devm_gpiod_get_optional(&client->dev,
						    "wake", GPIOD_OUT_LOW);
	if (IS_ERR(tsdata->wake_gpio)) {
		error = PTR_ERR(tsdata->wake_gpio);
		dev_err(&client->dev,
			"Failed to request GPIO wake pin, error %d\n", error);
		return error;
	}

	/*
	 * Check which sleep modes we can support. Power-off requieres the
	 * reset-pin to ensure correct power-down/power-up behaviour. Start with
	 * the EDT_PMODE_POWEROFF test since this is the deepest possible sleep
	 * mode.
	 */
	if (tsdata->reset_gpio)
		tsdata->suspend_mode = EDT_PMODE_POWEROFF;
	else if (tsdata->wake_gpio)
		tsdata->suspend_mode = EDT_PMODE_HIBERNATE;
	else
		tsdata->suspend_mode = EDT_PMODE_NOT_SUPPORTED;

	if (tsdata->wake_gpio) {
		usleep_range(5000, 6000);
		gpiod_set_value_cansleep(tsdata->wake_gpio, 1);
	}

	if (tsdata->reset_gpio) {
		usleep_range(5000, 6000);
		gpiod_set_value_cansleep(tsdata->reset_gpio, 0);
		msleep(300);
	}

	input = devm_input_allocate_device(&client->dev);
	if (!input) {
		dev_err(&client->dev, "failed to allocate input device.\n");
		return -ENOMEM;
	}

	mutex_init(&tsdata->mutex);
	tsdata->client = client;
	tsdata->input = input;
	tsdata->factory_mode = false;

	error = edt_ft5x06_ts_identify(client, tsdata, fw_version);
	if (error) {
		dev_err(&client->dev, "touchscreen probe failed\n");
		return error;
	}

	/*
	 * Dummy read access. EP0700MLP1 returns bogus data on the first
	 * register read access and ignores writes.
	 */
	edt_ft5x06_ts_readwrite(tsdata->client, 2, buf, 2, buf);

	edt_ft5x06_ts_set_regs(tsdata);
	edt_ft5x06_ts_get_defaults(&client->dev, tsdata);
	edt_ft5x06_ts_get_parameters(tsdata);

	dev_dbg(&client->dev,
		"Model \"%s\", Rev. \"%s\", %dx%d sensors\n",
		tsdata->name, fw_version, tsdata->num_x, tsdata->num_y);

	input->name = tsdata->name;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	input_set_abs_params(input, ABS_MT_POSITION_X,
			     0, tsdata->num_x * 64 - 1, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y,
			     0, tsdata->num_y * 64 - 1, 0, 0);

	touchscreen_parse_properties(input, true, &tsdata->prop);

	error = input_mt_init_slots(input, tsdata->max_support_points,
				INPUT_MT_DIRECT);
	if (error) {
		dev_err(&client->dev, "Unable to init MT slots.\n");
		return error;
	}

	i2c_set_clientdata(client, tsdata);

	irq_flags = irq_get_trigger_type(client->irq);
	if (irq_flags == IRQF_TRIGGER_NONE)
		irq_flags = IRQF_TRIGGER_FALLING;
	irq_flags |= IRQF_ONESHOT;

	error = devm_request_threaded_irq(&client->dev, client->irq,
					NULL, edt_ft5x06_ts_isr, irq_flags,
					client->name, tsdata);
	if (error) {
		dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
		return error;
	}

	error = devm_device_add_group(&client->dev, &edt_ft5x06_attr_group);
	if (error)
		return error;

	error = input_register_device(input);
	if (error)
		return error;

	edt_ft5x06_ts_prepare_debugfs(tsdata, dev_driver_string(&client->dev));

	dev_dbg(&client->dev,
		"EDT FT5x06 initialized: IRQ %d, WAKE pin %d, Reset pin %d.\n",
		client->irq,
		tsdata->wake_gpio ? desc_to_gpio(tsdata->wake_gpio) : -1,
		tsdata->reset_gpio ? desc_to_gpio(tsdata->reset_gpio) : -1);

	return 0;
}

static int edt_ft5x06_ts_remove(struct i2c_client *client)
{
	struct edt_ft5x06_ts_data *tsdata = i2c_get_clientdata(client);

	edt_ft5x06_ts_teardown_debugfs(tsdata);

	return 0;
}

static int __maybe_unused edt_ft5x06_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct edt_ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	struct gpio_desc *reset_gpio = tsdata->reset_gpio;
	int ret;

	if (device_may_wakeup(dev))
		return 0;

	if (tsdata->suspend_mode == EDT_PMODE_NOT_SUPPORTED)
		return 0;

	/* Enter hibernate mode. */
	ret = edt_ft5x06_register_write(tsdata, PMOD_REGISTER_OPMODE,
					PMOD_REGISTER_HIBERNATE);
	if (ret)
		dev_warn(dev, "Failed to set hibernate mode\n");

	if (tsdata->suspend_mode == EDT_PMODE_HIBERNATE)
		return 0;

	/*
	 * Power-off according the datasheet. Cut the power may leaf the irq
	 * line in an undefined state depending on the host pull resistor
	 * settings. Disable the irq to avoid adjusting each host till the
	 * device is back in a full functional state.
	 */
	disable_irq(tsdata->client->irq);

	gpiod_set_value_cansleep(reset_gpio, 1);
	usleep_range(1000, 2000);

	ret = regulator_disable(tsdata->vcc);
	if (ret)
		dev_warn(dev, "Failed to disable vcc\n");
	ret = regulator_disable(tsdata->iovcc);
	if (ret)
		dev_warn(dev, "Failed to disable iovcc\n");

	return 0;
}

static int __maybe_unused edt_ft5x06_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct edt_ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	int ret = 0;

	if (device_may_wakeup(dev))
		return 0;

	if (tsdata->suspend_mode == EDT_PMODE_NOT_SUPPORTED)
		return 0;

	if (tsdata->suspend_mode == EDT_PMODE_POWEROFF) {
		struct gpio_desc *reset_gpio = tsdata->reset_gpio;

		/*
		 * We can't check if the regulator is a dummy or a real
		 * regulator. So we need to specify the 5ms reset time (T_rst)
		 * here instead of the 100us T_rtp time. We also need to wait
		 * 300ms in case it was a real supply and the power was cutted
		 * of. Toggle the reset pin is also a way to exit the hibernate
		 * mode.
		 */
		gpiod_set_value_cansleep(reset_gpio, 1);
		usleep_range(5000, 6000);

		ret = regulator_enable(tsdata->iovcc);
		if (ret) {
			dev_err(dev, "Failed to enable iovcc\n");
			return ret;
		}

		/* Delay enabling VCC for > 10us (T_ivd) after IOVCC */
		usleep_range(10, 100);

		ret = regulator_enable(tsdata->vcc);
		if (ret) {
			dev_err(dev, "Failed to enable vcc\n");
			regulator_disable(tsdata->iovcc);
			return ret;
		}

		usleep_range(1000, 2000);
		gpiod_set_value_cansleep(reset_gpio, 0);
		msleep(300);

		edt_ft5x06_restore_reg_parameters(tsdata);
		enable_irq(tsdata->client->irq);

		if (tsdata->factory_mode)
			ret = edt_ft5x06_factory_mode(tsdata);
	} else {
		struct gpio_desc *wake_gpio = tsdata->wake_gpio;

		gpiod_set_value_cansleep(wake_gpio, 0);
		usleep_range(5000, 6000);
		gpiod_set_value_cansleep(wake_gpio, 1);
	}


	return ret;
}

static SIMPLE_DEV_PM_OPS(edt_ft5x06_ts_pm_ops,
			 edt_ft5x06_ts_suspend, edt_ft5x06_ts_resume);

static const struct edt_i2c_chip_data edt_ft5x06_data = {
	.max_support_points = 5,
};

static const struct edt_i2c_chip_data edt_ft5506_data = {
	.max_support_points = 10,
};

static const struct edt_i2c_chip_data edt_ft6236_data = {
	.max_support_points = 2,
};

static const struct i2c_device_id edt_ft5x06_ts_id[] = {
	{ .name = "edt-ft5x06", .driver_data = (long)&edt_ft5x06_data },
	{ .name = "edt-ft5506", .driver_data = (long)&edt_ft5506_data },
	{ .name = "ev-ft5726", .driver_data = (long)&edt_ft5506_data },
	/* Note no edt- prefix for compatibility with the ft6236.c driver */
	{ .name = "ft6236", .driver_data = (long)&edt_ft6236_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, edt_ft5x06_ts_id);

static const struct of_device_id edt_ft5x06_of_match[] = {
	{ .compatible = "edt,edt-ft5206", .data = &edt_ft5x06_data },
	{ .compatible = "edt,edt-ft5306", .data = &edt_ft5x06_data },
	{ .compatible = "edt,edt-ft5406", .data = &edt_ft5x06_data },
	{ .compatible = "edt,edt-ft5506", .data = &edt_ft5506_data },
	{ .compatible = "evervision,ev-ft5726", .data = &edt_ft5506_data },
	/* Note focaltech vendor prefix for compatibility with ft6236.c */
	{ .compatible = "focaltech,ft6236", .data = &edt_ft6236_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, edt_ft5x06_of_match);

static struct i2c_driver edt_ft5x06_ts_driver = {
	.driver = {
		.name = "edt_ft5x06",
		.of_match_table = edt_ft5x06_of_match,
		.pm = &edt_ft5x06_ts_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.id_table = edt_ft5x06_ts_id,
	.probe    = edt_ft5x06_ts_probe,
	.remove   = edt_ft5x06_ts_remove,
};

module_i2c_driver(edt_ft5x06_ts_driver);

MODULE_AUTHOR("Simon Budig <simon.budig@kernelconcepts.de>");
MODULE_DESCRIPTION("EDT FT5x06 I2C Touchscreen Driver");
MODULE_LICENSE("GPL v2");
