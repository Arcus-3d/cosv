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

#ifndef __VISP_H__
#define __VISP_H__

//  Looking at the component side of the PCB
// +-----------------------+
// |                       |
// |                       |
// |                       |
// |                       |
// |                       |
// |                       |
// |                       |
// |                       |
// |      U7       U6      |
// |                       |
// |           0           |
// |                       |
// |      U8       U5      |
// |                       |
// |                       |
// +-----------------------+

// Dual I2C VISP:
// U5 & U6 are on SDA1/SCK1
// U7 & U8 are on SDA2/SCK2
//
// MUX VISP
// U5 & U6 are on Mux Bus #1
// U7 & U8 are on Mux Bus #2
//
// U5 & U7 = 0x76
// U6 & U8 = 0x77
// Index mapping of the sensors to the board (See schematic)
#define SENSOR_U5 0
#define SENSOR_U6 1
#define SENSOR_U7 2
#define SENSOR_U8 3

typedef enum {
  VISP_BUS_TYPE_NONE = ' ',
  VISP_BUS_TYPE_SPI = 's',
  VISP_BUS_TYPE_I2C = 'i',
  VISP_BUS_TYPE_MUX = 'm',
  VISP_BUS_TYPE_XLATE = 'x'
} vispBusType_e;

typedef enum {
  VISP_BODYTYPE_UNKNOWN = 0,
  VISP_BODYTYPE_PITOT = 'p',
  VISP_BODYTYPE_VENTURI = 'v',
  VISP_BODYTYPE_HYBRID = 'h',
  VISP_BODYTYPE_EXPERIMENTAL = 'x'
} vispBodyType_e;


// EEPROM is 24c01 128x8
typedef struct sensor_mapping_s {
  uint8_t mappingType; // In case we want to change this in the future, provide a mechanism to detect this
  uint8_t i2cAddress;  // If non zero, then it's an I2C device.  Otherwise, we assume SPI.
  uint8_t muxAddress; // 0 if not behind a mux
  uint8_t busNumber; //  If MUXed, then it's the muxChannel.  If straight I2C, then it could be multiple I2C channels, if SPI, then it's the enable pin on the VISP
}  sensor_mapping_t;


// First 32-bytes is configuration
// WARNING: Must be a multiple of the eeprom's write page.   Assume 8-byte multiples
// This is at address 0 in the eeprom
typedef struct visp_eeprom_s {
  uint32_t VISP;  // LETTERS VISP
  uint8_t busType;  // Character type "M"=mux "X"=XLate, "D"=Dual "S"=SPI
  uint8_t bodyType; // "V"=Venturi "P"=Pitot "H"=Hybrid
  uint8_t bodyVersion; // printable character
  uint8_t zero;  // NULL Terminator, so we can Serial.print(*eeprom);
  uint8_t extra[2];
  sensor_mapping_t sensorMapping[4]; // 16 bytes for sensor mapping.
  uint16_t breath_pressure; // For pressure controlled automatic ventilation
  uint16_t breath_volume;
  uint8_t breath_rate;
  uint8_t breath_ratio;
  uint16_t breath_threshold;
  uint16_t motor_speed;    // For demonstration purposes, run motor at a fixed speed...
  uint8_t extra2[26];
  uint16_t checksum; // TODO: future, paranoia about integrity


  // This is VISP specific (venturi/pitot/etc).
  uint8_t visp_calibration[64];

} visp_eeprom_t; // WARNING: Must be a multiple of the eeprom's write page.   Assume (EEPROM_PAGE_SIZE) multiples


extern visp_eeprom_t visp_eeprom;

extern float calibrationOffsets[4]; // Used by dataSend in command.cpp

extern float ambientPressure, throatPressure;
extern float pressure; // Used for PC-CMV
extern float volume; // Used for VC-CMV
extern float tidalVolume; // Maybe used for VC-CMV definitely safety limits

extern baroDev_t sensors[4]; // See mappings SENSOR_U[5678] and PATIENT_PRESSURE, AMBIENT_PRESSURE, PITOT1, PITOT2
extern bool sensorsFound ;

void vispInit();

void handleSensorFailure();
void detectEEPROM(TwoWire * wire, uint8_t address, uint8_t muxChannel = 0, busDevice_t *muxDevice = NULL, busDeviceEnableCbk enableCbk = noEnableCbk);
bool detectMuxedSensors(TwoWire *wire, busDeviceEnableCbk enableCbk = noEnableCbk);
bool detectXLateSensors(TwoWire * wire, busDeviceEnableCbk enableCbk = noEnableCbk);
bool detectDualI2CSensors(TwoWire * wireA, TwoWire * wireB, busDeviceEnableCbk enableCbkA = noEnableCbk, busDeviceEnableCbk enableCbkB = noEnableCbk);
void sanitizeVispData();
void detectVISP(TwoWire * i2cBusA, TwoWire * i2cBusB, busDeviceEnableCbk enableCbkA = noEnableCbk, busDeviceEnableCbk enableCbkB = noEnableCbk);
void saveParametersToVISP();

void calibrateClear();
bool calibrateInProgress();
void calibrateSensors();
void calibrateApply();

void calculatePitotValues();
void calculateVenturiValues();
void calculateTidalVolume();

#endif
