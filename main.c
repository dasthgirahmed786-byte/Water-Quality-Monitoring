/*
 * main.c
 *
 * Project : Real-Time Water Quality Monitoring System
 * MCU     : NXP LPC1768 (ARM Cortex-M3)
 * Author  : Shaik Dastagiri Ahmed (Y23EC160)
 *
 * Reads temperature/humidity (DHT11), water level (HC-SR04 ultrasonic),
 * turbidity, pH, and dissolved oxygen (analog sensors via ADC), displays
 * everything on a 16x2 LCD, and streams the readings as JSON over UART3
 * to an ESP8266/ESP32 module for IoT/cloud monitoring, following the
 * pin map and calibration tables from the project report.
 *
 * NOTE ON BUILDING/RUNNING THIS PROJECT:
 * This is bare-metal ARM Cortex-M3 register-level code - it is not a
 * script you "run" the way you'd run a Python file. See README.md for
 * how to actually build and test it (Keil uVision simulator is the
 * most direct path, since that's the tool used during the internship).
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lpc17xx_regs.h"

// ---------------- Pin Map (from the project's pin configuration table) ----------------
#define DHT11_PIN   (1UL << 4)   // P0.4
#define TRIG_PIN    (1UL << 5)   // P0.5
#define ECHO_PIN    (1UL << 6)   // P0.6

#define LCD_RS_PIN  (1UL << 10)  // P0.10
#define LCD_EN_PIN  (1UL << 11)  // P0.11
#define LCD_DATA_SHIFT 15        // P0.15 - P0.22 = D0 - D7
#define LCD_DATA_MASK  (0xFFUL << LCD_DATA_SHIFT)

#define TURBIDITY_CHANNEL 0      // AD0.0 -> P0.23
#define PH_CHANNEL         1     // AD0.1 -> P0.24
#define DO_CHANNEL          2    // AD0.2 -> P0.25

// Assumed clock configuration (typical LPC1768 Keil project default):
// CCLK = 100 MHz, peripheral clock (PCLK) = CCLK / 4 = 25 MHz for
// Timer0 and the UARTs, unless PCLKSEL registers are changed elsewhere.
#define PCLK_HZ  25000000UL

char buffer[80];

// ---------------------------------------------------------------------------
// Timer0 - used as a free-running microsecond time base for delays and for
// timing the ultrasonic sensor's echo pulse.
// ---------------------------------------------------------------------------
void Timer0_Init(void) {
    LPC_SC_PCONP |= (1UL << 1);           // Power on Timer0
    LPC_T0TCR = 0x02;                      // Reset timer
    LPC_T0PR  = (PCLK_HZ / 1000000UL) - 1; // Prescaler -> TC increments every 1us
    LPC_T0TCR = 0x01;                      // Enable timer
}

static void timer0_reset(void) {
    LPC_T0TCR = 0x02;
    LPC_T0TCR = 0x01;
}

static uint32_t timer0_us(void) {
    return LPC_T0TC;
}

void delay_us(uint32_t us) {
    timer0_reset();
    while (timer0_us() < us) {
        // busy wait
    }
}

void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000);
    }
}

// ---------------------------------------------------------------------------
// GPIO setup
// ---------------------------------------------------------------------------
void DIO_Init(void) {
    LPC_GPIO0_FIODIR |= TRIG_PIN;      // Ultrasonic trigger -> output
    LPC_GPIO0_FIODIR &= ~ECHO_PIN;     // Ultrasonic echo -> input

    LPC_GPIO0_FIODIR |= LCD_RS_PIN;
    LPC_GPIO0_FIODIR |= LCD_EN_PIN;
    LPC_GPIO0_FIODIR |= LCD_DATA_MASK; // LCD data bus -> output
}

// ---------------------------------------------------------------------------
// ADC (Turbidity, pH, DO)
// ---------------------------------------------------------------------------
void ADC_Init(void) {
    LPC_SC_PCONP |= (1UL << 12); // Power on ADC

    // Configure AD0.0 (P0.23) for Turbidity
    LPC_PINSEL1 &= ~(3UL << 14);
    LPC_PINSEL1 |=  (1UL << 14);

    // Configure AD0.1 (P0.24) for pH
    LPC_PINSEL1 &= ~(3UL << 16);
    LPC_PINSEL1 |=  (1UL << 16);

    // Configure AD0.2 (P0.25) for DO
    LPC_PINSEL1 &= ~(3UL << 18);
    LPC_PINSEL1 |=  (1UL << 18);

    LPC_ADC_ADCR = (24UL << 8) | (1UL << 21); // CLKDIV = 24, ADC enabled (PDN)
}

uint16_t ADC_Read(uint8_t channel) {
    uint32_t result;

    LPC_ADC_ADCR &= 0xFFFFFF00UL;
    LPC_ADC_ADCR |= (1UL << channel) | (1UL << 24); // select channel, start conversion now

    while (!(LPC_ADC_ADGDR & (1UL << 31))) {
        // wait for DONE flag
    }

    result = LPC_ADC_ADGDR;
    return (uint16_t)((result >> 4) & 0xFFF); // 12-bit result
}

// ---------------------------------------------------------------------------
// Turbidity sensor (calibration table from the report)
// ---------------------------------------------------------------------------
uint16_t read_turbidity(void) {
    uint16_t adc_value;
    float voltage;
    float ntu;
    uint32_t sum = 0;

    for (uint8_t i = 0; i < 10; i++) {
        sum += ADC_Read(TURBIDITY_CHANNEL);
        delay_ms(10);
    }
    adc_value = (uint16_t)(sum / 10);

    voltage = ((float)adc_value * 3.3f) / 4095.0f;

    if (voltage >= 1.80f) {
        ntu = 0;
    } else if (voltage >= 1.70f) {
        ntu = (1.80f - voltage) * 200.0f;
    } else if (voltage >= 1.50f) {
        ntu = 20.0f + ((1.70f - voltage) * 400.0f);
    } else if (voltage >= 1.00f) {
        ntu = 100.0f + ((1.50f - voltage) * 800.0f);
    } else {
        ntu = 500.0f + (1.00f - voltage) * 1000.0f;
    }

    if (ntu < 0) ntu = 0;
    return (uint16_t)ntu;
}

// ---------------------------------------------------------------------------
// pH sensor (calibration table from the report)
// ---------------------------------------------------------------------------
float read_ph_sensor(void) {
    uint16_t adc_value;
    float voltage;
    float phValue;
    uint32_t sum = 0;

    for (uint8_t i = 0; i < 10; i++) {
        sum += ADC_Read(PH_CHANNEL);
        delay_ms(10);
    }
    adc_value = (uint16_t)(sum / 10);

    voltage = ((float)adc_value * 3.3f) / 4095.0f;

    if (voltage <= 2.8f) {
        phValue = -10.0f * (voltage - 2.6f) + 10.0f;
    } else if (voltage <= 3.23f) {
        phValue = -4.76f * (voltage - 2.8f) + 8.0f;
    } else {
        phValue = -57.14f * (voltage - 3.23f) + 6.0f;
    }

    if (phValue < 0.0f)  phValue = 0.0f;
    if (phValue > 14.0f) phValue = 14.0f;

    return phValue;
}

// ---------------------------------------------------------------------------
// Dissolved Oxygen sensor
// (Not shown in the report's code excerpt - implemented here using a
//  standard linear DO-probe calibration: adjust the two calibration
//  points below to match your specific probe's datasheet/calibration.)
// ---------------------------------------------------------------------------
#define DO_CAL_VOLTAGE_AT_SAT   1.60f   // volts at known saturated DO reading
#define DO_CAL_VALUE_AT_SAT     8.0f    // mg/L at that calibration point

float DO_GetVoltage(uint16_t adc_value) {
    return ((float)adc_value * 3.3f) / 4095.0f;
}

float DO_ConvertToMgL(float voltage) {
    float do_value = (voltage / DO_CAL_VOLTAGE_AT_SAT) * DO_CAL_VALUE_AT_SAT;
    if (do_value < 0) do_value = 0;
    return do_value;
}

// ---------------------------------------------------------------------------
// DHT11 temperature & humidity sensor (bit-banged, single-wire protocol)
// ---------------------------------------------------------------------------
static void dht11_set_output(void) { LPC_GPIO0_FIODIR |= DHT11_PIN; }
static void dht11_set_input(void)  { LPC_GPIO0_FIODIR &= ~DHT11_PIN; }
static void dht11_write_low(void)  { LPC_GPIO0_FIOCLR = DHT11_PIN; }
static uint8_t dht11_read_pin(void) { return (LPC_GPIO0_FIOPIN & DHT11_PIN) ? 1 : 0; }

// Returns 1 on success, 0 on failure (timeout or checksum mismatch)
uint8_t dht11_read(float *temperature, float *humidity) {
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint32_t timeout;

    // 1. Send start signal
    dht11_set_output();
    dht11_write_low();
    delay_ms(18);
    dht11_set_input();
    delay_us(30);

    // 2. Wait for sensor's response (80us low, then 80us high)
    timeout = 0;
    while (dht11_read_pin() == 1) { if (++timeout > 1000) return 0; }
    timeout = 0;
    while (dht11_read_pin() == 0) { if (++timeout > 1000) return 0; }
    timeout = 0;
    while (dht11_read_pin() == 1) { if (++timeout > 1000) return 0; }

    // 3. Read 40 bits (5 bytes)
    for (uint8_t byte_i = 0; byte_i < 5; byte_i++) {
        for (uint8_t bit_i = 0; bit_i < 8; bit_i++) {
            timeout = 0;
            while (dht11_read_pin() == 0) { if (++timeout > 1000) return 0; } // 50us low start

            delay_us(40); // if still high after 40us, it's a '1' bit (~70us), else '0' (~26-28us)
            uint8_t bit_value = dht11_read_pin();

            timeout = 0;
            while (dht11_read_pin() == 1) { if (++timeout > 1000) return 0; } // wait for line to go low again

            data[byte_i] <<= 1;
            data[byte_i] |= bit_value;
        }
    }

    // 4. Verify checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        return 0;
    }

    *humidity    = (float)data[0] + (float)data[1] / 10.0f;
    *temperature = (float)data[2] + (float)data[3] / 10.0f;
    return 1;
}

// ---------------------------------------------------------------------------
// Ultrasonic (HC-SR04) water level sensor
// ---------------------------------------------------------------------------
float get_ultrasonic_distance(void) {
    uint32_t elapsed;

    LPC_GPIO0_FIOSET = TRIG_PIN;
    delay_us(10);
    LPC_GPIO0_FIOCLR = TRIG_PIN;

    uint32_t timeout = 0;
    while (!(LPC_GPIO0_FIOPIN & ECHO_PIN)) {
        if (++timeout > 30000) return -1.0f; // no response from sensor
    }

    timer0_reset();
    while (LPC_GPIO0_FIOPIN & ECHO_PIN) {
        if (timer0_us() > 30000) break; // ~5m max range safety timeout
    }
    elapsed = timer0_us();

    return (elapsed * 0.0343f) / 2.0f; // distance in cm
}

// ---------------------------------------------------------------------------
// UART0 (PC serial monitor) and UART3 (ESP8266/ESP32 module)
// Both peripherals share the same register layout, so one Init routine
// is written per port to keep the pin setup (PINSEL) correct for each.
// ---------------------------------------------------------------------------
void init_uart0(void) {
    LPC_SC_PCONP |= (1UL << 3); // Power on UART0

    // P0.2 = TXD0, P0.3 = RXD0 (function 01)
    LPC_PINSEL0 &= ~(0xFUL << 4);
    LPC_PINSEL0 |=  (0x5UL << 4);

    uint16_t divisor = (uint16_t)(PCLK_HZ / (16UL * 9600UL));

    LPC_U0LCR = 0x83;         // 8N1, DLAB = 1
    LPC_U0DLL = divisor & 0xFF;
    LPC_U0DLM = (divisor >> 8) & 0xFF;
    LPC_U0LCR = 0x03;         // 8N1, DLAB = 0
    LPC_U0FCR = 0x07;         // Enable and reset FIFOs
}

void UART0_SendChar(char c) {
    while (!(LPC_U0LSR & (1UL << 5))) { /* wait for THR empty */ }
    LPC_U0THR = c;
}

void UART0_SendString(const char *s) {
    while (*s) {
        UART0_SendChar(*s++);
    }
}

void UART3_Init(void) {
    LPC_SC_PCONP |= (1UL << 25); // Power on UART3

    // P0.0 = TXD3, P0.1 = RXD3 (function 10)
    LPC_PINSEL0 &= ~(0xFUL << 0);
    LPC_PINSEL0 |=  (0xAUL << 0);

    uint16_t divisor = (uint16_t)(PCLK_HZ / (16UL * 9600UL));

    LPC_U3LCR = 0x83;
    LPC_U3DLL = divisor & 0xFF;
    LPC_U3DLM = (divisor >> 8) & 0xFF;
    LPC_U3LCR = 0x03;
    LPC_U3FCR = 0x07;
}

void UART3_SendChar(char c) {
    while (!(LPC_U3LSR & (1UL << 5))) { /* wait for THR empty */ }
    LPC_U3THR = c;
}

void UART3_SendString(const char *s) {
    while (*s) {
        UART3_SendChar(*s++);
    }
}

// ---------------------------------------------------------------------------
// 16x2 LCD (8-bit parallel, HD44780-compatible)
// ---------------------------------------------------------------------------
static void lcd_pulse_enable(void) {
    LPC_GPIO0_FIOSET = LCD_EN_PIN;
    delay_us(2);
    LPC_GPIO0_FIOCLR = LCD_EN_PIN;
    delay_us(50);
}

static void lcd_write(uint8_t value, uint8_t is_data) {
    if (is_data) {
        LPC_GPIO0_FIOSET = LCD_RS_PIN;
    } else {
        LPC_GPIO0_FIOCLR = LCD_RS_PIN;
    }

    LPC_GPIO0_FIOCLR = LCD_DATA_MASK;
    LPC_GPIO0_FIOSET = ((uint32_t)value << LCD_DATA_SHIFT) & LCD_DATA_MASK;

    lcd_pulse_enable();
}

void lcd_command(uint8_t cmd) { lcd_write(cmd, 0); }
void lcd_data(uint8_t data)   { lcd_write(data, 1); }

void lcd_init(void) {
    delay_ms(20);           // wait for LCD power-up
    lcd_command(0x38);      // function set: 8-bit, 2-line, 5x8 font
    lcd_command(0x0C);      // display ON, cursor OFF
    lcd_command(0x06);      // entry mode: increment cursor
    lcd_command(0x01);      // clear display
    delay_ms(2);
}

void lcd_clear(void) {
    lcd_command(0x01);
    delay_ms(2);
}

void lcd_goto(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_command(address);
}

void lcd_string(const char *str) {
    while (*str) {
        lcd_data((uint8_t)*str++);
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    float distance;
    uint16_t turbidity_ntu;
    float phValue;
    float do_value;
    float temperature = 0, humidity = 0;

    DIO_Init();
    Timer0_Init();
    init_uart0();
    UART3_Init();
    lcd_init();
    ADC_Init();

    lcd_clear();
    lcd_goto(0, 0);
    lcd_string("Multi-Sensor");
    lcd_goto(1, 0);
    lcd_string("System Ready");
    delay_ms(2000);

    while (1) {
        // ---- Read DHT11 ----
        if (dht11_read(&temperature, &humidity)) {
            snprintf(buffer, sizeof(buffer), "{\"temp\":%.1f,\"hum\":%.1f}\n", temperature, humidity);
            UART3_SendString(buffer);
        }

        // ---- Read Ultrasonic (water level) ----
        distance = get_ultrasonic_distance();
        snprintf(buffer, sizeof(buffer), "{\"dist\":%.2f,\"level\":\"", distance);
        UART3_SendString(buffer);
        if (distance >= 25)      UART3_SendString("LOW");
        else if (distance >= 15) UART3_SendString("MEDIUM");
        else                     UART3_SendString("HIGH");
        UART3_SendString("\"}\n");
        delay_ms(2500);

        // ---- Read Turbidity ----
        turbidity_ntu = read_turbidity();
        snprintf(buffer, sizeof(buffer), "{\"turb\":%d,\"status\":\"", turbidity_ntu);
        UART3_SendString(buffer);
        if (turbidity_ntu <= 30)      UART3_SendString("EXCELLENT");
        else if (turbidity_ntu <= 50) UART3_SendString("GOOD");
        else                          UART3_SendString("WARNING");
        UART3_SendString("\"}\n");
        delay_ms(2500);

        // ---- Read pH ----
        phValue = read_ph_sensor();
        snprintf(buffer, sizeof(buffer), "{\"ph\":%.2f,\"status\":\"", phValue);
        UART3_SendString(buffer);
        if (phValue < 6)       UART3_SendString("ACIDIC");
        else if (phValue <= 8) UART3_SendString("NEUTRAL");
        else                   UART3_SendString("ALKALINE");
        UART3_SendString("\"}\n");
        delay_ms(2500);

        // ---- Read Dissolved Oxygen ----
        do_value = DO_ConvertToMgL(DO_GetVoltage(ADC_Read(DO_CHANNEL)));
        snprintf(buffer, sizeof(buffer), "{\"do\":%.2f,\"status\":\"", do_value);
        UART3_SendString(buffer);
        if (do_value < 3.0f)      UART3_SendString("CRITICAL");
        else if (do_value < 6.0f) UART3_SendString("WARNING");
        else                      UART3_SendString("GOOD");
        UART3_SendString("\"}\n");

        // ---- Update LCD with the latest readings ----
        lcd_clear();
        lcd_goto(0, 0);
        snprintf(buffer, sizeof(buffer), "T:%.1fC H:%.0f%%", temperature, humidity);
        lcd_string(buffer);
        lcd_goto(1, 0);
        snprintf(buffer, sizeof(buffer), "pH:%.1f DO:%.1f", phValue, do_value);
        lcd_string(buffer);

        // ---- Also print a human-readable line to the PC monitor ----
        snprintf(buffer, sizeof(buffer), "Temp:%.1fC Hum:%.1f%% Dist:%.1fcm Turb:%dNTU pH:%.2f DO:%.2f\r\n",
                 temperature, humidity, distance, turbidity_ntu, phValue, do_value);
        UART0_SendString(buffer);

        delay_ms(2500);
    }
}
