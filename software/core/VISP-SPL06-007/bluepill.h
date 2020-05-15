#ifndef __BLUEPILL_H__
#define __BLUEPILL_H__

#include <Wire.h>
#include <SPI.h>

#define hwSerial Serial1

// Check every pinMode(): the Maple has more modes for GPIO pins.
// For example, make sure to set analog pins to INPUT_ANALOG before reading and PWM pins to PWM before writing.
// The full set of pin modes is documented in the pinMode() reference.

// Modify PWM writes: pinMode() must be set to PWM, the frequency of the PWM pulse configured, and the duty cycle written with up to 16-bit resolution.

// Modify ADC reads: analogRead() takes the full pin number (not 0-5) and returns a full 12-bit reading. The ADC pin must have its pinMode() set to INPUT_ANALOG.

// Possibly convert all Serial-over-USB communications to use SerialUSB instead of a USART serial port. The Maple has a dedicated USB port which is not connected to the USART TX/RX pins in any way.

// Check for peripheral conflicts; changing the configuration of timers and bus speeds for a feature on one header may impact all the features on that hardware “port”.
// For example, changing the timer prescaler to do long PWM pulses could impact I2C communications on nearby headers.

// Bootloader: https://github.com/rogerclarkmelbourne/STM32duino-bootloader
// STLink Flash loader: https://github.com/rogerclarkmelbourne/STM32duino-bootloader
// Linux Mint needs libusb-1.0-0-dev installed for st-link to compile
// Flashing Blue Pill Boards (HW bug fix): https://github.com/rogerclarkmelbourne/Arduino_STM32/wiki/Flashing-Bootloader-for-BluePill-Boards

// Note: Most "generic" STM32F103 boards only have a reset button, and not a user / test button.
// So the bootloader code always configures the Button input pin as PullDown.
// Hence, if a button is not present on the Button pin (Default is PC14), the pin should remain in a LOW state, and the bootloader will assume the Button is not being pressed.

// IMPORTANT! If your board has external hardware attached to pin PC12 and it pulls that pin HIGH,
// you will need to make a new build target for your board which uses a different pin for the Button,
// or change the code to make it ignore the Button.

#define PUTINFLASH

#define WANT_BMP388 1
#define WANT_BMP280 1
#define WANT_SPL06  1

#define MAX_ANALOG 4096
#define MAX_PWM 65536

// http://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU:MSP2807


//The Arduino has 3 timers and 6 PWM output pins. The relation between timers and PWM outputs is:
//
//    Pins 5 and 6: controlled by Timer0   (On the Uno and similar boards, pins 5 and 6 have a frequency of approximately 980 Hz.)
//    Pins 9 and 10: controlled by Timer1  (490 Hz)
//    Pins 11 and 3: controlled by Timer2  (490 Hz)
//
// We should rebalance the PWM's to give Timer1 or Timer2 to AccelStepper.
//
// The Arduino does not have a built-in digital-to-analog converter (DAC),
// but it can pulse-width modulate (PWM) a digital signal to achieve some
// of the functions of an analog output. The function used to output a
// PWM signal is analogWrite(pin, value). pin is the pin number used for
// the PWM output. value is a number proportional to the duty cycle of
// the signal.
// When value = 0, the signal is always off.
// When value = 255, the signal is always on.
// On most Arduino boards, the PWM function is available on pins 3, 5, 6, 9, 10, and 11. The frequency of the PWM signal on most pins is approximately 490 Hz.

// If ADC_MODE is set to zero, then the display controls things
#define ADC_VOLUME   A0 // How much volume when in VC-CMV mode, 0->1023 milliliters
#define ADC_PRESSURE A1 // How much pressure when in PC-CMV mode
#define ADC_RATIO    A2 // How much time to push, scaled to the MIN_BREATH_RATIO to MAX_BREATH_RATIO
#define ADC_RATE     A3 // Breaths Per Minute (Scaled to MIN_BREATH_RATE and MAX_BREATH_RATE)
#define SPI1_CS     PA4 // 3v3 only
#define SPI1_SCK    PA5 // 3v3 only
#define SPI1_MISO   PA6 // 3v3 only
#define SPI1_MOSI   PA7 // 3v3 only
#define ADC_MODE     A8 // Crude mode selection divided down into 3 'modes', 'display-controlled', 'forced-pc-cmv', 'forced-vc-cmv'
#define ADC_BATTERY  A9 // Battery monitoring of portable unit

#define SPI_DCRS PB5

#define SCL1 PB6
#define SDA1 PB7
#define SCL2 PB10
#define SDA2 PB11

// All motors use these 4 signals.
#define MOTOR_PIN_A    PB8 // MUST BE IRQ CAPABLE!  (STEPPER_ENABLE, BLDC_FEEDBACK)  
#define MOTOR_PIN_B    PB3 // (MOTOR_HBRIDGE_R_EN)
#define MOTOR_PIN_C    PB4 // (MOTOR_HBRIDGE_L_EN, MOTOR_STEPPER_DIR, MOTOR_BLDC_DIR)
#define MOTOR_PIN_PWM  PB9 // HARDWARE PWM Capable output required (MOTOR_HBRIDGE_PWM, MOTOR_STEPPER_STEP, MOTOR_BLDC_PWM)

// Serial1 is PA9 and PA10
#define SERIAL_TX    PA9
#define SERIAL_RX    PA10

#define USB_DMINUS   PA11
#define USB_DPLUS    PA12

#define HOME_SENSOR  PA15 // IRQ on low.

#define LINE_POWER_DETECTION PA8 // Future: need to detect line power
#define MISSING_PULSE_PIN  PB12   // If outside circuitry doens't get pulses within a second, then error

// Extra pins PB12 PB14 and PB15
// PC13 has a board level LED on it

#endif
