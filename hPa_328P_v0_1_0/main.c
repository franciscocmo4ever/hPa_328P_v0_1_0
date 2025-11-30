#define F_CPU 1000000UL   // Clock interno de 1 MHz


#include <avr/io.h>          // Define registradores e pinos (PORT, DDR, PIN) para controlar hardware AVR
#include <avr/interrupt.h>   // Permite usar interrupções (ISR, sei, cli)
#include <avr/sleep.h>       // Funções para modos de economia de energia (sleep modes)
#include <util/delay.h>      // Funções de atraso (_delay_ms, _delay_us)
#include <string.h>          // Funções para manipular strings (strlen, strcpy, strcmp, etc.)
#include <math.h>            // Funções matemáticas (sqrt, sin, pow, etc.)
#include <stdio.h>           // Funções de entrada e saída (printf, sprintf, etc.)


#include "twi_master.h"   // Comunicação I²C
#include "lcd_i2c.h"      // Display LCD via PCF8574
#include "bmp180.h"      // Sensor de pressão/temperatura BMP180


#define READ_INTERVAL_SECONDS   10     // Intervalo entre leituras (em segundos)
#define LOW_PRESSURE_HPA        1000.0f // Valor limite para pressão baixa (em hPa, tipo float)

// ==============================
// Configuração dos pinos do LED
// ==============================
#define LED_PORT PORTB          // Porta usada pelo LED (PORTB)
#define LED_DDR  DDRB           // Registrador de direção do LED (define entrada/saída)
#define LED_PIN  PB0            // Pino do LED (pino físico correspondente ao bit 0 da PORTB)

// ==============================
// Outros pinos usados no projeto
// ==============================
#define BTN_PIN  PB2            // Pino do botão (Button) conectado ao PB2
#define BL_PIN   PB1            // Pino do backlight (BL) do display ou LED secundário no PB1

// ==============================
// Canal analógico do sensor LM35
// ==============================
#define LM35_CHANNEL PC0        // Canal ADC0 (pino PC0) usado para ler o sensor LM35

#define LED_STATUS_PIN PB4   // LED de atividade

volatile uint8_t wdt_ticks = 0;
float press_ref = 1013.25f;  // Pressão de referência ao ligar

// ============ TIMER1 para piscar PB4 apenas quando acordado ===============
void timer1_init_ctc(void){
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); // CTC, prescaler 1024
	OCR1A = 488;                // 1 segundo com 1 MHz
	TIMSK1 = (1 << OCIE1A);     // Habilita interrupção do Timer1
}

void timer1_stop(void){
	TIMSK1 &= ~(1 << OCIE1A);   // Desabilita interrupção
}

void timer1_start(void){
	TIMSK1 |= (1 << OCIE1A);    // Habilita interrupção
}

ISR(TIMER1_COMPA_vect){
	PINB |= (1 << LED_STATUS_PIN); // Pisca LED PB4
}

// ===================== WATCHDOG TIMER =======================================
// Função: configura o Watchdog Timer (WDT) para gerar uma interrupção periódica
// sem resetar o microcontrolador, usada como temporizador de tempo fixo (~1s).

static void wdt_setup_seconds(uint8_t seconds) {
	cli();                                 // Desabilita interrupções globais (garante configuração segura)

	MCUSR &= ~(1<<WDRF);                   // Limpa o flag WDRF (Watchdog Reset Flag)
	// Esse bit é setado se o WDT causou um reset anteriormente.
	// Aqui limpamos para evitar confusão de status.

	WDTCSR = (1<<WDCE) | (1<<WDE);         // Habilita mudança de configuração do WDT
	// WDCE = Watchdog Change Enable
	// WDE  = Watchdog System Reset Enable
	// Após setar WDCE, temos 4 ciclos de clock para reconfigurar o WDT.

	WDTCSR = (1<<WDIE) | (1<<WDP2) | (1<<WDP1);
	// Ativa interrupção do WDT (WDIE)
	// Define prescaler para ~1 segundo (WDP2:WDP1 = 110 => ~1.0s)
	// Importante: sem o bit WDE, ele apenas gera interrupção, não reset.

	sei();                                 // Reabilita interrupções globais
}

// ============================================================================
// Rotina de interrupção do Watchdog Timer
// Executada automaticamente a cada vez que o WDT "estoura" (~1s)
// ============================================================================

ISR(WDT_vect) {
	if (wdt_ticks < 255)                   // Se contador ainda não atingiu o máximo (evita overflow)
	wdt_ticks++;                       // Incrementa variável global (ex: usada como contador de segundos)
}


// ===================== SLEEP ================================================
// Função: coloca o microcontrolador em modo de sono (power-down) por um tempo definido
// usando o Watchdog Timer para acordar periodicamente até completar "seconds" segundos.

static void sleep_seconds(uint8_t seconds) {
	wdt_ticks = 0;                       // Zera o contador global de "ticks" do WDT (um tick ? 1 segundo)

	timer1_stop();                       // (Função externa) Desliga o Timer1 — aqui provavelmente
	// interrompe o piscar do LED ou outra rotina temporizada
	// enquanto o MCU dorme para economizar energia.

	set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Define o modo de sono mais profundo: "Power-down"
	// Nesse modo, quase tudo é desligado (CPU, ADC, timers, etc.)
	// e apenas o Watchdog ou interrupções externas podem acordar o chip.

	while (wdt_ticks < seconds) {        // Permanece dormindo até passar o tempo desejado
		sleep_enable();                  // Habilita o modo sleep (ativa o bit SE)
		sleep_cpu();                     // Executa o sono — o MCU realmente entra em modo "Power-down"
		sleep_disable();                 // Ao acordar (interrupção do WDT), desativa o modo sleep
	}

	timer1_start();                      // (Função externa) Religa o Timer1 — o LED volta a piscar
	// ou a rotina temporizada é retomada normalmente.
}


// ===================== ADC: LM35 ============================================
// ===================== ADC INITIALIZATION ===================================
// Função: inicializa o conversor analógico-digital (ADC)

static void adc_init(void) {
	ADMUX = (1 << REFS0);                        // Define a referência de tensão para o ADC como AVcc (5V)
	// REFS1:0 = 01 ? AVcc com capacitor no pino AREF
	// O pino AREF deve ter um capacitor de 100nF para o GND.

	ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
	// ADEN: habilita o ADC
	// ADPS2:0 = 011 ? Prescaler de 8
	// Frequência do ADC = F_CPU / 8
	// (Ex: se F_CPU = 8 MHz ? ADC opera a 1 MHz)
	// Isso garante conversões rápidas e precisas.
}


// ===================== ADC READING FUNCTION =================================
// Função: realiza a leitura analógica de um canal específico do ADC
// Retorna um valor de 10 bits (0–1023)

static uint16_t adc_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);    // Seleciona o canal (0–7) mantendo o restante de ADMUX
	// Limpa os 4 bits menos significativos e insere o novo canal

	ADCSRA |= (1 << ADSC);                        // Inicia a conversão (ADC Start Conversion)
	while (ADCSRA & (1 << ADSC));                 // Espera até que a conversão termine (ADSC = 0)

	return ADC;                                   // Retorna o resultado (registrador de 16 bits)
}


// ===================== MAIN ================================================
int main(void){

	// --------- Configuração de saídas (LEDs) ----------
	LED_DDR |= (1<<LED_PIN);            // Configura pino do LED de alerta como saída
	LED_DDR |= (1<<LED_STATUS_PIN);     // Configura pino do LED de status como saída
	LED_PORT &= ~(1<<LED_PIN);          // Garante LED de alerta desligado no início
	LED_PORT &= ~(1<<LED_STATUS_PIN);   // Garante LED de status desligado no início

	// --------- Botão e Backlight LCD ---------------
	DDRB &= ~(1<<BTN_PIN);              // Configura pino do botão como entrada
	PORTB |= (1<<BTN_PIN);              // Habilita resistor pull-up interno no botão (entrada em HIGH até apertar)

	DDRB |= (1<<BL_PIN);                // Pino do backlight como saída
	PORTB &= ~(1<<BL_PIN);              // Backlight desligado inicialmente

	// --------- Inicializações de periféricos --------
	twi_init();                         // Inicializa comunicação I2C (TWI) — para o BMP180 e LCD I2C

	//========================================================================================================================
	//========================================================================================================================
	
	lcd_init();                         // Inicializa o display LCD
	bmp180_init();                      // Inicializa o sensor BMP180 (temperatura e pressão)
	adc_init();                         // Inicializa ADC (para LM35 ou outros sensores analógicos)

	sei();                              // Habilita interrupções globais

	wdt_setup_seconds(1);               // Configura Watchdog para gerar interrupção a cada ~1 segundo
	timer1_init_ctc();                  // Inicia Timer1 em modo CTC (piscar LED PB4 ou status)

	// --------- Leitura inicial para calibrar altitude ----------
	float temp_dummy = 0;
	bmp180_read(&temp_dummy, &press_ref); // Lê pressão atual como referência para cálculo de altitude

	// --------- Tela de calibração inicial ----------
	lcd_clear();
	lcd_set_cursor(0,0);
	lcd_print("Calibrando Altitude");
	lcd_set_cursor(0,1);
	lcd_printf("Ref: %.2f hPa", press_ref); // Mostra pressão de referência
	_delay_ms(500);                         // Pausa para o usuário ver a mensagem

	// Variáveis de leitura em loop
	float temp_bmp = 0, press = 0, prev_press = 0;


	while (1) {                                                     // Loop infinito: o programa fica executando continuamente

		// Leitura do BMP180
		bmp180_read(&temp_bmp, &press);                            // Lê do BMP180 a temperatura (°C) e pressão (hPa) via I2C

		float altitude = 44330.0f * (1.0f -                        // Fórmula barométrica padrão (ISA) para altitude
		pow((press / press_ref), 0.1903f));      // Usa a razão press/press_ref ^ 0,1903; press_ref = pressão de referência ao nível "0"

		// Leitura do LM35
		uint16_t adc_val = adc_read(LM35_CHANNEL);                 // Lê o valor bruto (0–1023) do ADC no canal do LM35
		float temp_lm35 = ((adc_val * 5000.0f) / 1023.0f) / 10.0f; // Converte para °C: (adc * 5000/1023) -> mV; LM35 = 10 mV/°C, então divide por 10

		// Exibição no LCD
		lcd_clear();                                               // Limpa a tela do LCD antes de escrever novos dados
		lcd_set_cursor(0, 0);                                      // Posiciona o cursor na coluna 0, linha 0 (primeira linha)
		lcd_printf("T:%4.1fC P:%4.0fhPa", temp_bmp, press);        // Escreve a temperatura BMP180 com 1 casa e a pressão sem decimais (hPa)

		lcd_set_cursor(0, 1);                                      // Vai para o início da segunda linha
		if (press < 996.0f)                                        // Classificação simples do “tempo” baseada na pressão
		lcd_print("Tempo: Tempestade");                        // Pressão muito baixa ? tendência a tempestade
		else if (press < 1004.0f)
		lcd_print("Tempo: Chuva");                             // Faixa baixa ? possibilidade de chuva
		else if (press < 1010.0f)
		lcd_print("Tempo: Nublado");                           // Faixa intermediária ? nublado
		else
		lcd_print("Tempo: Sol");                               // Pressão mais alta ? tempo estável/ensolarado


		    lcd_set_cursor(14,1);                              // Move o cursor para a coluna 14 da linha 1 (parte direita da 2ª linha)

		    if (press > prev_press + 0.3f)                     // Se a pressão subiu mais de 0.3 hPa desde a última leitura
		    lcd_print("?");                                // Mostra uma seta para cima (indicando melhora do tempo)
		    else if (press < prev_press - 0.3f)                // Se a pressão caiu mais de 0.3 hPa
		    lcd_print("?");                                // Mostra uma seta para baixo (indicando piora do tempo)
		    else
		    lcd_print("-");                                // Caso contrário, mostra traço (pressão estável)

		    prev_press = press;                                // Atualiza a variável anterior para comparar na próxima iteração

		    // ===================== Exibição das demais informações =====================

		    lcd_set_cursor(0,2);                               // Linha 3 do LCD (posição 0)
		    lcd_printf("Temp LM35: %4.1fC", temp_lm35);        // Exibe temperatura medida pelo sensor LM35

		    lcd_set_cursor(0,3);                               // Linha 4 do LCD (posição 0)
		    lcd_printf("Altitude: %6.1fm", altitude);          // Exibe altitude calculada a partir da pressão

		    // ===================== LED de alerta de pressão baixa =====================

		    if (press < LOW_PRESSURE_HPA) {                    // Se a pressão está abaixo do limite definido (ex: 1000 hPa)
			    for (uint8_t i = 0; i < 5; i++) {              // Pisca o LED 5 vezes
				    LED_PORT |= (1 << LED_PIN);                // Liga LED (coloca nível alto no pino)
				    _delay_ms(300);                            // Espera 300 ms
				    LED_PORT &= ~(1 << LED_PIN);               // Desliga LED
				    _delay_ms(300);                            // Espera mais 300 ms
			    }
			    } else {
			    LED_PORT &= ~(1 << LED_PIN);                   // Caso contrário, mantém o LED apagado
		    }

		    // ===================== Controle manual do backlight =======================

		    if (!(PINB & (1 << BTN_PIN)))                      // Verifica se o botão (PB2) está pressionado (nível lógico 0)
		    PORTB |= (1 << BL_PIN);                        // Liga a luz de fundo (backlight)
		    else
		    PORTB &= ~(1 << BL_PIN);                       // Desliga a luz de fundo

		    // ===================== Economia de energia ===============================

		    sleep_seconds(30);                                  // Coloca o microcontrolador para dormir por 5 segundos
		    // Durante o sono, Timer1 (piscar do LED PB4) é pausado

	    }                                                       // Fim do loop while(1)
    }                                                       // Fim da função main()
