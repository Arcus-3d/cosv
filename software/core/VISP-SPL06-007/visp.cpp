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

float calibrationOffsets[4];

float ambientPressure = 0.0, throatPressure = 0.0;
float pressure = 0.0; // Used for PC-CMV
float volume = 0.0; // Used for VC-CMV
float tidalVolume = 0.0; // Maybe used for VC-CMV definitely safety limits

uint8_t calibrationSampleCounter = 0;
#define CALIBRATION_FINISHED 99

vispBusType_e detectedVispType = VISP_BUS_TYPE_NONE;

baroDev_t sensors[4]; // See mappings SENSOR_U[5678] and PATIENT_PRESSURE, AMBIENT_PRESSURE, PITOT1, PITOT2
bool sensorsFound = false;

void vispInit()
{
  memset(&sensors, 0, sizeof(sensors));
  ambientPressure = 0.0, throatPressure = 0.0;
  pressure = 0.0; // Used for PC-CMV
  volume = 0.0; // Used for VC-CMV
  tidalVolume = 0.0; // Maybe used for VC-CMV definitely safety limits
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

  //  // Make *sure* that the calibration data is formatted properly
  //  calibrateClear();
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
  missing = 0;
  for (int x = 0; x < 4; x++)
    missing |= (sensors[x].busDev ? 0 : 1<<x);
    
  if (missing)
  {
    warning(PSTR("Sensors missing 0x%x"), missing);
    return;
  }

  if (detectedVispType == VISP_BUS_TYPE_I2C) debug(PSTR("DUAL I2C%S"), strBasedType);
  else if (detectedVispType == VISP_BUS_TYPE_XLATE) debug(PSTR("XLate%S"), strBasedType);
  else if (detectedVispType == VISP_BUS_TYPE_MUX)  debug(PSTR("MUX%S"), strBasedType);
  else if (detectedVispType == VISP_BUS_TYPE_SPI) debug(PSTR("SPI%S"), strBasedType);

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

  sanitizeVispData();

  sensorsFound = true;

  // Just put it out there, what type we are for the status system to figure out
  info(PSTR("Sensors detected"));
  sendCurrentSystemHealth();
}

void calibrateClear()
{
  calibrationSampleCounter = 0;
  memset(&calibrationOffsets, 0, sizeof(calibrationOffsets));
}


void calibrateApply(float * P)
{
  for (int x = 0; x < 4; x++)
    P[x] += calibrationOffsets[x];
}

void calibrateSensors(float * P)
{
  int x;
  if (calibrationSampleCounter == 1)
    respond('C', PSTR("0,Starting Calibration"));
  for (x = 0; x < 4; x++)
    calibrationOffsets[x] += P[x];
  calibrationSampleCounter++;
  if (calibrationSampleCounter == CALIBRATION_FINISHED) {
    float average = 0.0;
    for (x = 0; x < 4; x++)
      average += calibrationOffsets[x];
    average /= 400.0;

    for (x = 0; x < 4; x++)
      calibrationOffsets[x] = average - calibrationOffsets[x] / 100.0;
    respond('C', PSTR("2,Calibration Finished"));
  }
}

bool calibrateInProgress()
{
  return (calibrationSampleCounter < CALIBRATION_FINISHED);
}



// Use these definitions to map sensors output P[SENSOR_Ux] to their usage
#define THROAT_PRESSURE  SENSOR_U5
#define AMBIANT_PRESSURE SENSOR_U6
#define PITOT1           SENSOR_U7
#define PITOT2           SENSOR_U8

void calculatePitotValues(float * P)
{
  const float paTocmH2O = 0.0101972;
  float  airflow, roughVolume, pitot_diff, pitot1, pitot2;

  pitot1 = P[PITOT1];
  pitot2 = P[PITOT2];
  ambientPressure = P[AMBIANT_PRESSURE];
  throatPressure = P[THROAT_PRESSURE];
  pressure = (throatPressure - ambientPressure) * paTocmH2O;

  pitot_diff = (pitot1 - pitot2) / 100.0; // pascals to hPa

  airflow = ((0.05 * pitot_diff * pitot_diff) - (0.0008 * pitot_diff)); // m/s
  //airflow=sqrt(2.0*pitot_diff/2.875);
  if (pitot_diff < 0) {
    airflow = -airflow;
  }
  //airflow = -(-0.0008+sqrt(0.0008*0.0008-4*0.05*0.0084+4*0.05*pitot_diff))/2*0.05;

  // TODO: smoothing for Pitot version
  volume = roughVolume = airflow * 0.25 * 60.0; // volume for our 18mm orfice, and 60s/min
}


// Use these definitions to map sensors output P[SENSOR_Ux] to their usage
//U7 is input tube, U8 is output tube, U5 is venturi, U6 is ambient
#define VENTURI_SENSOR  SENSOR_U5
#define VENTURI_AMBIANT SENSOR_U6
#define VENTURI_INPUT   SENSOR_U7
#define VENTURI_OUTPUT  SENSOR_U8
void calculateVenturiValues(float * P)
{
  const float paTocmH2O = 0.0101972;
  // venturi calculations
  const float aPipe = 232.35219306;
  const float aRestriction = 56.745017403;
  const float a_diff = (aPipe * aRestriction) / sqrt((aPipe * aPipe) - (aRestriction * aRestriction)); // area difference
  float roughVolume, inletPressure, outletPressure;

  //    static float paTocmH2O = 0.00501972;
  ambientPressure = P[VENTURI_AMBIANT];
  inletPressure = P[VENTURI_INPUT];
  outletPressure = P[VENTURI_OUTPUT];
  // patientPressure = P[VENTURI_SENSOR];   // This is not used?
  throatPressure = P[VENTURI_SENSOR];
  pressure = ((inletPressure + outletPressure) / 2.0 - ambientPressure) * paTocmH2O;

  //float h= ( inletPressure-throatPressure )/(9.81*998); //pressure head difference in m
  //airflow = a_diff * sqrt(2.0 * (inletPressure - throatPressure)) / 998.0) * 600000.0; // airflow in cubic m/s *60000 to get L/m
  // Why multiply by 2 then devide by a number, why not just divide by half the number?
  //airflow = a_diff * sqrt((inletPressure - throatPressure) / (449.0*1.2)) * 600000.0; // airflow in cubic m/s *60000 to get L/m


  if (inletPressure > outletPressure && inletPressure > throatPressure)
  {
    roughVolume = a_diff * sqrt((inletPressure - throatPressure) / (449.0 * 1.2)) * 0.6; // instantaneous volume
  }
  else if (outletPressure > inletPressure && outletPressure > throatPressure)
  {
    roughVolume = -a_diff * sqrt((outletPressure - throatPressure) / (449.0 * 1.2)) * 0.6;
  }
  else
  {
    roughVolume = 0.0;
  }
  if (isnan(roughVolume) || abs(roughVolume) < 1.0 )
  {
    roughVolume = 0.0;
  }

  const float alpha = 0.15; // smoothing factor for exponential filter
  volume = roughVolume * alpha + volume * (1.0 - alpha);
}


void calculateTidalVolume()
{
  static unsigned long lastSampleTime = 0;
  unsigned long sampleTime = millis();

  if (lastSampleTime)
  {
    tidalVolume = tidalVolume + volume * (sampleTime - lastSampleTime) / 60 - 0.05; // tidal volume is the volume delivered to the patient at this time.  So it is cumulative.
  }
  if (tidalVolume < 0.0)
  {
    tidalVolume = 0.0;
  }
  lastSampleTime = sampleTime;
}
