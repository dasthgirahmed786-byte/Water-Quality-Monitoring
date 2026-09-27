# Real-Time Water Quality Monitoring System (LPC1768 / ARM Cortex-M3)

Monitors temperature, humidity, water level, turbidity, pH, and dissolved oxygen using an NXP LPC1768 microcontroller, displays live readings on a 16x2 LCD, and streams the data as JSON over UART to an ESP8266/ESP32 module for IoT/cloud monitoring.

Built as part of a Summer Internship (Embedded System Design with ARM Cortex-M3) at SSIT, 2026.

## Important note on this version of the code
The functions specific to this project's calibration logic (`read_turbidity`, `read_ph_sensor`, `ADC_Init`, `ADC_Read`, and the main control flow) are transcribed directly from the original project report. However, several supporting functions were **not included in the report's code excerpts** (only referenced by name) and have been written here to complete the project:
- `dht11_read()` — DHT11 bit-banged protocol implementation
- `get_ultrasonic_distance()` — HC-SR04 timing/distance calculation
- `lcd_init()`, `lcd_command()`, `lcd_data()`, `lcd_goto()`, `lcd_string()` — 16x2 LCD driver
- `init_uart0()`, `UART3_Init()`, and their send functions
- `Timer0_Init()` and the `delay_us()`/`delay_ms()` functions built on it
- `DO_GetVoltage()` / `DO_ConvertToMgL()` — dissolved oxygen conversion (adjust the two calibration constants near the top of `main.c` to match your specific DO probe's datasheet)

The main loop was also **re-ordered** to correctly nest all five sensor reads inside the `while(1)` loop, following the operation flow diagram in the report (the report's raw text extraction had a misplaced closing brace that cut the loop short).

**This has been compiled and verified with zero errors and zero warnings** using `arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Wall -Wextra`.

## Hardware Required
| Component | Quantity |
|---|---|
| LPC1768 development board | 1 |
| DHT11 Temperature/Humidity Sensor | 1 |
| HC-SR04 Ultrasonic Sensor | 1 |
| Turbidity Sensor (analog) | 1 |
| pH Sensor (analog) | 1 |
| Dissolved Oxygen Sensor (analog) | 1 |
| 16x2 LCD (parallel, HD44780-compatible) | 1 |
| ESP8266 or ESP32 module | 1 |

## Pin Configuration
| Component | Pin | LPC1768 Connection |
|---|---|---|
| DHT11 | Data | P0.4 (GPIO) |
| Ultrasonic | TRIG | P0.5 (GPIO Output) |
| Ultrasonic | ECHO | P0.6 (GPIO Input) |
| Turbidity Sensor | Analog Out | P0.23 (AD0.0) |
| pH Sensor | Analog Out | P0.24 (AD0.1) |
| DO Sensor | Analog Out | P0.25 (AD0.2) |
| LCD | RS | P0.10 (GPIO) |
| LCD | EN | P0.11 (GPIO) |
| LCD | Data Bus | P0.15–P0.22 (GPIO) |
| ESP Module | TX | P0.0 (UART3 TXD) |
| ESP Module | RX | P0.1 (UART3 RXD) |

## About "running this in VS Code"
This is important to understand: this is **bare-metal embedded C for an ARM microcontroller**, not a script. It doesn't run like a Python or Node program — there's no operating system underneath it, and VS Code has no built-in way to simulate an LPC1768's peripherals (ADC, LCD, sensors). There are two real ways to actually run/test it:

### Option A — Keil µVision Simulator (recommended, matches your internship tool)
This is what you already used during the internship, and it's the most direct path since Keil's simulator can simulate LPC1768 peripherals and let you watch UART output and GPIO states without any physical hardware:
1. Install Keil µVision (MDK-ARM) and use the Pack Installer to add the **LPC1700 device family pack** — this gives you the official `LPC17xx.h` CMSIS header and startup files that a real project needs (this repo's `lpc17xx_regs.h` is a minimal stand-in for compile-testing only, not a replacement for these).
2. Create a new project targeting LPC1768, add `main.c`, and replace the `#include "lpc17xx_regs.h"` line with `#include "LPC17xx.h"` (the official header uses slightly different macro names like `LPC_GPIO0->FIODIR` as struct members rather than flat `#define`s — Keil's header supports both this project's original style and the flat style, but double check against the header once installed).
3. Build, then use Debug → Start/Stop Debug Session with the simulator target to step through and watch the UART0 output window.

### Option B — Real Hardware
Flash the compiled `.hex`/`.bin` to an actual LPC1768 board via a programmer (e.g. via the onboard USB bootloader or a JTAG/SWD debugger), then observe the LCD and a serial terminal (e.g. PuTTY / Tera Term) connected via a USB-to-UART adapter on UART0's pins.

### What you *can* do in VS Code
You can absolutely write and compile-check this code in VS Code using the ARM GCC toolchain (`arm-none-eabi-gcc`), which is exactly how this version was verified. Install the "ARM" and "C/C++" extensions, then compile with:
```
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Wall -Wextra -c main.c -o main.o
```
This confirms your code is free of syntax/type errors before you ever touch hardware — but it will not "run" and show you sensor output inside VS Code itself.

## How It Works
1. `DIO_Init()`, `Timer0_Init()`, `init_uart0()`, `UART3_Init()`, `lcd_init()`, and `ADC_Init()` configure all peripherals at startup.
2. The main loop repeatedly: reads DHT11 temperature/humidity, measures water level via ultrasonic time-of-flight, samples turbidity/pH/DO via the 12-bit ADC (averaging 10 readings each for stability), applies the calibration formulas from the report to convert raw voltage into meaningful units, updates the LCD, and streams everything as JSON over UART3 to the ESP module (with a plain-text summary also sent over UART0 for PC monitoring).

## License
No license. All rights reserved by the organization.