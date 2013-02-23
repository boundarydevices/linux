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

#define I2C_WRITE         0         // write operation
#define I2C_READ          1         // read operation

#define I2CDIV_MAX        15        // I2C maximum divider

#define TIMEOUT           500       // timeout value (500ms) to wait for an I2C operation
                                    // passing 3 values to I2C takes ~450ms

typedef enum { LevelLow, LevelHi } TLevel;
enum {
	I2CERR_OK        = 0,     // no error
	I2CERR_INIT      = 1,     // error in initialization
	I2CERR_MODE      = 2,     // invalid I2C mode (must be either HW or SW)
	I2CERR_NOACK     = 3,     // no ACK received from slave
	I2CERR_TIMEOUT   = 4,     // timeout error
	I2CERR_SCL       = 5,     // unable to change SCL line
	I2CERR_SDA       = 6,     // unable to change SDA line
	I2CERR_NODEVICE	 = 9,	  // the address is error -- for tuner
};

struct i2c_sw {
    void*   chip;
	u32		cycle;       // software control of frequency
	int     err_code;
	spinlock_t slock;

	bool (*s_scl)(struct i2c_sw *i2c, TLevel);
    bool (*s_sda)(struct i2c_sw *i2c, TLevel);
    int  (*g_scl)(struct i2c_sw *i2c);
    int  (*g_sda)(struct i2c_sw *i2c);
};

bool i2csw_s_scl(struct i2c_sw *i2c, TLevel scl);
bool i2csw_s_sda(struct i2c_sw *i2c, TLevel sda);

bool i2csw_init(struct i2c_sw* i2c, long freq);
bool i2csw_write_bytes(struct i2c_sw *i2c, u8* data, int length);
bool i2csw_write_bytes_nostop(struct i2c_sw *i2c, u8* data, int length);
bool i2csw_read_bytes(struct i2c_sw *i2c, u8 *data_w, int length_w, u8 *data_r, int length_r);
