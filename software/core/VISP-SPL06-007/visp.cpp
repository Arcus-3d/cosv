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

#include "config.h"



// Do your venturi mapping here
#define VENTURI0 0
#define VENTURI1 1
#define VENTURI2 2
#define VENTURI3 3

busDevice_t *eeprom = NULL;
visp_eeprom_t visp_eeprom;

vispBusType_e detectedVispType = VISP_BUS_TYPE_NONE;

baroDev_t sensors[4]; // See mappings SENSOR_U[5678] and PATIENT_PRESSURE, AMBIENT_PRESSURE, PITOT1, PITOT2
bool sensorsFound = false;

void vispInit()
{
  memset(&sensors, 0, sizeof(sensors));
}

void formatVisp(busDevice_t *busDev, struct visp_eeprom_s *data, uint8_t busType, uint8_t bodyType)
{
  memset(data, 0, sizeof(visp_eeprom));

  data->VISP[0] = 'V';
  data->VISP[1] = 'I';
  data->VISP[2] = 'S';
  data->VISP[3] = 'p';
  data->busType = busType;
  data->bodyType = bodyType;
  data->bodyVersion = '0';

  data->debug = DEBUG_DISABLED;

//  // Make *sure* that the calibration data is formatted properly
//  clearCalibrationData();
//
//  sanitizeVispData();

  writeEEPROM(busDev, (unsigned short)0, (unsigned char *)&visp_eeprom, sizeof(visp_eeprom));
}
void saveParametersToVISP()
{
  visp_eeprom.checksum = 0;
  // TODO: compute checksum
  writeEEPROM(eeprom, (unsigned short)0, (unsigned char *)&visp_eeprom, sizeof(visp_eeprom));
}

void handleSensorFailure()
{
  for (uint8_t x = 0; x < 4; x++)
  {
    busDeviceFree(sensors[x].busDev);
    sensors[x].busDev = NULL;
    sensors[x].calculate = NULL;
    sensors[x].sensorType = SENSOR_UNKNOWN;
  }
  sensorsFound = false;
  sendCurrentSystemHealth();
  critical(PSTR("Sensor communication failure"));
}

void detectEEPROM(TwoWire * wire, uint8_t address, uint8_t muxChannel, busDevice_t *muxDevice, int8_t enablePin)
{
  busDevice_t *thisDevice = busDeviceInitI2C(wire, address, muxChannel, muxDevice, enablePin);
  if (busDeviceDetect(thisDevice))
  {
    thisDevice->hwType = HWTYPE_EEPROM;
    eeprom = thisDevice;
  }
  else
    busDeviceFree(thisDevice);
}


bool detectMuxedSensors(TwoWire *wire, int enablePin)
{
  // MUX has a switching chip that can have different adresses (including ones on our devices)
  // Could be 2 paths with 2 sensors each or 4 paths with 1 on each.

  busDevice_t *muxDevice = busDeviceInitI2C(wire, 0x70, 0, NULL, enablePin);
  if (!busDeviceDetect(muxDevice))
  {
    //debug(PSTR("Failed to find MUX on I2C, detecting enable pin"));
    busDeviceFree(muxDevice);
    return false;
  }

  // Assign the device it's correct type
  muxDevice->hwType = HWTYPE_MUX;

  detectEEPROM(wire, 0x54, 1, muxDevice, enablePin);


  // Detect U5, U6
  detectIndividualSensor(&sensors[SENSOR_U5], wire, 0x76, 1, muxDevice, enablePin);
  detectIndividualSensor(&sensors[SENSOR_U6], wire, 0x77, 1, muxDevice, enablePin);
  // Detect U7, U8
  detectIndividualSensor(&sensors[SENSOR_U7], wire, 0x76, 2, muxDevice, enablePin);
  detectIndividualSensor(&sensors[SENSOR_U8], wire, 0x77, 2, muxDevice, enablePin);

  detectedVispType = VISP_BUS_TYPE_MUX;

  // Do not free muxDevice, as it is shared by the sensors
  return true;
}

// TODO: verify mapping
bool detectXLateSensors(TwoWire * wire, int enablePin)
{
  // XLATE version has chips at 0x74, 0x75, 0x76, and 0x77
  // So, if we find 0x74... We are good to go

  busDevice_t *seventyFour = busDeviceInitI2C(wire, 0x74, 0, NULL, enablePin);
  if (!busDeviceDetect(seventyFour))
  {
    busDeviceFree(seventyFour);
    return false;
  }

  detectEEPROM(wire, 0x54, 0, NULL, enablePin);

  // Detect U5, U6
  detectIndividualSensor(&sensors[SENSOR_U5], wire, 0x76, 0, NULL, enablePin);
  detectIndividualSensor(&sensors[SENSOR_U6], wire, 0x77, 0, NULL, enablePin);

  // Detect U7, U8
  detectIndividualSensor(&sensors[SENSOR_U7], wire, 0x74, 0, NULL, enablePin);
  detectIndividualSensor(&sensors[SENSOR_U8], wire, 0x75, 0, NULL, enablePin);

  detectedVispType = VISP_BUS_TYPE_XLATE;

  return true;
}

bool detectDualI2CSensors(TwoWire * wireA, TwoWire * wireB, int enablePinA, int enablePinB)
{
  detectEEPROM(wireA, 0x54);
  if (!eeprom)
  {
    detectEEPROM(wireA, 0x54, 0, NULL, enablePinA);
    if (!eeprom)
    {
      detectEEPROM(wireA, 0x54, 0, NULL, enablePinB);
      if (eeprom)
      {
        TwoWire *swap = wireA;
        wireA = wireB;
        wireB = swap;
        //debug(PSTR("PORTS SWAPPED!  EEPROM DETECTED ON BUS B"));
      }
      //else
      //  debug(PSTR("Failed to detect DUAL I2C VISP"));
      return false;
    }
    //debug(PSTR("Enable pins detected!"));
  }

  // Detect U5, U6
  detectIndividualSensor(&sensors[SENSOR_U5], wireA, 0x76, 0, NULL, enablePinA);
  detectIndividualSensor(&sensors[SENSOR_U6], wireA, 0x77, 0, NULL, enablePinA);

  // TEENSY has dual i2c busses, NANO does not.
  // Detect U7, U8
  if (wireB)
  {
    //debug(PSTR("TEENSY second I2C bus"));
    detectIndividualSensor(&sensors[SENSOR_U7], wireB, 0x76, 0, NULL, enablePinB);
    detectIndividualSensor(&sensors[SENSOR_U8], wireB, 0x77, 0, NULL, enablePinB);
  }
  else
  {
    //debug(PSTR("No second HW I2C, using Primary I2C bus with enable pin"));
    detectIndividualSensor(&sensors[SENSOR_U7], wireA, 0x76, 0, NULL, enablePinB);
    detectIndividualSensor(&sensors[SENSOR_U8], wireA, 0x77, 0, NULL, enablePinB);
  }

  detectedVispType = VISP_BUS_TYPE_I2C;

  return true;
}

void sanitizeVispData()
{
  if (visp_eeprom.breath_ratio > MAX_BREATH_RATIO)
    visp_eeprom.breath_ratio = MAX_BREATH_RATIO;
  if (visp_eeprom.breath_ratio < MIN_BREATH_RATIO)
    visp_eeprom.breath_ratio = MIN_BREATH_RATIO;
  if (visp_eeprom.breath_rate > MAX_BREATH_RATE)
    visp_eeprom.breath_rate = MAX_BREATH_RATE;
  if (visp_eeprom.breath_rate < MIN_BREATH_RATE)
    visp_eeprom.breath_rate = MIN_BREATH_RATE;
  if (visp_eeprom.breath_pressure > MAX_BREATH_PRESSURE)
    visp_eeprom.breath_pressure = MAX_BREATH_PRESSURE;
  if (visp_eeprom.breath_pressure < MIN_BREATH_PRESSURE)
    visp_eeprom.breath_pressure = MIN_BREATH_PRESSURE;
}

const char strBasedType[] PUTINFLASH = " Based VISP Detected"; // Save some bytes in flash
// FUTURE: read EEPROM and determine what type of VISP it is.
void detectVISP(TwoWire * i2cBusA, TwoWire * i2cBusB, int enablePinA, int enablePinB)
{
  bool format = false;
  uint8_t missing;
  memset(&sensors, 0, sizeof(sensors));

  debug(PSTR("Detecting sensors"));

  if (!detectMuxedSensors(i2cBusA, enablePinA))
  if (!detectMuxedSensors(i2cBusA, enablePinB))
    if (!detectXLateSensors(i2cBusA, enablePinA))
    if (!detectXLateSensors(i2cBusA, enablePinB))
      if (!detectDualI2CSensors(i2cBusA, i2cBusB, enablePinA, enablePinB))
        return;

  // Make sure they are all there
  missing = (!sensors[0].busDev ? 1 : 0)
            | (!sensors[1].busDev ? 2 : 0)
            | (!sensors[2].busDev ? 4 : 0)
            | (!sensors[3].busDev ? 8 : 0);
  if (missing)
  {
    warning(PSTR("Sensors missing 0x%x"), missing);
    return;
  }

  switch (detectedVispType) {
    case VISP_BUS_TYPE_I2C:   debug(PSTR("DUAL I2C%S"), strBasedType); break;
    case VISP_BUS_TYPE_XLATE: debug(PSTR("XLate%S"), strBasedType);    break;
    case VISP_BUS_TYPE_MUX:   debug(PSTR("MUX%S"), strBasedType);      break;
    case VISP_BUS_TYPE_SPI:   debug(PSTR("SPI%S"), strBasedType);      break;
    case VISP_BUS_TYPE_NONE:  break;
  }

  if (eeprom)
  {
    //debug(PSTR("Reading VISP EEPROM"));
    readEEPROM(eeprom, 0, (unsigned char *)&visp_eeprom, sizeof(visp_eeprom));

    if (visp_eeprom.VISP[0] != 'V'
        ||  visp_eeprom.VISP[1] != 'I'
        ||  visp_eeprom.VISP[2] != 'S'
        ||  visp_eeprom.VISP[3] != 'p')
    {
      // ok, unformatted VISP
      format = true;
      warning(PSTR("ERROR eeprom not formatted"));
    }
  }
  else
  {
    warning(PSTR("ERROR eeprom not available"));
    // Actually, just provide soem sane numbers for the system
    format = true;
  }

  if (format)
    formatVisp(eeprom, &visp_eeprom, detectedVispType, VISP_BODYTYPE_VENTURI);


  // We store the calibration data in the VISP eeprom variable to conserve ram space
  // We only have 2KB on an Arduino NANO/UNO
  clearCalibrationData();

  sanitizeVispData();

  sensorsFound = true;

  // Just put it out there, what type we are for the status system to figure out
  info(PSTR("Sensors detected"));
  sendCurrentSystemHealth();
}
