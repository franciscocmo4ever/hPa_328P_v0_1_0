#ifndef BMP280_H
#define BMP280_H
#include <avr/io.h>
#include <stdint.h>

// AJUSTE AQUI se seu BMP280 estiver em 0x77
//#define BMP280_ADDR 0x76
#define BMP280_ADDR 0x77


void bmp280_init(void);
void bmp280_read(float *temperature, float *pressure);

#endif
