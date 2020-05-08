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

#elif ARDUINO_AVR_NANO

#include <avr/io.h>
#include <avr/interrupt.h>
#define hwSerial Serial
#define SERIAL_BAUD 115200

#define PUTINFLASH PROGMEM

#define WANT_BMP388 1 // 1822 bytes
#define WANT_BMP280 1
#define WANT_SPL06  1


#elif ARDUINO_AVR_UNO

#include <avr/io.h>
#include <avr/interrupt.h>
#define hwSerial Serial
#define SERIAL_BAUD 115200

#define PUTINFLASH PROGMEM

#define WANT_BMP388 1
#define WANT_BMP280 1
#define WANT_SPL06  1

#else

#error Unsupported board selection.

#endif




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
#define ADC_HALL     A7 // Future?  Analog HALL sensor for better homing code

#define HOME_SENSOR  2 // IRQ on low.


// All motors use these 5 signals.
#define MOTOR_PIN_A    3 // MUST BE IRQ CAPABLE!  (STEPPER_ENABLE, BLDC_FEEDBACK)  
#define MOTOR_PIN_B    4 // (MOTOR_HBRIDGE_R_EN)
#define MOTOR_PIN_C    5 // (MOTOR_HBRIDGE_L_EN, MOTOR_STEPPER_DIR, MOTOR_BLDC_DIR)
#define MOTOR_PIN_PWM  6 // HARDWARE PWM Capable output required (MOTOR_HBRIDGE_PWM, MOTOR_STEPPER_STEP, MOTOR_BLDC_PWM)

// Pins D4 and D5 are enable pins for NANO's NPN SCL enable pins
#define ENABLE_PIN_BUS_A 7
#define ENABLE_PIN_BUS_B 8

#define MISSING_PULSE_PIN  9   // Output a pulse every time we check the sensors.

// This leaves the entire SPI bus available for a display or alternate IO:  10, 11, 12, and 13


// System modes (This needs to be a bitmask friendly for settings validity checking (setting Volume when in Pressure mode is an error)
#define MODE_NONE  0x00
#define MODE_OFF   0x01
#define MODE_PCCMV 0x02
#define MODE_VCCMV 0x04
#define MODE_MANUAL 0x08 // No display, just some analog dial inputs that adjust breath_rate, I:E ratio, and pressure/volume limit
#define MODE_ALL   0xFF

// rate, i:e, pressure, volume, mode as analog inputs  5 of them...  e have 6 available on the nano!

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
#define MIN_BREATH_PRESSURE 0 // TODO: saner limits
#define MAX_BREATH_PRESSURE 100// TODO: saner limits


void clearCalibrationData();

#define VERSION_MAJOR     0
#define VERSION_MINOR     1
#define VERSION_REVISION  5

#include "respond.h"
#include "busdevice.h"
#include "sensors.h"
#include "visp.h"
#include "eeprom.h"
#include "command.h"
#include "motor.h"

#endif
