/*
   This file is part of VISP Core.

   VISP Core is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   VISP Core is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with VISP Core.  If not, see <http://www.gnu.org/licenses/>.

   Author: Steven.Carr@hammontonmakers.org
*/

#ifndef __MY_CONFIG_H__
#define __MY_CONFIG_H__
#include <stdarg.h>
#include <stdint.h>
#include <Wire.h>
#include <SPI.h>

#ifdef ARDUINO_TEENSY40
#include <IntervalTimer.h>
#define hwSerial Serial1
#define SERIAL_BAUD 115200

#define PUTINFLASH

#define WANT_BMP388 1
#define WANT_BMP280 1
#define WANT_SPL06  1

#define MAX_ANALOG 4096
#define MAX_PWM 65536

#elif ARDUINO_BLUEPILL_F103C8

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

#define hwSerial Serial1
#define SERIAL_BAUD 115200

#define PUTINFLASH

#define WANT_BMP388 1
#define WANT_BMP280 1
#define WANT_SPL06  1

#define MAX_ANALOG 4096
#define MAX_PWM 65536

// http://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU:MSP2807

#elif ARDUINO_AVR_NANO

#include <avr/io.h>
#include <avr/interrupt.h>
#define hwSerial Serial
#define SERIAL_BAUD 115200

#define PUTINFLASH PROGMEM

#define WANT_BMP388 1 // 1858 bytes and 112bytes of ram
#define WANT_BMP280 1 // 2306 bytes
#define WANT_SPL06  1 // 1350 bytes

#define MAX_ANALOG 1024
#define MAX_PWM 255

#elif ARDUINO_AVR_UNO

#include <avr/io.h>
#include <avr/interrupt.h>
#define hwSerial Serial
#define SERIAL_BAUD 115200

#define PUTINFLASH PROGMEM

#define WANT_BMP388 1
#define WANT_BMP280 1
#define WANT_SPL06  1

#define MAX_ANALOG 1024
#define MAX_PWM 255

#else

#error Unsupported board selection.

#endif

// Stepper lib is 3122 bytes
#define WANT_STEPPER 1


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
#define ADC_SDA      A4 // Used by I2C
#define ADC_SCL      A5 // Used by I2C
#define ADC_MODE     A6 // Crude mode selection divided down into 3 'modes', 'display-controlled', 'forced-pc-cmv', 'forced-vc-cmv'
#define ADC_BATTERY  A7 // Battery monitoring of portable unit

#define HOME_SENSOR  2 // IRQ on low.


// All motors use these 4 signals.
#define MOTOR_PIN_A    3 // MUST BE IRQ CAPABLE!  (STEPPER_ENABLE, BLDC_FEEDBACK)  
#define MOTOR_PIN_B    4 // (MOTOR_HBRIDGE_R_EN)
#define MOTOR_PIN_C    5 // (MOTOR_HBRIDGE_L_EN, MOTOR_STEPPER_DIR, MOTOR_BLDC_DIR)
#define MOTOR_PIN_PWM  6 // HARDWARE PWM Capable output required (MOTOR_HBRIDGE_PWM, MOTOR_STEPPER_STEP, MOTOR_BLDC_PWM)

// Pins D4 and D5 are enable pins for NANO's NPN SCL enable pins
#define ENABLE_PIN_BUS_A 7 // Future: this toggles HIGH=busA, LOW=busB  NOTE: also is SPI 2.8" display DC/RS 
#define ENABLE_PIN_BUS_B 8

#define LINE_POWER_DETECTION 8 // Future: need to detect line power
#define MISSING_PULSE_PIN  9   // Output a pulse every time we check the sensors.

// This leaves the entire SPI bus available for a display or alternate IO:  10, 11, 12, and 13
// SS=D10
// MOSI=D11
// MISO=D12
// SCL=D13
// 2.8" screen needs another signal...
// DC/RS: LCD register / data selection signal, high level: register, low level: data

// System modes (This needs to be a bitmask friendly for settings validity checking (setting Volume when in Pressure mode is an error)
#define MODE_NONE  0x00
#define MODE_OFF   0x01
#define MODE_PCCMV 0x02
#define MODE_VCCMV 0x04
#define MODE_MANUAL_PCCMV 0x20 // By ADC input
#define MODE_MANUAL_VCCMV 0x40 // By ADC input
#define MODE_ALL   0xFF
#define MODE_MANUAL (MODE_MANUAL_PCCMV|MODE_MANUAL_VCCMV) // Used to disable things from a display interface if the ADC input is turned to a setting
// rate, i:e, pressure, volume, mode as analog inputs  5 of them...  e have 6 available on the nano!

// We can always add an I2C ADC chip for more input capability (couple bucks)

extern uint8_t currentMode;

typedef enum {
  DEBUG_DISABLED = 0,
  DEBUG_ENABLED
} debugState_e;

extern debugState_e debug;

#define MIN_BREATH_RATIO 2
#define MAX_BREATH_RATIO 5
#define MIN_BREATH_RATE  5 // TODO: saner limits 5 breaths/minute
#define MAX_BREATH_RATE  20 // TODO: saner limits 30 breaths/minute
#define MIN_BREATH_PRESSURE 0    // TODO: saner limits
#define MAX_BREATH_PRESSURE 100  // TODO: saner limits
#define MIN_BREATH_VOLUME   0    // TODO: saner limits 
#define MAX_BREATH_VOLUME   1000 // TODO: saner limits

void clearCalibrationData();

#define VERSION_MAJOR     0
#define VERSION_MINOR     1
#define VERSION_REVISION  5


// Motor specific configurations

#define WIPER_SWEEP_SPEED 150 // Ford F150 motor wiper
#define STEPPER_SWEEP_SPEED 128
#define BLDC_SWEEP_SPEED   128

// ok, we can have a simple H bridge  HiLetGo BTS7960
#define MOTOR_HBRIDGE_R_EN    MOTOR_PIN_B   // R_EN: forware drive enable input, high-level enable, low level off  (ACTIVE_HIGH)
#define MOTOR_HBRIDGE_L_EN    MOTOR_PIN_C   // L_EN: Reverse drive enable input, high-level enable, low level off  (ACTIVE HIGH)
#define MOTOR_HBRIDGE_PWM     MOTOR_PIN_PWM // PWM: PWM signal, active high, attach to BOTH LPWM and RPWM
// WARNING: If you enable R_EN and L_EN at the same time, you fry the chip, so always set both to 0 first, then enable delay(1) and then set the direction pin

// We can have a stepper motor - Schmalz easy driver (Does not fry H if used mistakenly)
#define MOTOR_STEPPER_ENABLE  MOTOR_PIN_A  //  enable input, low-level enable, high level off   (ACTIVE LOW)<-- This is important as BLDC detection pulls this pin HIGH and looks for a pull to zero
#define MOTOR_STEPPER_DIR     MOTOR_PIN_C
#define MOTOR_STEPPER_STEP    MOTOR_PIN_PWM

// Same pins as the stepper.   Except that when watching for the FEEDBACK, the pin is high, disabling the stepper
#define MOTOR_BLDC_FEEDBACK   MOTOR_PIN_A  // Must be IRQ capable
#define MOTOR_BLDC_DIR        MOTOR_PIN_C
#define MOTOR_BLDC_PWM        MOTOR_PIN_PWM



#include "respond.h"
#include "busdevice.h"
#include "sensors.h"
#include "visp.h"
#include "eeprom.h"
#include "command.h"
#include "motor.h"
#include "stepper.h"

#endif
