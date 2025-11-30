#define F_CPU 1000000UL

#include "lcd_i2c.h"

static void i2c_out(uint8_t v){
	twi_start();
	twi_write((LCD_I2C_ADDR<<1) | 0);
	twi_write(v);
	twi_stop();
}

static void lcd_send_nibble(uint8_t nibble, uint8_t mode){
	uint8_t d = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
	i2c_out(d | LCD_ENABLE);
	_delay_us(1);
	i2c_out(d & ~LCD_ENABLE);
	_delay_us(50);
}

static void lcd_send(uint8_t val, uint8_t mode){
	lcd_send_nibble(val & 0xF0, mode);
	lcd_send_nibble((val<<4) & 0xF0, mode);
}

static void cmd(uint8_t c){ lcd_send(c, LCD_COMMAND); _delay_ms(2); }

void lcd_clear(void){ cmd(0x01); _delay_ms(2); }
void lcd_home(void){ cmd(0x02); _delay_ms(2); }

void lcd_set_cursor(uint8_t col, uint8_t row){
	static const uint8_t offs[] = {0x00,0x40,0x14,0x54};
	cmd(0x80 | (offs[row] + col));
}

void lcd_init(void){
	_delay_ms(40);
	lcd_send_nibble(0x30, LCD_COMMAND); _delay_ms(5);
	lcd_send_nibble(0x30, LCD_COMMAND); _delay_us(150);
	lcd_send_nibble(0x20, LCD_COMMAND); // 4-bit

	cmd(0x28); // 4-bit, 2 linhas, 5x8
	cmd(0x0C); // display ON, cursor OFF
	cmd(0x06); // entry mode
	lcd_clear();
}

void lcd_print(const char *s){
	while(*s) lcd_send(*s++, LCD_DATA);
}

void lcd_printf(const char *fmt, ...){
	char buf[32];
	va_list ap; va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	lcd_print(buf);
}
