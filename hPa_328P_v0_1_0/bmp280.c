#define F_CPU 1000000UL
#include "bmp280.h"
#include "twi_master.h"
#include <util/delay.h>

static int32_t t_fine;
static uint16_t dig_T1, dig_P1;
static int16_t  dig_T2, dig_T3;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

static uint8_t  r8(uint8_t reg){
	twi_start(); twi_write((BMP280_ADDR<<1)|0); twi_write(reg);
	twi_start(); twi_write((BMP280_ADDR<<1)|1);
	uint8_t v = twi_read_nack(); twi_stop(); return v;
}
static uint16_t r16(uint8_t reg){
	twi_start(); twi_write((BMP280_ADDR<<1)|0); twi_write(reg);
	twi_start(); twi_write((BMP280_ADDR<<1)|1);
	uint16_t v = ((uint16_t)twi_read_ack()<<8) | twi_read_nack(); twi_stop(); return v;
}
static void w8(uint8_t reg, uint8_t val){
	twi_start(); twi_write((BMP280_ADDR<<1)|0); twi_write(reg); twi_write(val); twi_stop();
}

void bmp280_init(void){
	_delay_ms(200);
	// ctrl_meas: osrs_t=×1, osrs_p=×1, mode=normal (0x27)
	w8(0xF4, 0x27);
	// config: t_sb=1000ms, filtro=4 (0xA0)
	w8(0xF5, 0xA0);

	// calib
	dig_T1 = r16(0x88);
	dig_T2 = (int16_t)r16(0x8A);
	dig_T3 = (int16_t)r16(0x8C);

	dig_P1 = r16(0x8E);
	dig_P2 = (int16_t)r16(0x90);
	dig_P3 = (int16_t)r16(0x92);
	dig_P4 = (int16_t)r16(0x94);
	dig_P5 = (int16_t)r16(0x96);
	dig_P6 = (int16_t)r16(0x98);
	dig_P7 = (int16_t)r16(0x9A);
	dig_P8 = (int16_t)r16(0x9C);
	dig_P9 = (int16_t)r16(0x9E);
}

void bmp280_read(float *temperature, float *pressure){
	// burst 6B from 0xF7
	uint8_t d[6];
	twi_start(); twi_write((BMP280_ADDR<<1)|0); twi_write(0xF7);
	twi_start(); twi_write((BMP280_ADDR<<1)|1);
	for(uint8_t i=0;i<5;i++) d[i]=twi_read_ack(); d[5]=twi_read_nack(); twi_stop();

	uint32_t adc_P = ((uint32_t)d[0]<<12) | ((uint32_t)d[1]<<4) | (d[2]>>4);
	uint32_t adc_T = ((uint32_t)d[3]<<12) | ((uint32_t)d[4]<<4) | (d[5]>>4);

	// temp
	int32_t var1 = ((((int32_t)adc_T>>3) - ((int32_t)dig_T1<<1)) * (int32_t)dig_T2) >> 11;
	int32_t var2 = (((((int32_t)adc_T>>4) - (int32_t)dig_T1) * (((int32_t)adc_T>>4) - (int32_t)dig_T1)) >> 12) * (int32_t)dig_T3 >> 14;
	t_fine = var1 + var2;
	*temperature = ((t_fine * 5 + 128) >> 8) / 100.0f;

	// pressure (Bosch, 64-bit)
	int64_t var1p, var2p, p64;
	var1p = ((int64_t)t_fine) - 128000;
	var2p = var1p * var1p * (int64_t)dig_P6;
	var2p = var2p + ((var1p * (int64_t)dig_P5) << 17);
	var2p = var2p + (((int64_t)dig_P4) << 35);
	var1p = ((var1p * var1p * (int64_t)dig_P3) >> 8) + ((var1p * (int64_t)dig_P2) << 12);
	var1p = (((((int64_t)1) << 47) + var1p)) * ((int64_t)dig_P1) >> 33;

	if (var1p == 0){ *pressure = 0; return; }

	p64 = 1048576 - adc_P;
	p64 = (((p64 << 31) - var2p) * 3125) / var1p;
	var1p = (((int64_t)dig_P9) * (p64 >> 13) * (p64 >> 13)) >> 25;
	var2p = (((int64_t)dig_P8) * p64) >> 19;
	p64 = ((p64 + var1p + var2p) >> 8) + (((int64_t)dig_P7) << 4);

	*pressure = ((float)p64 / 256.0f) / 100.0f;   // Pa ? hPa
}
