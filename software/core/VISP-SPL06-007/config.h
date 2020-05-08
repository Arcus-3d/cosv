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
#define VERSION_REVISION  4

#include "respond.h"
#include "busdevice.h"
#include "sensors.h"
#include "visp.h"
#include "eeprom.h"
#include "command.h"


#endif
