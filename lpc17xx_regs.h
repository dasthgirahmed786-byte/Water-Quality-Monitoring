/*
 * lpc17xx_regs.h
 *
 * Minimal LPC1768 peripheral register definitions - only the peripherals
 * actually used by this project (GPIO0, System Control, Pin Connect, ADC,
 * UART0, UART3, Timer0).
 *
 * NOTE: In a real Keil uVision project, you would normally use NXP's
 * official CMSIS device header (LPC17xx.h) obtained via the Keil Pack
 * Installer instead of this file. This minimal version exists so the
 * application code below can be compiled and checked for errors
 * independently of that vendor package. See the README for details.
 */

#ifndef LPC17XX_REGS_H
#define LPC17XX_REGS_H

#include <stdint.h>

// ---- System Control Block ----
#define LPC_SC_BASE     0x400FC000UL
#define LPC_SC_PCONP    (*(volatile uint32_t *)(LPC_SC_BASE + 0xC4))

// ---- Pin Connect Block ----
#define LPC_PINCON_BASE     0x4002C000UL
#define LPC_PINSEL0         (*(volatile uint32_t *)(LPC_PINCON_BASE + 0x00))
#define LPC_PINSEL1         (*(volatile uint32_t *)(LPC_PINCON_BASE + 0x04))

// ---- GPIO0 (fast GPIO) ----
#define LPC_GPIO0_BASE      0x2009C000UL
#define LPC_GPIO0_FIODIR    (*(volatile uint32_t *)(LPC_GPIO0_BASE + 0x00))
#define LPC_GPIO0_FIOPIN    (*(volatile uint32_t *)(LPC_GPIO0_BASE + 0x14))
#define LPC_GPIO0_FIOSET    (*(volatile uint32_t *)(LPC_GPIO0_BASE + 0x18))
#define LPC_GPIO0_FIOCLR    (*(volatile uint32_t *)(LPC_GPIO0_BASE + 0x1C))

// ---- ADC ----
#define LPC_ADC_BASE        0x40034000UL
#define LPC_ADC_ADCR        (*(volatile uint32_t *)(LPC_ADC_BASE + 0x00))
#define LPC_ADC_ADGDR       (*(volatile uint32_t *)(LPC_ADC_BASE + 0x04))

// ---- UART0 (connected to PC serial monitor) ----
#define LPC_UART0_BASE      0x4000C000UL
#define LPC_U0RBR           (*(volatile uint32_t *)(LPC_UART0_BASE + 0x00))
#define LPC_U0THR           (*(volatile uint32_t *)(LPC_UART0_BASE + 0x00))
#define LPC_U0DLL           (*(volatile uint32_t *)(LPC_UART0_BASE + 0x00))
#define LPC_U0DLM           (*(volatile uint32_t *)(LPC_UART0_BASE + 0x04))
#define LPC_U0FCR           (*(volatile uint32_t *)(LPC_UART0_BASE + 0x08))
#define LPC_U0LCR           (*(volatile uint32_t *)(LPC_UART0_BASE + 0x0C))
#define LPC_U0LSR           (*(volatile uint32_t *)(LPC_UART0_BASE + 0x14))

// ---- UART3 (connected to ESP8266/ESP32 module) ----
#define LPC_UART3_BASE      0x4009C000UL
#define LPC_U3THR           (*(volatile uint32_t *)(LPC_UART3_BASE + 0x00))
#define LPC_U3DLL           (*(volatile uint32_t *)(LPC_UART3_BASE + 0x00))
#define LPC_U3DLM           (*(volatile uint32_t *)(LPC_UART3_BASE + 0x04))
#define LPC_U3FCR           (*(volatile uint32_t *)(LPC_UART3_BASE + 0x08))
#define LPC_U3LCR           (*(volatile uint32_t *)(LPC_UART3_BASE + 0x0C))
#define LPC_U3LSR           (*(volatile uint32_t *)(LPC_UART3_BASE + 0x14))

// ---- Timer0 (used here as a microsecond time base) ----
#define LPC_TIM0_BASE       0x40004000UL
#define LPC_T0IR            (*(volatile uint32_t *)(LPC_TIM0_BASE + 0x00))
#define LPC_T0TCR           (*(volatile uint32_t *)(LPC_TIM0_BASE + 0x04))
#define LPC_T0TC            (*(volatile uint32_t *)(LPC_TIM0_BASE + 0x08))
#define LPC_T0PR            (*(volatile uint32_t *)(LPC_TIM0_BASE + 0x0C))

#endif // LPC17XX_REGS_H
