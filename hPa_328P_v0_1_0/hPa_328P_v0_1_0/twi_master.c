#define F_CPU 1000000UL
#include "twi_master.h"

void twi_init(void) {
	TWSR = 0x00;     // prescaler = 1
	TWBR = 12;       // ~25 kHz em F_CPU = 1 MHz
	TWCR = (1<<TWEN);
}

uint8_t twi_start(void) {
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
	return (TWSR & 0xF8);
}

void twi_stop(void) {
	TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
	while(TWCR & (1<<TWSTO));
}

void twi_write(uint8_t data) {
	TWDR = data;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
}

uint8_t twi_read_ack(void) {
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
	while(!(TWCR & (1<<TWINT)));
	return TWDR;
}

uint8_t twi_read_nack(void) {
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
	return TWDR;
}
