/*
 * ds1307.h
 *
 * Created: 16/11/2025 19:35:40
 *  Author: franc
 */ 
#ifndef DS1307_H_
#define DS1307_H_

#include <stdint.h>

#define DS1307_ADDR 0x68

typedef struct {
	uint8_t sec;
	uint8_t min;
	uint8_t hour;
} rtc_time;

typedef struct {
	uint8_t day;
	uint8_t month;
	uint16_t year;
	uint8_t weekday;
} rtc_date;

void ds1307_init(void);
void ds1307_setTime(rtc_time *t);
void ds1307_setDate(rtc_date *d);
void ds1307_getTime(rtc_time *t);
void ds1307_getDate(rtc_date *d);

#endif
