#ifndef LCD_I2C_H_
#define LCD_I2C_H_

#include <avr/io.h>
#include <util/delay.h>
#include <stdarg.h>
#include <stdio.h>
#include "twi_master.h"

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

// ajuste se necessário (0x20..0x27)
#define LCD_I2C_ADDR 0x27

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_COMMAND   0
#define LCD_DATA      1

void lcd_init(void);
void lcd_clear(void);
void lcd_home(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_print(const char *s);
void lcd_printf(const char *fmt, ...);

#endif
