#define F_CPU 1000000UL   // Clock interno de 1 MHz

/*
 * ds1307.c
 * Versão ajustada para usar o driver TWI (twi_master.c)
 */

#include "ds1307.h"
#include "twi_master.h"   // usamos twi_start(), twi_write(), twi_read_ack(), twi_read_nack(), twi_stop()

// -----------------------------
// Funções auxiliares BCD <-> decimal
// -----------------------------
static uint8_t bcd2dec(uint8_t v)
{
    return (v >> 4) * 10 + (v & 0x0F);
}

static uint8_t dec2bcd(uint8_t v)
{
    return ((v / 10) << 4) | (v % 10);
}

// -----------------------------
// Inicialização do DS1307
// -----------------------------
void ds1307_init(void)
{
    // Zera o registrador de segundos e garante CH = 0 (clock rodando)
    twi_start();
    twi_write(DS1307_ADDR << 1);  // SLA+W
    twi_write(0x00);              // endereço do registrador de segundos
    twi_write(0x00);              // segundos = 0, CH = 0
    twi_stop();
}

// -----------------------------
// Ajuste de hora
// -----------------------------
void ds1307_setTime(rtc_time *t)
{
    twi_start();
    twi_write(DS1307_ADDR << 1);  // SLA+W
    twi_write(0x00);              // começa em segundos

    twi_write(dec2bcd(t->sec));
    twi_write(dec2bcd(t->min));
    twi_write(dec2bcd(t->hour));

    twi_stop();
}

// -----------------------------
// Ajuste de data
// -----------------------------
void ds1307_setDate(rtc_date *d)
{
    twi_start();
    twi_write(DS1307_ADDR << 1);  // SLA+W
    twi_write(0x03);              // começa em dia da semana

    twi_write(dec2bcd(d->weekday));
    twi_write(dec2bcd(d->day));
    twi_write(dec2bcd(d->month));
    twi_write(dec2bcd(d->year - 2000));

    twi_stop();
}

// -----------------------------
// Leitura de hora
// -----------------------------
void ds1307_getTime(rtc_time *t)
{
    // 1) posiciona ponteiro no registrador 0x00
    twi_start();
    twi_write(DS1307_ADDR << 1);  // SLA+W
    twi_write(0x00);              // registrador de segundos
    twi_stop();

    // 2) leitura sequencial: sec, min, hour
    twi_start();
    twi_write((DS1307_ADDR << 1) | 1);   // SLA+R

    t->sec  = bcd2dec(twi_read_ack() & 0x7F); // limpa bit CH
    t->min  = bcd2dec(twi_read_ack());
    t->hour = bcd2dec(twi_read_nack());

    twi_stop();
}

// -----------------------------
// Leitura de data
// -----------------------------
void ds1307_getDate(rtc_date *d)
{
    // 1) posiciona ponteiro no registrador 0x03
    twi_start();
    twi_write(DS1307_ADDR << 1);  // SLA+W
    twi_write(0x03);              // dia da semana
    twi_stop();

     twi_start();
     twi_write((DS1307_ADDR << 1) | 1);   // SLA+R

     d->weekday = bcd2dec(twi_read_ack());
     d->day     = bcd2dec(twi_read_ack());
     d->month   = bcd2dec(twi_read_ack());
     d->year    = 2000 + bcd2dec(twi_read_nack());

     twi_stop();
     
}