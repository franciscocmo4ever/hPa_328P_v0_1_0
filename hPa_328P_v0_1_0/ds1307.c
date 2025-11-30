/*
 * ds1307.c
 *
 * Created: 16/11/2025 19:36:19
 *  Author: franc
 */ 


#include "ds1307.h"
#include "i2c.h"

static uint8_t bcd2dec(uint8_t v){ return (v>>4)*10 + (v & 0x0F); }
static uint8_t dec2bcd(uint8_t v){ return ((v/10)<<4) | (v%10); }

void ds1307_init(void)
{
	i2c_start();
	i2c_write(DS1307_ADDR << 1);
	i2c_write(0x00);      // registro de segundos
	i2c_write(0x00);      // CH=0 (clock habilitado)
	i2c_stop();
}

void ds1307_setTime(rtc_time *t)
{
	i2c_start();
	i2c_write(DS1307_ADDR << 1);
	i2c_write(0x00);
	i2c_write(dec2bcd(t->sec));
	i2c_write(dec2bcd(t->min));
	i2c_write(dec2bcd(t->hour));
	i2c_stop();
}

void ds1307_setDate(rtc_date *d)
{
	i2c_start();
	i2c_write(DS1307_ADDR << 1);
	i2c_write(0x03);
	i2c_write(dec2bcd(d->weekday));
	i2c_write(dec2bcd(d->day));
	i2c_write(dec2bcd(d->month));
	i2c_write(dec2bcd(d->year - 2000));
	i2c_stop();
}

void ds1307_getTime(rtc_time *t)
{
	i2c_start();
	i2c_write(DS1307_ADDR << 1);
	i2c_write(0x00);
	i2c_stop();

	i2c_start();
	i2c_write((DS1307_ADDR << 1) | 1);
	t->sec  = bcd2dec(i2c_readAck() & 0x7F);
	t->min  = bcd2dec(i2c_readAck());
	t->hour = bcd2dec(i2c_readNak());
	i2c_stop();
}

void ds1307_getDate(rtc_date *d)
{
	i2c_start();
	i2c_write(DS1307_ADDR << 1);
	i2c_write(0x03);
	i2c_stop();

	i2c_start();
	i2c_write((DS1307_ADDR << 1) | 1);
	d->weekday = bcd2dec(i2c_readAck());
	d->day     = bcd2dec(i2c_readAck());
	d->month   = bcd2dec(i2c_readAck());
	d->year    = 2000 + bcd2dec(i2c_readNak());
	i2c_stop();
}
