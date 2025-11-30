#define F_CPU 1000000UL      // Define clock do AVR como 1 MHz (para delay e timing interno)

#include "bmp180.h"          // Header do driver do BMP180 (declarações)
#include "twi_master.h"      // Funções de I²C (start, write, read, stop)
#include <util/delay.h>      // Biblioteca de delays do AVR
#include <math.h>            // Usado para cálculos matemáticos (float)

// Variáveis globais de calibração do BMP180 armazenadas após bmp180_init()
static int16_t  AC1, AC2, AC3, B1, B2, MB, MC, MD;
static uint16_t AC4, AC5, AC6;
static int32_t  b5;          // Valor intermediário usado nos cálculos

// =======================================================
// Funções básicas de leitura e escrita I²C (8 bits)
// =======================================================

// Lê 1 byte de um registrador do BMP180
static uint8_t r8(uint8_t reg) {
	twi_start();             // START no barramento I²C
	twi_write(0xEE);         // Endereço do BMP180 em modo WRITE (0xEE)
	twi_write(reg);          // Seleciona o registrador que queremos ler
	twi_start();             // Repeated START
	twi_write(0xEF);         // Endereço do BMP180 em modo READ (0xEF)
	uint8_t v = twi_read_nack(); // Lê 1 byte, último byte ? NACK
	twi_stop();              // Encerra a comunicação
	return v;                // Retorna o valor lido
}

// Lê 2 bytes consecutivos (MSB + LSB)
static uint16_t r16(uint8_t reg) {
	twi_start();             // START
	twi_write(0xEE);         // Endereço WRITE
	twi_write(reg);          // Registrador
	twi_start();             // Repeated START
	twi_write(0xEF);         // Endereço READ
	uint16_t v = ((uint16_t)twi_read_ack() << 8) | twi_read_nack();
	// Lê MSB com ACK e LSB com NACK
	twi_stop();              // STOP
	return v;                // Retorna o valor de 16 bits
}

// Escreve 1 byte em um registrador
static void w8(uint8_t reg, uint8_t val) {
	twi_start();             // START
	twi_write(0xEE);         // Endereço WRITE
	twi_write(reg);          // Escolhe registrador
	twi_write(val);          // Escreve valor
	twi_stop();              // STOP
}

// =======================================================
// Inicialização e leitura da calibração
// =======================================================
void bmp180_init(void) {
	_delay_ms(1000);         // Tempo para o BMP180 inicializar após ligar

	// Pequenas leituras para garantir que o sensor "acordou"
	for(uint8_t i=0;i<5;i++) {
		r8(0xD0);           // Registrador ID
		_delay_ms(10);
	}

	// Leitura em bloco dos 22 bytes de calibração
	twi_start();
	twi_write(0xEE);         // Endereço WRITE
	twi_write(0xAA);         // Endereço do primeiro registrador de calibração
	twi_start();
	twi_write(0xEF);         // Endereço READ

	uint8_t buf[22];         // Buffer dos 22 bytes
	for (uint8_t i=0; i<21; i++)
	buf[i] = twi_read_ack();
	buf[21] = twi_read_nack(); // Último byte com NACK
	twi_stop();

	// Converte bytes para variáveis reais do datasheet
	AC1 = (int16_t)((buf[0]  << 8) | buf[1]);
	AC2 = (int16_t)((buf[2]  << 8) | buf[3]);
	AC3 = (int16_t)((buf[4]  << 8) | buf[5]);
	AC4 = (uint16_t)((buf[6]  << 8) | buf[7]);
	AC5 = (uint16_t)((buf[8]  << 8) | buf[9]);
	AC6 = (uint16_t)((buf[10] << 8) | buf[11]);
	B1  = (int16_t)((buf[12] << 8) | buf[13]);
	B2  = (int16_t)((buf[14] << 8) | buf[15]);
	MB  = (int16_t)((buf[16] << 8) | buf[17]);
	MC  = (int16_t)((buf[18] << 8) | buf[19]);
	MD  = (int16_t)((buf[20] << 8) | buf[21]);
}

// =======================================================
// Leitura de temperatura e pressão (OSS = 0)
// =======================================================
void bmp180_read(float *temperature, float *pressure) {

	// Se calibração inválida
	if (AC1 == 0 || AC1 == 0xFFFF) {
		*temperature = 0;
		*pressure = 0;
		return;
	}

	// ===== Temperatura =====
	w8(0xF4, 0x2E);          // Comando de leitura de temperatura
	_delay_ms(5);            // Conversão demora 4.5 ms
	uint16_t ut = r16(0xF6); // Lê temperatura bruta

	// Fórmulas do datasheet (compensação)
	int32_t x1 = ((int32_t)ut - AC6) * AC5 / 32768;
	int32_t x2 = ((int32_t)MC * 2048) / (x1 + MD);
	b5 = x1 + x2;

	// Converte para °C com 1 decimal
	*temperature = ((b5 + 8) >> 4) / 10.0f;

	// ===== Pressão =====
	w8(0xF4, 0x34);          // Comando de leitura da pressão (OSS = 0)
	_delay_ms(8);            // Conversão ~7.5 ms

	// Leitura de 3 bytes (MSB, LSB, XLSB)
	uint32_t up = ((uint32_t)r16(0xF6) << 8) | r8(0xF8);
	up >>= 8;                // Ajuste porque OSS=0

	// Mais fórmulas longas do datasheet
	int32_t b6 = b5 - 4000;
	x1 = (B2 * ((b6 * b6) >> 12)) >> 11;
	x2 = (AC2 * b6) >> 11;
	int32_t x3 = x1 + x2;

	int32_t b3 = (((((int32_t)AC1) * 4 + x3) + 2) >> 2);

	x1 = (AC3 * b6) >> 13;
	x2 = (B1 * ((b6 * b6) >> 12)) >> 16;
	x3 = ((x1 + x2) + 2) >> 2;

	uint32_t b4 = (AC4 * (uint32_t)(x3 + 32768)) >> 15;
	uint32_t b7 = ((uint32_t)up - b3) * 50000;

	int32_t p;
	if (b7 < 0x80000000)
	p = (b7 << 1) / b4;    // Caminho para valores menores
	else
	p = (b7 / b4) << 1;    // Caminho para valores grandes

	// Compensação final
	x1 = (p >> 8) * (p >> 8);
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	p = p + ((x1 + x2 + 3791) >> 4);

	*pressure = p / 100.0f; // Converte para hPa (hectopascal)
}
