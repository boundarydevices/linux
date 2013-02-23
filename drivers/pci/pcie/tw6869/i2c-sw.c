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

#include <linux/kernel.h>
#include <linux/time.h>
#include "i2c-sw.h"

static struct timeval i2csw_tv_begin;

//--------------------------------------------------------
// 获得时钟
//--------------------------------------------------------
static u32 tick_count( void )
{
    struct timeval tv_now;
    u64 tv_diff = 0;

    do_gettimeofday( &tv_now );
    tv_diff = 1000000 * ( tv_now.tv_sec - i2csw_tv_begin.tv_sec ) +
              tv_now.tv_usec - i2csw_tv_begin.tv_usec;

	return (((u32)tv_diff)/1000);
}

static void i2csw_bit_delay( struct i2c_sw *i2c )
{
	volatile u32 i;
	for ( i = i2c->cycle; i > 0; i-- ) ;
}

static bool i2csw_start(struct i2c_sw *i2c)
{
	// SendStart - send an I2c start
	// i.e. SDA 1 -> 0 with SCL = 1

	if ( !i2c->s_sda( i2c, LevelHi )  ) { i2c->err_code = I2CERR_SDA; return false; }
	if ( !i2c->s_scl( i2c, LevelHi )  ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SDA; return false; }
	if ( !i2c->s_scl( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SCL; return false; }
	i2c->err_code = I2CERR_OK;

    return true;
}

static bool i2csw_stop(struct i2c_sw *i2c)
{
	// SendStop - sends an I2C stop, releasing the bus.
	// i.e. SDA 0 -> 1 with SCL = 1

	if ( !i2c->s_scl( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelLow ) )	{ i2c->err_code = I2CERR_SDA; return false; }
	if ( !i2c->s_scl( i2c, LevelHi ) )	{ i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelHi )  )	{ i2c->err_code = I2CERR_SDA; return false; }
   	i2c->err_code = I2CERR_OK;

	return true;
}

static bool i2csw_send_ack( struct i2c_sw *i2c )
{
	// Generate ACK signal
	// i.e. SDA = 0 with SCL = 1

	if ( !i2c->s_scl( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SDA; return false; }
	if ( !i2c->s_scl( i2c, LevelHi  ) ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_scl( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelHi  ) ) { i2c->err_code = I2CERR_SDA; return false; }
	i2c->err_code = I2CERR_OK;

	return true;
}

static bool i2csw_send_nack( struct i2c_sw *i2c )
{
	// Generate NACK signal
	// i.e. SDA = 1 with SCL = 1

	if ( !i2c->s_scl( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelHi )  ) { i2c->err_code = I2CERR_SDA; return false; }
	if ( !i2c->s_scl( i2c, LevelHi )  ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_scl( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SDA; return false; }
	i2c->err_code = I2CERR_OK;

	return true;
}

static bool i2csw_wait_ack( struct i2c_sw *i2c )
{
	u32 startTime;
	u32 maxWait = 500; // 500 mSec

	if ( !i2c->s_scl( i2c, LevelLow ) ){ i2c->err_code = I2CERR_SCL; return false; }
	if ( !i2c->s_sda( i2c, LevelHi ) ) { i2c->err_code = I2CERR_SDA; return false; }

	// loop until either ACK or timeout
	startTime = tick_count();

	while (1)
	{
		// SDA pin == 0 means the slave ACK'd
		if ( i2c->g_sda( i2c ) == 0 )
		{
			if ( !i2c->s_scl( i2c, LevelHi ) ) { i2c->err_code = I2CERR_SCL; return false; }
			if ( !i2c->s_scl( i2c, LevelLow ) ){ i2c->err_code = I2CERR_SCL; return false; }
			i2c->err_code = I2CERR_OK;
			return true;
		}

		// timeout?
		if ( tick_count() - startTime > (u32)maxWait )
		{
			i2csw_stop(i2c);
			i2c->err_code = I2CERR_TIMEOUT;
			return false;
		}
	} // while

	return true;
}

static bool i2csw_read(struct i2c_sw *i2c, u8 *value)
{
    u8 mask;

    *value = 0x00;

    // read 8 bits from I2c into Accumulator
    for(mask = 0x80; mask > 0; mask = (u8)(mask >> 1)) {
		if ( !i2c->s_scl( i2c, LevelLow ) ) { i2c->err_code = I2CERR_SCL; return false; }
		if ( !i2c->s_scl( i2c, LevelHi ) )	{ i2c->err_code = I2CERR_SCL; return false; }
		if ( i2c->g_sda( i2c ) == LevelHi ){ *value=(u8)( *value | mask ); }// set the bit
    }
	i2c->err_code = I2CERR_OK;

    return true;
}

static bool i2csw_write(struct i2c_sw *i2c, u8 value)
{
    u8 mask;

    // generate bit patterns by setting SCL and SDA lines
    for(mask = 0x80; mask > 0; mask = (u8)(mask >> 1)) {
        if(!i2c->s_scl( i2c,LevelLow)) {i2c->err_code = I2CERR_SCL; return false;}

        // Send one data bit.
        if(value & mask) { // Put data bit on pin.
            if(!i2c->s_sda( i2c,LevelHi)) {i2c->err_code = I2CERR_SDA; return false;}
        }
        else if(!i2c->s_sda( i2c,LevelLow)) {i2c->err_code = I2CERR_SDA; return false;}

        if(!i2c->s_scl( i2c,LevelHi)) {i2c->err_code = I2CERR_SCL; return false;}
    }

    return i2csw_wait_ack(i2c);
}

bool i2csw_init(struct i2c_sw* i2c, long freq)
{
    u32 elapsed = 0L;
    u32 start;
    volatile u32 i;

    if( !i2c ) {
        return false;
    }

    i2c->cycle = 10000L;    // use a large number to start

    while(elapsed < 5) // loop until delay is long enough for calculation
    {
        i2c->cycle *= 10;
        start = tick_count();

        for(i = i2c->cycle; i > 0; i--) ;
        elapsed = tick_count() - start;
    }

    if(freq > 1)
        i2c->cycle = i2c->cycle / elapsed * 1000L / freq / 2;

	spin_lock_init(&i2c->slock);

	i2c->err_code = I2CERR_OK;

	return true;
}

bool i2csw_s_scl(struct i2c_sw *i2c, TLevel scl)
{
    // loop until SCL really changes or timeout
    u32 maxWait = 500;                  // 500 mSec
    u32 startTime = tick_count();

    while(1)
    {
        // has SCL changed yet?
        if(i2c->g_scl(i2c) == scl)
            break;

        // timeout?
        if(tick_count() - startTime > (u32)maxWait)
        {
            i2c->err_code = I2CERR_TIMEOUT;
            return false;
        }
    }

    i2csw_bit_delay(i2c);
	i2c->err_code = I2CERR_OK;

    return true;
}

bool i2csw_s_sda(struct i2c_sw *i2c, TLevel sda)
{
    i2csw_bit_delay(i2c);
	i2c->err_code = I2CERR_OK;

    return true;
}

//--------------------------------------------------------
// 写数据
//--------------------------------------------------------
bool i2csw_write_bytes(struct i2c_sw *i2c, u8* data, int length)
{
	int i;

    spin_lock( &i2c->slock );

	if( i2csw_start(i2c) ) {
 		for(i = 0; i < length;i++)	{
			if(!i2csw_write(i2c, *(data+i)))	{
				break;
			}
		}
	}

	i2csw_stop(i2c);

    spin_unlock( &i2c->slock );

	return (i2c->err_code==I2CERR_OK);
}

//--------------------------------------------------------
// 写数据
//--------------------------------------------------------
bool i2csw_write_bytes_nostop(struct i2c_sw *i2c, u8* data, int length)
{
	int i;

    spin_lock( &i2c->slock );

	if( i2csw_start(i2c) ) {
 		for(i = 0; i < length;i++)	{
			if(!i2csw_write(i2c, *(data+i)))	{
				break;
			}
		}
	}

    spin_unlock( &i2c->slock );

	return (i2c->err_code==I2CERR_OK);
}

//--------------------------------------------------------
// 读数据
//--------------------------------------------------------
bool i2csw_read_bytes(struct i2c_sw *i2c, u8 *data_w, int length_w, u8 *data_r, int length_r)
{
	int i;

    spin_lock( &i2c->slock );

	if( i2csw_start(i2c) ) {
 		for(i = 0; i < length_w;i++)	{
			if(!i2csw_write(i2c, *(data_w+i)))	{
				break;
			}
		}

		if(i >= length_w) {
			for(i = 0; i < length_r; i++) {
				if(!i2csw_read(i2c, data_r+i)) {
					break;
				}
				if(i < length_r - 1) {
					i2csw_send_ack(i2c);
				}
			}
			i2csw_send_nack(i2c);
		}
	}

    spin_unlock( &i2c->slock );

	return (i2c->err_code==I2CERR_OK);
}
