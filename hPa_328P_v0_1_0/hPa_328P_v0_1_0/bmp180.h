#ifndef BMP180_H
#define BMP180_H
#include <avr/io.h>
#include <stdint.h>

#define BMP180_ADDR 0x77

void bmp180_init(void);
void bmp180_read(float *temperature, float *pressure);

#endif
