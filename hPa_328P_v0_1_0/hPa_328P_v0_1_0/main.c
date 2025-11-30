#define F_CPU 1000000UL   // Clock interno de 1 MHz

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "twi_master.h"   // Comunicação I²C
#include "lcd_i2c.h"      // Display LCD via PCF8574
#include "bmp180.h"       // Sensor de pressão/temperatura BMP180
#include "ds1307.h"       // Novo: DS1307 (RTC)

// ==============================
// Definições de parâmetros
// ==============================
#define READ_INTERVAL_SECONDS   10       // (não está sendo usado no momento)
#define LOW_PRESSURE_HPA        1000.0f  // Pressão baixa (hPa)

// ==============================
// Configuração dos pinos do LED
// ==============================
#define LED_PORT PORTB
#define LED_DDR  DDRB
#define LED_PIN  PB0

#define LED_STATUS_PIN PB4   // LED de atividade (Timer1)

// ==============================
// Outros pinos usados no projeto
// ==============================
#define BTN_PIN  PB2      // Botão
#define BL_PIN   PB1      // Backlight do LCD (on/off)

// ==============================
// Canal analógico do sensor LM35
// ==============================
#define LM35_CHANNEL PC0

// Variáveis globais
volatile uint8_t wdt_ticks = 0;
float press_ref = 1013.25f;  // Pressão de referência ao ligar
uint8_t screen = 0;          // 0 = Tela barômetro / 1 = Tela relógio

// ============ TIMER1 para piscar PB4 apenas quando acordado ===============
void timer1_init_ctc(void){
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); // CTC, prescaler 1024
	OCR1A = 488;                // Aproximadamente ~0,5 s @ 1 MHz (pisca "devagar")
	TIMSK1 = (1 << OCIE1A);     // Habilita interrupção do Timer1
}

void timer1_stop(void){
	TIMSK1 &= ~(1 << OCIE1A);   // Desabilita interrupção
}

void timer1_start(void){
	TIMSK1 |= (1 << OCIE1A);    // Habilita interrupção
}

ISR(TIMER1_COMPA_vect){
	PINB |= (1 << LED_STATUS_PIN); // Pisca LED PB4 (toggle)
}

// ===================== WATCHDOG TIMER =======================================
static void wdt_setup_seconds(uint8_t seconds) {
	(void)seconds; // aqui você está fixo em ~1s, então não usa o parâmetro

	cli();

	MCUSR &= ~(1<<WDRF);

	WDTCSR = (1<<WDCE) | (1<<WDE);

	// ~1 segundo de período
	WDTCSR = (1<<WDIE) | (1<<WDP2) | (1<<WDP1);

	sei();
}

ISR(WDT_vect) {
	if (wdt_ticks < 255)
	wdt_ticks++;
}

// ===================== SLEEP ================================================
static void sleep_seconds(uint8_t seconds) {
	wdt_ticks = 0;

	timer1_stop(); // para o pisca LED durante o sono

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	while (wdt_ticks < seconds) {
		sleep_enable();
		sleep_cpu();
		sleep_disable();
	}

	timer1_start(); // volta a piscar LED
}

// ===================== ADC: LM35 ============================================
static void adc_init(void) {
	ADMUX = (1 << REFS0);  // AVcc como referência
	ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
	// com F_CPU=1MHz => prescaler=8 => F_ADC = 125kHz
}

static uint16_t adc_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);

	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));

	return ADC;
}

// ===================== MAIN ================================================
int main(void){

	// --------- Configuração de saídas (LEDs) ----------
	LED_DDR |= (1<<LED_PIN);            // LED alerta
	LED_DDR |= (1<<LED_STATUS_PIN);     // LED status (PB4)
	LED_PORT &= ~(1<<LED_PIN);
	LED_PORT &= ~(1<<LED_STATUS_PIN);

	// --------- Botão e Backlight LCD ---------------
	DDRB &= ~(1<<BTN_PIN);              // Botão como entrada
	PORTB |= (1<<BTN_PIN);              // Pull-up no botão

	DDRB |= (1<<BL_PIN);                // Backlight como saída
	PORTB &= ~(1<<BL_PIN);              // Backlight desligado inicialmente

	// --------- Inicializações de periféricos --------
	twi_init();                         // I2C para BMP180, LCD, DS1307
	lcd_init();                         // LCD via PCF8574
	bmp180_init();                      // BMP180
	adc_init();                         // ADC (LM35)
	ds1307_init();                      // DS1307 (RTC)

	sei();                              // Habilita interrupções globais

	wdt_setup_seconds(1);               // WDT ~1s
	timer1_init_ctc();                  // Pisca LED de status

	// --------- Leitura inicial para calibrar altitude ----------
	float temp_dummy = 0;
	bmp180_read(&temp_dummy, &press_ref); // Pressão atual como referência

	// --------- Tela de calibração inicial ----------
	lcd_clear();
	lcd_set_cursor(0,0);
	lcd_print("Calibrando Altitude");
	lcd_set_cursor(0,1);
	lcd_printf("Ref: %.2f hPa", press_ref);
	_delay_ms(500);

	// Variáveis de leitura em loop
	float temp_bmp = 0, press = 0, prev_press = 0;

	while (1) {

		// ===================== Leitura do BMP180 =====================
		bmp180_read(&temp_bmp, &press);

		float altitude = 44330.0f * (1.0f - pow((press / press_ref), 0.1903f));

		// ===================== Leitura do LM35 =======================
		uint16_t adc_val = adc_read(LM35_CHANNEL);
		float temp_lm35 = ((adc_val * 5000.0f) / 1023.0f) / 10.0f;

		// ===================== BACKLIGHT NO BOTÃO ====================
		if (!(PINB & (1 << BTN_PIN)))
		PORTB |= (1 << BL_PIN);   // Liga backlight
		else
		PORTB &= ~(1 << BL_PIN);  // Desliga backlight

		// ===================== SELEÇÃO DE TELAS ======================
		if (screen == 0) {
			// ===================== TELA 1 – BARÔMETRO =====================
			lcd_clear();
			lcd_set_cursor(0, 0);
			lcd_printf("T:%4.1fC P:%4.0fhPa", temp_bmp, press);

			lcd_set_cursor(0, 1);
			if (press < 996.0f)
			lcd_print("Tempo: Tempestade");
			else if (press < 1004.0f)
			lcd_print("Tempo: Chuva");
			else if (press < 1010.0f)
			lcd_print("Tempo: Nublado");
			else
			lcd_print("Tempo: Sol");

			lcd_set_cursor(14,1);
			if (press > prev_press + 0.3f)
			lcd_print("^");   // seta pra cima (melhora)
			else if (press < prev_press - 0.3f)
			lcd_print("v");   // seta pra baixo (piora)
			else
			lcd_print("-");   // estável

			prev_press = press;

			lcd_set_cursor(0,2);
			lcd_printf("Temp LM35: %4.1fC", temp_lm35);

			lcd_set_cursor(0,3);
			lcd_printf("Altitude: %6.1fm", altitude);

			// ---------- LED de alerta de pressão baixa ----------
			if (press < LOW_PRESSURE_HPA) {
				for (uint8_t i = 0; i < 5; i++) {
					LED_PORT |= (1 << LED_PIN);
					_delay_ms(300);
					LED_PORT &= ~(1 << LED_PIN);
					_delay_ms(300);
				}
				} else {
				LED_PORT &= ~(1 << LED_PIN);
			}
			} else {
			// ===================== TELA 2 – RELOGIO DS1307 =================
			rtc_time t;
			rtc_date d;

			ds1307_getTime(&t);
			ds1307_getDate(&d);

			lcd_clear();
			lcd_set_cursor(0,0);
			lcd_printf("Data: %02u/%02u/%04u", d.day, d.month, d.year);

			lcd_set_cursor(0,1);
			lcd_printf("Hora: %02u:%02u:%02u", t.hour, t.min, t.sec);

			lcd_set_cursor(0,2);
			lcd_printf("Semana: %u", d.weekday);

			lcd_set_cursor(0,3);
			lcd_print("Estacao ativa");
		}

		// ===================== ECONOMIA DE ENERGIA ===================
		sleep_seconds(10);   // Dorme 30s com WDT

		// Quando acordar ? alterna tela
		screen ^= 1;
	}
}
