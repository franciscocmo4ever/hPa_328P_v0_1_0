/*
 * i2c.h
 *
 * Created: 16/11/2025 19:38:36
 *  Author: franc
 */ 

#ifndef I2C_H_
#define I2C_H_

void i2c_init(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_write(uint8_t data);
uint8_t i2c_readAck(void);
uint8_t i2c_readNak(void);

#endif
