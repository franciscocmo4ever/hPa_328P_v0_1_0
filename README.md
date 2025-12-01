Projeto hPa_328P_v0_1_0 — Explicação Detalhada


O projeto hPa_328P_v0_1_0 é uma estação barométrica completa construída sobre o microcontrolador ATmega328P rodando a 1 MHz com oscilador interno. Ela combina:
✔ Sensor barométrico BMP180 (pressão e temperatura)
✔ Sensor de temperatura LM35 (analógico)
✔ Relógio de tempo real DS1307
✔ Display LCD 20x4 I2C via PCF8574
✔ Algoritmos adicionais (fase da lua, altitude, tendência de tempo)
✔ Sistema de menus automáticos
✔ Modo sleep com Watchdog Timer
✔ LED de status via Timer1 CTC
✔ Arquitetura modular organizada (drivers + camada de aplicação)
O objetivo é monitorar pressão atmosférica, temperatura e informações astronômicas com baixo consumo de energia.

🧱 Arquitetura Geral do Sistema
/Libraries
    bmp180.c / bmp180.h     -> Driver do sensor barométrico I2C
    ds1307.c / ds1307.h     -> Driver do RTC por I2C
    lcd_i2c.c / lcd_i2c.h   -> Comunicação com LCD 20x4 via PCF8574
    twi_master.c / twi.h    -> Implementação TWI (I²C) em 25 kHz
    uart.c / uart.h         -> (Opcional) Debug
    logger.c / logger.h     -> (Opcional) Registro EEPROM
main.c                      -> Lógica principal e menus
O código segue o padrão:
    • HAL (Hardware Abstraction Layer) → drivers
    • Application Layer → menus, lógica de exibição e medições
Isso torna o projeto fácil de expandir (ex: incluir EEPROM, SD Card, WiFi ESP8266 etc.).

🌡 Sensores e Recursos Lidos
🔵 BMP180 (I2C)
    • Temperatura ambiente
    • Pressão atmosférica (hPa)
    • Calibração interna automática
    • Altitude calculada pela fórmula barométrica:
altitude = 44330 * (1 - (Pressão / PressãoRef)^0.1903)
🔴 LM35 (Analógico)
    • Lido pelo ADC do ATmega328P
    • Conversão usada:
temp = (ADC * 5000 mV / 1023) / 10
🕒 DS1307 (I2C)
    • Obtém dia, mês, ano
    • Calcula:
        ◦ Dia do ano
        ◦ Dias restantes até o fim do ano
        ◦ Fase da lua (algoritmo aproximado)
        ◦ Nome abreviado do mês
        ◦ Nome da fase lunar

🔀 Sistema de Menus Automáticos
A interface do usuário no LCD funciona com 3 menus que mudam automaticamente a cada 1 segundo:
Menu 0 → Pressão / Temperatura / Tendência do tempo
Menu 1 → LM35 + Altitude calculada
Menu 2 → Calendário + Fase da Lua
A troca é controlada pela variável:
segundos_menu
Incrementada pelo Watchdog Timer.

⚡ Recursos Internos do Microcontrolador
🔹 Watchdog Timer como temporizador de 1 segundo
Usado para:
    • Contar segundos
    • Controlar troca de menu
    • Acordar o sistema do modo sleep (Power Down)
Configuração:
WDTCSR = (1<<WDIE) | (1<<WDP2) | (1<<WDP1); // ~1s
🔹 Sleep Mode (Power Down)
Reduz consumo energético entre leituras:
sleep_seconds(10);
🔹 Timer1 em modo CTC
Gera um blink no LED PB4 automaticamente:
    • Frequência ≈ 1 Hz
    • OCR1A = 488 para 1 MHz com prescaler 1024
Interrupt:
ISR(TIMER1_COMPA_vect){
    PINB |= (1 << LED_STATUS_PIN);
}
Isso deixa o LED piscando usando hardware, sem gastar CPU.

💨 Fluxo Geral de Execução do Programa
1️⃣ Inicialização
    • Configura pinos (LED, botão, backlight)
    • Inicia drivers: TWI / LCD / BMP180 / ADC / DS1307
    • Liga interrupções (sei())
    • Inicia Watchdog + Timer1
    • Faz leitura inicial da pressão para usar como pressão de referência
2️⃣ Loop principal (while 1)
A cada ciclo:
    1. Verifica se precisa trocar de menu
    2. Lê BMP180 (temp_bmp e press)
    3. Lê ADC do LM35
    4. Calcula altitude
    5. Chama o menu correto
    6. Guarda prev_press para tendência
    7. Entra em sleep por 10s

🌦 Interpretação de Tempo (Weather Forecast)
No Menu 0:
if (press < 996)     → Tempestade
else if < 1004       → Chuva
else if < 1010       → Nublado
else                 → Sol
Indica:
    • Pressão baixa → instabilidade
    • Pressão alta → céu limpo
E mostra tendência:
    • Subindo → "↑"
    • Descendo → "↓"
    • Estável → "-"

🌙 Algoritmo de Fase da Lua
Baseado na aproximação:
    • Calcula dia Juliano relativo
    • Aplica módulo do ciclo lunar (29.53 dias)
    • Converte para uma das 8 fases:
Nova, Crescente, 1/4+, Gib+, Cheia, Gib-, 1/4-, Minguante
O LCD mostra:
Lua: Cheia

🧭 Altímetro Barométrico
A altitude não vem do sensor — é calculada comparando a pressão atual com a pressão de referência:
press_ref  → lida no início
press      → lida agora

altitude = 44330 * (1 - (press / press_ref)^0.1903)
Se a pressão sobe → altitude diminui
Se a pressão cai → altitude aumenta (tempestade chegando)

🔌 GPIOs do Projeto
Sinal	Porta	Função
LED_PIN	PB0	LED principal
LED_STATUS_PIN	PB4	LED que pisca via Timer1
BTN_PIN	PB2	Botão de controle (pull-up)
BL_PIN	PB1	Controle do backlight
LM35_CHANNEL	PC0	Entrada ADC do LM35

📊 Resumo Geral do Projeto
O hPa_328P_v0_1_0 é uma estação barométrica compacta que:
    • Lê pressão e temperatura (BMP180)
    • Lê temperatura externa (LM35)
    • Calcula altitude barométrica
    • Monitora fase da lua e calendário
    • Atualiza menus automaticamente
    • Pisca LED via Timer1
    • Entra em sleep para economizar energia
    • Usa I²C em 25 kHz com drivers próprios
    • É totalmente modular e expansível
É um projeto robusto, limpo e semi profissional

