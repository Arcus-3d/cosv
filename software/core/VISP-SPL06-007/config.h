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

#ifdef ARDUINO_TEENSY40
#include "teensy.h"
#elif ARDUINO_BLUEPILL_F103C8
#include "bluepill.h"
#elif ARDUINO_AVR_NANO
#include "nano.h"
#elif ARDUINO_AVR_UNO
#include "nano.h"
#define WANT_BMP388 1 // 1858 bytes and 112bytes of ram, fits on the UNO but not the Nano right now
#else
#error Unsupported board selection.
#endif

#define SERIAL_BAUD 115200


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
#define VERSION_REVISION  6


// Motor specific configurations

// percentage as 0->100 of MAX_PWM
#define WIPER_SWEEP_SPEED   65 // Ford F150 motor wiper
#define STEPPER_SWEEP_SPEED 50
#define BLDC_SWEEP_SPEED    50

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
#include "display.h"

#endif
