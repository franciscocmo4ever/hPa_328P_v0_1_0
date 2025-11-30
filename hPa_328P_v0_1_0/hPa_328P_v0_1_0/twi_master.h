#ifndef TWI_MASTER_H_
#define TWI_MASTER_H_

#include <avr/io.h>
#include <util/twi.h>

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

// 100 kHz em F_CPU = 8 MHz ? TWBR = 32 (aprox), prescaler = 1
#define F_SCL      100000UL
#define TWI_PRESC  1
#define TWBR_VAL   (uint8_t)((F_CPU / F_SCL - 16) / (2 * TWI_PRESC))

void twi_init(void);
uint8_t twi_start(void);
void twi_stop(void);
void twi_write(uint8_t data);
uint8_t twi_read_ack(void);
uint8_t twi_read_nack(void);

#endif
