/*
 * Driver for Techwell TW6864/68 based DVR cards
 *
 * (c) 2009-10 liran <jlee@techwellinc.com.cn> [Techwell China]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include "tw686x.h"
#include "tw686x-reg.h"

#define TW6864_I2C_SCLK_BIT			BIT(0)
#define TW6864_I2C_SDA_DATA_BIT		BIT(1)
#define TW6864_I2C_SCLK_CTRL_BIT	BIT(16)  //1:Input,0=Output
#define TW6864_I2C_SDA_CTRL_BIT		BIT(17)  //1:Input,0=Output

static bool tw686x_i2c_s_scl( struct i2c_sw *i2c, TLevel scl )
{
    struct tw686x_chip *chip = (struct tw686x_chip *)i2c->chip;
    struct tw686x_i2c *i2c_686x = &chip->i2c_686x;

	i2c_686x->control_word &= ~TW6864_I2C_SCLK_CTRL_BIT;
	if(scl == LevelHi) {
		i2c_686x->control_word |= TW6864_I2C_SCLK_BIT;
	}
	else {
		i2c_686x->control_word &= ~TW6864_I2C_SCLK_BIT;
	}

	tw_write(TW6864_R_GPIO_REG, i2c_686x->control_word);

	return i2csw_s_scl(&i2c_686x->i2c, scl);
}

static bool tw686x_i2c_s_sda( struct i2c_sw *i2c, TLevel sda )
{
    struct tw686x_chip *chip = (struct tw686x_chip *)i2c->chip;
    struct tw686x_i2c *i2c_686x = &chip->i2c_686x;

	i2c_686x->control_word &= ~TW6864_I2C_SDA_CTRL_BIT;
	if(sda == LevelHi) {
		i2c_686x->control_word |= TW6864_I2C_SDA_DATA_BIT;
	}
	else {
		i2c_686x->control_word &= ~TW6864_I2C_SDA_DATA_BIT;
	}

	tw_write(TW6864_R_GPIO_REG, i2c_686x->control_word);

	return i2csw_s_sda(&i2c_686x->i2c, sda);
}

static int tw686x_i2c_g_scl( struct i2c_sw * i2c )
{
    struct tw686x_chip *chip = (struct tw686x_chip *)i2c->chip;

	return (tw_read(TW6864_R_GPIO_REG) & TW6864_I2C_SCLK_BIT);
}

static int tw686x_i2c_g_sda( struct i2c_sw * i2c )
{
    struct tw686x_chip *chip = (struct tw686x_chip *)i2c->chip;
    struct tw686x_i2c *i2c_686x = &chip->i2c_686x;

	i2c_686x->control_word |= TW6864_I2C_SDA_CTRL_BIT;
	tw_write(TW6864_R_GPIO_REG, i2c_686x->control_word);

	return ((tw_read(TW6864_R_GPIO_REG) & TW6864_I2C_SDA_DATA_BIT)>>1);
}

static int i2c_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg *msgs, int num)
{
	struct tw686x_chip *chip = i2c_adap->algo_data;
	struct i2c_sw       *i2c = &chip->i2c_686x.i2c;
	int i, retval = 0;

	dprintk(1, chip, "%s(num = %d)\n", __func__, num);

	for (i = 0 ; i < num; i++) {
		dprintk(1, chip, "%s(num = %d) addr = 0x%02x  len = 0x%x\n",
			__func__, num, msgs[i].addr, msgs[i].len);
		if (msgs[i].flags & I2C_M_RD) {
			/* read */
			retval = i2csw_read_bytes(i2c, (u8*)&msgs[i].addr, 1, msgs[i].buf, msgs[i].len);
		} else {
			/* write */
			retval = i2csw_write_bytes(i2c, msgs[i].buf, msgs[i].len);
		}
		if (!retval) {
		    retval = -i2c->err_code;
			goto err;
		}
	}
	return num;

 err:
	return retval;
}

static u32 tw686x_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL | I2C_FUNC_I2C;
}

static struct i2c_algorithm tw686x_i2c_algo_template = {
	.master_xfer	= i2c_xfer,
	.functionality	= tw686x_functionality,
};

static struct i2c_adapter tw686x_i2c_adap_template = {
	.name              = "tw6869",
	.owner             = THIS_MODULE,
/*	.id                = I2C_SW_TW686X, */
	.algo              = &tw686x_i2c_algo_template,
};

static struct i2c_client tw686x_client_template = {
	.name	= "tw6869 internal",
};

static struct i2c_sw tw686x_i2csw_template = {
    .s_scl = tw686x_i2c_s_scl,
    .s_sda = tw686x_i2c_s_sda,
    .g_scl = tw686x_i2c_g_scl,
    .g_sda = tw686x_i2c_g_sda,
};

static int tw686x_i2c_init( struct tw686x_chip *chip )
{
    struct tw686x_i2c *i2c_686x = &chip->i2c_686x;

    i2c_686x->i2c = tw686x_i2csw_template;
	i2c_686x->i2c.chip = chip;

	i2c_686x->control_word = tw_read(TW6864_R_GPIO_REG);
	i2c_686x->control_word &= ~TW6864_I2C_SCLK_CTRL_BIT;
	i2c_686x->control_word &= ~TW6864_I2C_SDA_CTRL_BIT;
	i2c_686x->control_word |= TW6864_I2C_SCLK_BIT;
	i2c_686x->control_word |= TW6864_I2C_SDA_DATA_BIT;
	tw_write(TW6864_R_GPIO_REG, i2c_686x->control_word);

    i2csw_init( &i2c_686x->i2c, 400000);//2000000);

    return 0;
}

/* init + register i2c algo-bit adapter */
int tw686x_i2c_register(struct tw686x_chip *chip)
{
//    u8 buf;
//    struct i2c_board_info info;
//    struct i2c_client *c;
//    const unsigned short addr_list[] = {
//        TW2864_R_I2C_ADDR0, I2C_CLIENT_END
//    };

	dprintk(1, chip, "%s()\n", __func__);

	chip->i2c_adap = tw686x_i2c_adap_template;
	chip->i2c_adap.dev.parent = &chip->pci->dev;
	strcpy(chip->i2c_adap.name, chip->name);
	chip->i2c_adap.algo_data = chip;
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	i2c_set_adapdata(&chip->i2c_adap, &chip->v4l2_dev);
#else
	i2c_set_adapdata(&chip->i2c_adap, chip);
#endif
	i2c_add_adapter(&chip->i2c_adap);

	chip->i2c_client = tw686x_client_template;
	chip->i2c_client.adapter = &chip->i2c_adap;
	chip->i2c_client.addr = TW2864_R_I2C_ADDR0;

    tw686x_i2c_init( chip );

//    if( i2c_master_recv(&chip->i2c_client, &buf, 0)<0 ) {
//        printk("%s: no tw286x found\n", chip->name);
//    }
//    else {
//        printk("%s: found tw286x\n", chip->name);
//    }
//
//    memset(&info, 0, sizeof(struct i2c_board_info));
//    strlcpy(info.type, "tw286x", I2C_NAME_SIZE);
//    c = i2c_new_probed_device(&chip->i2c_adap, &info, addr_list);
//	dprintk(1, chip, "%s() client=%p\n", __func__, c);

	return 0;
}

int tw686x_i2c_unregister(struct tw686x_chip* chip)
{
	i2c_del_adapter(&chip->i2c_adap);
	return 0;
}
