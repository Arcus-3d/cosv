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



#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <Wire.h>
#include <SPI.h>
#include <AccelStepper.h>


#define VERSION_MAJOR     0
#define VERSION_MINOR     1
#define VERSION_REVISION 'd'

#ifdef ARDUINO_TEENSY40

#include <IntervalTimer.h>
#define hwSerial Serial1
TwoWire *i2cBus1 = &Wire1;
TwoWire *i2cBus2 = &Wire2;
#define SERIAL_BAUD 115200

#define PUTINFLASH
#define SFLASH "%s"

#elif ARDUINO_AVR_NANO

#include <avr/io.h>
#include <avr/interrupt.h>
#define hwSerial Serial
TwoWire *i2cBus1 = &Wire;
TwoWire *i2cBus2 = NULL;
#define SERIAL_BAUD 115200

#define PUTINFLASH PROGMEM
#define SFLASH "%S"

#elif ARDUINO_AVR_UNO

#include <avr/io.h>
#include <avr/interrupt.h>
#define hwSerial Serial
TwoWire *i2cBus1 = &Wire;
TwoWire *i2cBus2 = NULL;
#define SERIAL_BAUD 115200

#define PUTINFLASH PROGMEM
#define SFLASH "%S"

#else

#error Unsupported board selection.

#endif


// Pins D4 and D5 are enable pins for NANO's NPN SCL enable pins
// This is detected if needed
#define ENABLE_PIN_BUS_A 4
#define ENABLE_PIN_BUS_B 5

#define M_PWM_1 6 // Hardware PWM on a Nano
#define M_PWM_2 9 // Hardware PWM on a Nano
#define M_DIR_1 10
#define M_DIR_2 11 // Daren, can you switch this to D12 so I can have the PWM from 11 for BLDC_PWM

#define STEPPER_DIR  7
#define STEPPER_STEP 8

#define BLDC_DIR     13 // 
#define BLDC_PWM     11 // Hardware PWM output for BLDC motor
#define BLDC_FEEDBACK 3 // Interrupt pin, let's us know the core motor has done 1 Rev

// If all of the ADC's are set to zero, then the display controls things
#define ADC_VOLUME   A0 // How much to push, 0->1023 milliliters, if it reads 0, then display can control this (parameter)
#define ADC_RATE     A1 // How much time to push, 0->1023.   Scaled to 1/2 the interval???
#define ADC_INTERVAL A2 // When to timeout and trigger a new breath.   0..1023 is scaled from 0 to 3 seconds.  0=display controls the parameter
#define ADC_HALL     A3 // Used for sensing home with Hall Effect Sensors
#define ADC_SDA      A4
#define ADC_SCL      A5
#define ADC_UNKNOWN1 A6
#define ADC_UNKNOWN2 A7


#define MISSING_PULSE_PIN  2   // Output a pulse every time we check the sensors.

// We only have ADC inputs A6 and A7 left...

AccelStepper mystepper(1, STEPPER_DIR, STEPPER_STEP);

// DUAL I2C VISP on a CPU with 1 I2C Bus using NPN transistors
//
// Simple way to make both I2C buses use a single I2C port
// Use an NPN transistor to permit the SCL line to pull SCL to GND
//
// NOTE: SCL1 & SCL2 have pullups on the VISP
//
//                                       / ------  SCL1 to VISP
//                                      /
// ENABLE_PIN_BUS_A  --- v^v^v^----  --|
//                        10K           V
//                                       \------  SCL from NANO
//
//                                       / ------  SCL2 to VISP
//                                      /
// ENABLE_PIN_BUS_B  --- v^v^v^----  --|
//                        10K           V
//                                       \------  SCL from NANO
//
// Connect BOTH SDA1 and SDA2 together to the SDA
//
//   SDA on NANO --------------+-- SDA1 to the VISP
//                             |
//                             +-- SDA2 to the VISP
//
// Shamelessly swiped from: https://i.stack.imgur.com/WnsM0.png


typedef enum modeState_e {
  MODE_UNKNOWN = 0,
  MODE_PCCMV = 1,
  MODE_VCCMV = 2
} modeState_t;

#define DEBUG_DISABLED 0
#define DEBUG_ENABLED  1


// EEPROM is 24c01 128x8
typedef struct sensor_mapping_s {
  uint8_t mappingType; // In case we want to change this in the future, provide a mechanism to detect this
  uint8_t i2cAddress;  // If non zero, then it's an I2C device.  Otherwise, we assume SPI.
  uint8_t muxAddress; // 0 if not behind a mux
  uint8_t busNumber; //  If MUXed, then it's the muxChannel.  If straight I2C, then it could be multiple I2C channels, if SPI, then it's the enable pin on the VISP
}  sensor_mapping_t;

#define SENSOR_UNKNOWN 0
#define SENSOR_BMP388  1
#define SENSOR_BMP280  2
#define SENSOR_SPL06   3

void handleSensorFailure();

// We are using the M24C01 128 byte eprom
#define EEPROM_PAGE_SIZE 16

// First 32-bytes is configuration
// WARNING: Must be a multiple of the eeprom's write page.   Assume 8-byte multiples
// This is at address 0 in the eeprom
typedef struct visp_eeprom_s {
  char VISP[4];  // LETTERS VISP
  uint8_t busType;  // Character type "M"=mux "X"=XLate, "D"=Dual "S"=SPI
  uint8_t bodyType; // "V"=Venturi "P"=Pitot "H"=Hybrid
  uint8_t bodyVersion; // printable character
  uint8_t zero;  // NULL Terminator, so we can Serial.print(*eeprom);
  uint8_t mode;
  uint8_t debug;
  uint8_t extra[2];
  sensor_mapping_t sensorMapping[4]; // 16 bytes for sensor mapping.
  float calibrationOffsets[4]; // 4*4=16 bytes
  uint16_t volume;
  uint16_t rate;
  uint16_t breath_interval;
  uint16_t breath_threshold;
  uint16_t motor_speed;    // For demonstration purposes, run motor at a fixed speed...
  uint8_t extra2[8];
  uint16_t checksum; // TODO: future, paranoia about integrity


  // This is VISP specific (venturi/pitot/etc).
  uint8_t visp_calibration[64];

} visp_eeprom_t; // WARNING: Must be a multiple of the eeprom's write page.   Assume (EEPROM_PAGE_SIZE) multiples

uint8_t calibrationSampleCounter = 0;

//static assert(sizeof(visp_eeprom_t) == 128, "visp_eeprom must be 128 bytes");
visp_eeprom_t visp_eeprom;

typedef enum {
  VISP_BUS_TYPE_NONE = ' ',
  VISP_BUS_TYPE_SPI = 's',
  VISP_BUS_TYPE_I2C = 'i',
  VISP_BUS_TYPE_MUX = 'm',
  VISP_BUS_TYPE_XLATE = 'x'
} vispBusType_e;

vispBusType_e detectedVispType = VISP_BUS_TYPE_NONE;

typedef enum {
  VISP_BODYTYPE_UNKNOWN = 0,
  VISP_BODYTYPE_PITOT = 'p',
  VISP_BODYTYPE_VENTURI = 'v',
  VISP_BODYTYPE_HYBRID = 'h',
  VISP_BODYTYPE_EXPERIMENTAL = 'x'
} vispBodyType_e;


// Chip type detection
#define DETECTION_MAX_RETRY_COUNT   2



typedef enum {
  RUNSTATE_CALIBRATE = 0,
  RUNSTATE_RUN
} runState_e;

runState_e runState = RUNSTATE_CALIBRATE;

typedef enum {
  BUSTYPE_ANY  = 0,
  BUSTYPE_NONE = 0,
  BUSTYPE_I2C  = 1,
  BUSTYPE_SPI  = 2
} busType_e;

typedef enum {
  HWTYPE_NONE = 0,
  HWTYPE_SENSOR  = 1,
  HWTYPE_MUX  = 2,
  HWTYPE_EEPROM = 3
} hwType_e;

typedef struct busDevice_s {
  busType_e busType;
  hwType_e hwType;
  uint8_t currentChannel; // If this device is a HWTYPE_MUX
  uint8_t refCount; // Reference count (mux is used by multiple sources)
  union {
    struct {
      SPIClass *spiBus;            // SPI bus
      uint8_t csnPin;         // IO for CS# pin
    } spi;
    struct {
      TwoWire *i2cBus;        // I2C bus ID
      uint8_t address;        // I2C bus device address
      uint8_t channel;        // MUXed I2C Channel
      struct busDevice_s *channelDev; // MUXed I2C Channel Address
      int8_t enablePin;
    } i2c;
  } busdev;
} busDevice_t;


// Simple bus device for initially reading the VISP eeprom
busDevice_t *eeprom = NULL;

typedef struct  bmp388_calib_param_s {
  float param_T1;
  float param_T2;
  float param_T3;
  float param_P1;
  float param_P2;
  float param_P3;
  float param_P4;
  float param_P5;
  float param_P6;
  float param_P7;
  float param_P8;
  float param_P9;
  float param_P10;
  float param_P11;
} bmp388_calib_param_t;


typedef struct bmp280_calib_param_s {
  uint16_t dig_T1; /* calibration T1 data */
  int16_t dig_T2; /* calibration T2 data */
  int16_t dig_T3; /* calibration T3 data */
  uint16_t dig_P1; /* calibration P1 data */
  int16_t dig_P2; /* calibration P2 data */
  int16_t dig_P3; /* calibration P3 data */
  int16_t dig_P4; /* calibration P4 data */
  int16_t dig_P5; /* calibration P5 data */
  int16_t dig_P6; /* calibration P6 data */
  int16_t dig_P7; /* calibration P7 data */
  int16_t dig_P8; /* calibration P8 data */
  int16_t dig_P9; /* calibration P9 data */
  int32_t t_fine; /* calibration t_fine data */
} bmp280_calib_param_t;

// SPL06, address 0x76

typedef struct {
  int16_t c0;
  int16_t c1;
  int32_t c00;
  int32_t c10;
  int16_t c01;
  int16_t c11;
  int16_t c20;
  int16_t c21;
  int16_t c30;
} spl06_coeffs_t;


struct baroDev_s;
typedef bool (*baroOpFuncPtr)(struct baroDev_s * baro);
typedef bool (*baroCalculateFuncPtr)(struct baroDev_s * baro, float *pressure, float *temperature);


typedef struct baroDev_s {
  busDevice_t * busDev;
  baroCalculateFuncPtr calculate;
  uint8_t sensorType; // Used by the interface to get the types of sensors we have
  union {
    struct {
      bmp280_calib_param_t cal;
      // uncompensated pressure and temperature
      int32_t up;
      int32_t ut;
      //error free measurements
      int32_t up_valid;
      int32_t ut_valid;
    } bmp280;
    struct {
      bmp388_calib_param_t cal;
      // uncompensated pressure and temperature
      int32_t up;
      int32_t ut;
      //error free measurements
      int32_t up_valid;
      int32_t ut_valid;
    } bmp388;
    struct {
      spl06_coeffs_t cal;
      // uncompensated pressure and temperature
      int32_t pressure_raw;
      int32_t temperature_raw;
    } spl06;
  } chip;
} baroDev_t;

// This is used in loop() and in handleSensorFailure()
bool sensorsFound = false;
baroDev_t sensors[4]; // See mappings SENSOR_U[5678] and PATIENT_PRESSURE, AMBIENT_PRESSURE, PITOT1, PITOT2


void respond(char command, PGM_P fmt, ...)
{
  char sbuffer[128];


  if (command == 'g' && visp_eeprom.debug == DEBUG_DISABLED)
    return;

  //Declare a va_list macro and initialize it with va_start
  va_list l_Arg;
  va_start(l_Arg, fmt);
  vsnprintf_P(sbuffer, sizeof(sbuffer), fmt, l_Arg);
  va_end(l_Arg);

  hwSerial.print(command);
  hwSerial.print(',');
  hwSerial.print(millis());
  hwSerial.print(',');
  hwSerial.println(sbuffer);
}


// critical alerts should result in the display indicating such (big red X)
// Think sensor communication loss or motor not detected (VISP not attached to bag while motor detection is happening?)
// We also need a way to clear the error.

#define info(...)     respond('i', __VA_ARGS__)
#define debug(...)    respond('g', __VA_ARGS__)
#define warning(...)  respond('w', __VA_ARGS__)
#define critical(...) respond('c', __VA_ARGS__)

// Core system health (we need a way to clear errors, like once the sensors are attached)
// Also we have to add motor failure detection to this.
void sendCurrentSystemHealth()
{
  respond('H', (sensorsFound ? PSTR("good") : PSTR("bad")));
}


uint8_t muxSelectChannel(busDevice_t *busDev, uint8_t channel)
{
  if (busDev && channel > 0 && channel < 5)
  {
    if (busDev->currentChannel != channel)
    {
      uint8_t error;

      busDev->busdev.i2c.i2cBus->beginTransmission(busDev->busdev.i2c.address);
      busDev->busdev.i2c.i2cBus->write(1 << (channel - 1));
      error = busDev->busdev.i2c.i2cBus->endTransmission();
      busDev->currentChannel = channel;
      // delay(1);
      return error;
    }
    return 0;
  }
  else {
    return 0xff;
  }
}

void busPrint(busDevice_t *bus, const char *function)
{
  if (bus == NULL)
  {
    debug(PSTR("" SFLASH "(busDev is NULL)"), function);
    return;
  }
  if (bus->busType == BUSTYPE_I2C)
  {
    debug(PSTR("" SFLASH "(I2C: address=0x%x channel=%d enablePin=%d)"), function, bus->busdev.i2c.address, bus->busdev.i2c.channel, bus->busdev.i2c.enablePin);
    return;
  }
  if (bus->busType == BUSTYPE_SPI)
  {
    debug(PSTR("" SFLASH "(SPI CSPIN=%d)"), function, bus->busdev.spi.csnPin);
    return;
  }
}

busDevice_t *busDeviceInitI2C(TwoWire *wire, uint8_t address, uint8_t channel = 0, busDevice_t *channelDev = NULL, int8_t enablePin = -1, hwType_e hwType = HWTYPE_NONE)
{
  busDevice_t *dev = (busDevice_t *)malloc(sizeof(busDevice_t));
  memset(dev, 0, sizeof(busDevice_t));
  dev->busType = BUSTYPE_I2C;
  dev->hwType = hwType;
  dev->busdev.i2c.i2cBus = wire;
  dev->busdev.i2c.address = address;
  dev->busdev.i2c.channel = channel; // If non-zero, then it is a channel on a TCA9546 at mux
  dev->busdev.i2c.channelDev = channelDev;
  dev->busdev.i2c.enablePin = enablePin;
  dev->refCount = 1;
  if (channelDev)
    channelDev->refCount++;
  return dev;
}

busDevice_t *busDeviceInitSPI(SPIClass *spiBus, uint8_t csnPin, hwType_e hwType = HWTYPE_NONE)
{
  busDevice_t *dev = (busDevice_t *)malloc(sizeof(busDevice_t));
  memset(dev, 0, sizeof(busDevice_t));
  dev->busType = BUSTYPE_SPI;
  dev->hwType = hwType;
  dev->busdev.spi.spiBus = spiBus;
  dev->busdev.spi.csnPin = csnPin;
  dev->refCount = 1;
  return dev;
}

// TODO: check to see if this is a mux...
void busDeviceFree(busDevice_t *dev)
{
  if (dev != NULL)
  {
    dev->refCount--;
    if (dev->refCount == 0)
    {
      // If there is a muxDevice as a part of this channel, go free it.
      // Note: mux devices might be daisy chained and support multiple layers.
      // Consider a Core with a Mux to 2 VISP sensors with MUX's of their own
      if (dev->busType == BUSTYPE_I2C && dev->busdev.i2c.channelDev)
        busDeviceFree(dev->busdev.i2c.channelDev);
      free(dev);
    }
  }
}

// Simply detect if the device is present on this device assignment
bool busDeviceDetect(busDevice_t *busDev)
{
  if (busDev && busDev->busType == BUSTYPE_I2C)
  {
    int error;
    uint8_t address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    // busPrint(busDev, PSTR("Detecting if present"));

    // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, HIGH);
    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);
    error = wire->endTransmission();

    // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, LOW);

    if (error == 0)
      busPrint(busDev, PSTR("DETECTED!!!"));
    //else
    //  busPrint(busDev, PSTR("MISSING..."));
    return (error == 0);
  }
  else
    warning(PSTR("busDeviceDetect() unsupported bustype"));

  return false;
}


bool busReadBuf(busDevice_t *busDev, unsigned short reg, unsigned char *values, uint8_t length)
{
  int error;

  if (busDev != NULL && busDev->busType == BUSTYPE_I2C)
  {
    uint8_t address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, HIGH);

    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);

    if (busDev->hwType == HWTYPE_EEPROM)
      wire->write( (byte) (reg >> 8) );   //high addr byte
    wire->write((uint8_t)(reg & 0xFF));

    if (0 == (error = wire->endTransmission()))
    {
      if (busDev->hwType == HWTYPE_EEPROM)
        delay(10);
      wire->requestFrom(address, length);

      while (wire->available() != length) ; // wait until bytes are ready
      for (uint8_t x = 0; x < length; x++)
        values[x] = wire->read();

      // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
      if (busDev->busdev.i2c.enablePin != -1)
        digitalWrite(busDev->busdev.i2c.enablePin, LOW);

      return true;
    }

    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, LOW);

    //debug(PSTR("endTransmission() returned %s"), error);
    return false;
  }
  return false;
}

bool busWriteBuf(busDevice_t *busDev, unsigned short reg, unsigned char *values, char length)
{
  int error;

  if (busDev != NULL && busDev->busType == BUSTYPE_I2C)
  {
    int address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, HIGH);

    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);
    if (busDev->hwType == HWTYPE_EEPROM)
      wire->write( (byte) (reg >> 8) );   //high addr byte
    wire->write((uint8_t)(reg & 0xFF));
    wire->write(values, length);
    error = wire->endTransmission();

    // EEPROM needs this...
    if (busDev->hwType == HWTYPE_EEPROM && error == 0)
    {
      //wait up to 50ms for the write to complete
      for (uint8_t i = 100; i; --i) {
        delayMicroseconds(500);                     //no point in waiting too fast
        wire->beginTransmission(address);
        wire->write(0);        //high addr byte
        wire->write(0);        //low addr byte
        error = wire->endTransmission();
        if (error == 0) break;
      }
    }

    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, LOW);

    return (error == 0 ? true : false);
  }

  return false;
}


bool busRead(busDevice_t *busDev, unsigned short reg, unsigned char *values)
{
  return busReadBuf(busDev, reg, values, 1);
}
bool busWrite(busDevice_t *busDev, unsigned short reg, unsigned char value)
{
  return busWriteBuf(busDev, reg, &value, 1);
}



bool readEEPROM(busDevice_t *busDev, unsigned short reg, unsigned char *values, uint8_t length)
{
  // loop through the length using EEPROM_PAGE_SIZE intervals
  // The lower level WIRE library has a buffer limitation, so staying in EEPROM_PAGE_SIZE intervals is a good thing
  for (unsigned short x = 0; x < length; x += EEPROM_PAGE_SIZE)
  {
    if (!busReadBuf(busDev, reg + x, &values[x], min(EEPROM_PAGE_SIZE, (length - x))))
      return false;
  }
  return true;
}

bool writeEEPROM(busDevice_t *busDev, unsigned short reg, unsigned char *values, char length)
{
  // loop through the length using EEPROM_PAGE_SIZE intervals
  // Writes must align on page size boundaries (otherwise it wraps to the beginning of the page)
  if (reg % EEPROM_PAGE_SIZE)
    return false;

  for (unsigned short x = 0; x < length; x += EEPROM_PAGE_SIZE)
  {
    if (!busWriteBuf(busDev, reg + x, &values[x], min(EEPROM_PAGE_SIZE, (length - x))))
      return false;
  }
  return true;
}


#define SPL06_I2C_ADDR                         0x76
#define SPL06_DEFAULT_CHIP_ID                  0x10

#define SPL06_PRESSURE_START_REG               0x00
#define SPL06_PRESSURE_LEN                     3       // 24 bits, 3 bytes
#define SPL06_PRESSURE_B2_REG                  0x00    // Pressure MSB Register
#define SPL06_PRESSURE_B1_REG                  0x01    // Pressure middle byte Register
#define SPL06_PRESSURE_B0_REG                  0x02    // Pressure LSB Register
#define SPL06_TEMPERATURE_START_REG            0x03
#define SPL06_TEMPERATURE_LEN                  3       // 24 bits, 3 bytes
#define SPL06_TEMPERATURE_B2_REG               0x03    // Temperature MSB Register
#define SPL06_TEMPERATURE_B1_REG               0x04    // Temperature middle byte Register
#define SPL06_TEMPERATURE_B0_REG               0x05    // Temperature LSB Register
#define SPL06_PRESSURE_CFG_REG                 0x06    // Pressure config
#define SPL06_TEMPERATURE_CFG_REG              0x07    // Temperature config
#define SPL06_MODE_AND_STATUS_REG              0x08    // Mode and status
#define SPL06_INT_AND_FIFO_CFG_REG             0x09    // Interrupt and FIFO config
#define SPL06_INT_STATUS_REG                   0x0A    // Interrupt and FIFO config
#define SPL06_FIFO_STATUS_REG                  0x0B    // Interrupt and FIFO config
#define SPL06_RST_REG                          0x0C    // Softreset Register
#define SPL06_CHIP_ID_REG                      0x0D    // Chip ID Register
#define SPL06_CALIB_COEFFS_START               0x10
#define SPL06_CALIB_COEFFS_END                 0x21

#define SPL06_CALIB_COEFFS_LEN                 (SPL06_CALIB_COEFFS_END - SPL06_CALIB_COEFFS_START + 1)

// TEMPERATURE_CFG_REG
#define SPL06_TEMP_USE_EXT_SENSOR              (1<<7)

// MODE_AND_STATUS_REG
#define SPL06_MEAS_PRESSURE                    (1<<0)  // measure pressure
#define SPL06_MEAS_TEMPERATURE                 (1<<1)  // measure temperature

#define SPL06_MEAS_CFG_CONTINUOUS              (1<<2)
#define SPL06_MEAS_CFG_PRESSURE_RDY            (1<<4)
#define SPL06_MEAS_CFG_TEMPERATURE_RDY         (1<<5)
#define SPL06_MEAS_CFG_SENSOR_RDY              (1<<6)
#define SPL06_MEAS_CFG_COEFFS_RDY              (1<<7)

// INT_AND_FIFO_CFG_REG
#define SPL06_PRESSURE_RESULT_BIT_SHIFT        (1<<2)  // necessary for pressure oversampling > 8
#define SPL06_TEMPERATURE_RESULT_BIT_SHIFT     (1<<3)  // necessary for temperature oversampling > 8

// TMP_RATE (Background mode only)
#define SPL06_SAMPLE_RATE_1   0
#define SPL06_SAMPLE_RATE_2   1
#define SPL06_SAMPLE_RATE_4   2
#define SPL06_SAMPLE_RATE_8   3
#define SPL06_SAMPLE_RATE_16  4
#define SPL06_SAMPLE_RATE_32  5
#define SPL06_SAMPLE_RATE_64  6
#define SPL06_SAMPLE_RATE_128 7

#define SPL06_PRESSURE_SAMPLING_RATE     SPL06_SAMPLE_RATE_64
#define SPL06_PRESSURE_OVERSAMPLING            8
#define SPL06_TEMPERATURE_SAMPLING_RATE     SPL06_SAMPLE_RATE_8
#define SPL06_TEMPERATURE_OVERSAMPLING         1


#define SPL06_MEASUREMENT_TIME(oversampling)   ((2 + lrintf(oversampling * 1.6)) + 1) // ms





int8_t spl06_samples_to_cfg_reg_value(uint8_t sample_rate)
{
  switch (sample_rate)
  {
    case 1: return 0;
    case 2: return 1;
    case 4: return 2;
    case 8: return 3;
    case 16: return 4;
    case 32: return 5;
    case 64: return 6;
    case 128: return 7;
    default: return -1; // invalid
  }
}

int32_t spl06_raw_value_scale_factor(uint8_t oversampling_rate)
{
  switch (oversampling_rate)
  {
    case 1: return 524288;
    case 2: return 1572864;
    case 4: return 3670016;
    case 8: return 7864320;
    case 16: return 253952;
    case 32: return 516096;
    case 64: return 1040384;
    case 128: return 2088960;
    default: return -1; // invalid
  }
}




bool spl06_read_temperature(baroDev_t * baro)
{
  uint8_t data[SPL06_TEMPERATURE_LEN];
  int32_t spl06_temperature;

  bool ack = busReadBuf(baro->busDev, SPL06_TEMPERATURE_START_REG, data, SPL06_TEMPERATURE_LEN);
  if (ack) {
    spl06_temperature = (int32_t)((data[0] & 0x80 ? 0xFF000000 : 0) | (((uint32_t)(data[0])) << 16) | (((uint32_t)(data[1])) << 8) | ((uint32_t)data[2]));
    baro->chip.spl06.temperature_raw = spl06_temperature;
  }
  else
    handleSensorFailure();

  return ack;
}

bool spl06_read_pressure(baroDev_t * baro)
{
  uint8_t data[SPL06_PRESSURE_LEN];
  int32_t spl06_pressure;

  bool ack = busReadBuf(baro->busDev, SPL06_PRESSURE_START_REG, data, SPL06_PRESSURE_LEN);

  if (ack) {
    spl06_pressure = (int32_t)((data[0] & 0x80 ? 0xFF000000 : 0) | (((uint32_t)(data[0])) << 16) | (((uint32_t)(data[1])) << 8) | ((uint32_t)data[2]));
    baro->chip.spl06.pressure_raw = spl06_pressure;
  }
  else
    handleSensorFailure();

  return ack;
}

// Returns temperature in degrees centigrade
float spl06_compensate_temperature(baroDev_t * baro, int32_t temperature_raw)
{
  const float t_raw_sc = (float)temperature_raw / spl06_raw_value_scale_factor(SPL06_TEMPERATURE_OVERSAMPLING);
  const float temp_comp = (float)baro->chip.spl06.cal.c0 / 2 + t_raw_sc * baro->chip.spl06.cal.c1;
  return temp_comp;
}

// Returns pressure in Pascal
float spl06_compensate_pressure(baroDev_t * baro, int32_t pressure_raw, int32_t temperature_raw)
{
  const float p_raw_sc = (float)pressure_raw / spl06_raw_value_scale_factor(SPL06_PRESSURE_OVERSAMPLING);
  const float t_raw_sc = (float)temperature_raw / spl06_raw_value_scale_factor(SPL06_TEMPERATURE_OVERSAMPLING);

  const float pressure_cal = (float)baro->chip.spl06.cal.c00 + p_raw_sc * ((float)baro->chip.spl06.cal.c10 + p_raw_sc * ((float)baro->chip.spl06.cal.c20 + p_raw_sc * baro->chip.spl06.cal.c30));
  const float p_temp_comp = t_raw_sc * ((float)baro->chip.spl06.cal.c01 + p_raw_sc * ((float)baro->chip.spl06.cal.c11 + p_raw_sc * baro->chip.spl06.cal.c21));

  return pressure_cal + p_temp_comp;
}

bool spl06Calculate(baroDev_t * baro, float * pressure, float * temperature)
{

  if (pressure) {
    // Is the pressure ready?
    //if (!(busRead(baro->busDev, SPL06_MODE_AND_STATUS_REG, &sstatus) && (sstatus & SPL06_MEAS_CFG_PRESSURE_RDY)))
    //  return false;   // error reading status or pressure is not ready
    // if (!spl06_read_pressure(baro))
    //  return false;
    if (!spl06_read_pressure(baro))
      return false;
    *pressure = spl06_compensate_pressure(baro, baro->chip.spl06.pressure_raw, baro->chip.spl06.temperature_raw);
  }

  if (temperature) {
    // Is the temperature ready?
    //if (!(busRead(baro->busDev, SPL06_MODE_AND_STATUS_REG, &sstatus) && (sstatus & SPL06_MEAS_CFG_TEMPERATURE_RDY)))
    //  return false;   // error reading status or pressure is not ready
    // if (!spl06_read_temperature(baro))
    //  return false;
    if (!spl06_read_temperature(baro))
      return false;
    *temperature = spl06_compensate_temperature(baro, baro->chip.spl06.temperature_raw);
  }

  return true;
}


bool spl06_read_calibration_coefficients(baroDev_t *baro) {
  uint8_t caldata[SPL06_CALIB_COEFFS_LEN];
  uint8_t sstatus;

  if (!(busRead(baro->busDev, SPL06_MODE_AND_STATUS_REG, &sstatus) && (sstatus & SPL06_MEAS_CFG_COEFFS_RDY)))
    return false;   // error reading status or coefficients not ready

  if (!busReadBuf(baro->busDev, SPL06_CALIB_COEFFS_START, (uint8_t *)&caldata, SPL06_CALIB_COEFFS_LEN)) {
    return false;
  }

  baro->chip.spl06.cal.c0 = (caldata[0] & 0x80 ? 0xF000 : 0) | ((uint16_t)caldata[0] << 4) | (((uint16_t)caldata[1] & 0xF0) >> 4);
  baro->chip.spl06.cal.c1 = ((caldata[1] & 0x8 ? 0xF000 : 0) | ((uint16_t)caldata[1] & 0x0F) << 8) | (uint16_t)caldata[2];
  baro->chip.spl06.cal.c00 = (caldata[3] & 0x80 ? 0xFFF00000 : 0) | ((uint32_t)caldata[3] << 12) | ((uint32_t)caldata[4] << 4) | (((uint32_t)caldata[5] & 0xF0) >> 4);
  baro->chip.spl06.cal.c10 = (caldata[5] & 0x8 ? 0xFFF00000 : 0) | (((uint32_t)caldata[5] & 0x0F) << 16) | ((uint32_t)caldata[6] << 8) | (uint32_t)caldata[7];
  baro->chip.spl06.cal.c01 = ((uint16_t)caldata[8] << 8) | ((uint16_t)caldata[9]);
  baro->chip.spl06.cal.c11 = ((uint16_t)caldata[10] << 8) | (uint16_t)caldata[11];
  baro->chip.spl06.cal.c20 = ((uint16_t)caldata[12] << 8) | (uint16_t)caldata[13];
  baro->chip.spl06.cal.c21 = ((uint16_t)caldata[14] << 8) | (uint16_t)caldata[15];
  baro->chip.spl06.cal.c30 = ((uint16_t)caldata[16] << 8) | (uint16_t)caldata[17];

  return true;
}

bool spl06_configure_measurements(baroDev_t *baro)
{
  uint8_t reg_value;

  reg_value = SPL06_TEMP_USE_EXT_SENSOR | spl06_samples_to_cfg_reg_value(SPL06_TEMPERATURE_OVERSAMPLING);
  reg_value |= SPL06_TEMPERATURE_SAMPLING_RATE << 4;
  if (!busWrite(baro->busDev, SPL06_TEMPERATURE_CFG_REG, reg_value))
    return false;

  reg_value = spl06_samples_to_cfg_reg_value(SPL06_PRESSURE_OVERSAMPLING);
  reg_value |= SPL06_PRESSURE_SAMPLING_RATE << 4;
  if (!busWrite(baro->busDev, SPL06_PRESSURE_CFG_REG, reg_value))
    return false;

  reg_value = 0;
  if (SPL06_TEMPERATURE_OVERSAMPLING > 8)
    reg_value |= SPL06_TEMPERATURE_RESULT_BIT_SHIFT;

  if (SPL06_PRESSURE_OVERSAMPLING > 8)
    reg_value |= SPL06_PRESSURE_RESULT_BIT_SHIFT;

  if (!busWrite(baro->busDev, SPL06_INT_AND_FIFO_CFG_REG, reg_value))
    return false;

  busWrite(baro->busDev, SPL06_MODE_AND_STATUS_REG, SPL06_MEAS_PRESSURE | SPL06_MEAS_TEMPERATURE | SPL06_MEAS_CFG_CONTINUOUS);
  return true;
}


bool spl06Detect(baroDev_t *baro, busDevice_t *busDev)
{
  uint8_t chipId;

  for (int retry = 0; retry < DETECTION_MAX_RETRY_COUNT; retry++) {
    bool ack = busRead(busDev, SPL06_CHIP_ID_REG, &chipId);

    if (ack && chipId == SPL06_DEFAULT_CHIP_ID) {
      busPrint(busDev, PSTR("SPL206 Detected"));

      baro->busDev = busDev;

      if (!(spl06_read_calibration_coefficients(baro) && spl06_configure_measurements(baro))) {
        baro->busDev = NULL;
        return false;
      }
      baro->sensorType = SENSOR_SPL06;
      baro->calculate = spl06Calculate;
      return true;
    }

    delay(100);
  }
  return false;
}




/************************************************/
#define BMP280_I2C_ADDR                      (0x76)
#define BMP280_DEFAULT_CHIP_ID               (0x58)

#define BMP280_CHIP_ID_REG                   (0xD0)  /* Chip ID Register */
#define BMP280_RST_REG                       (0xE0)  /* Softreset Register */
#define BMP280_STAT_REG                      (0xF3)  /* Status Register */
#define BMP280_CTRL_MEAS_REG                 (0xF4)  /* Ctrl Measure Register */
#define BMP280_CONFIG_REG                    (0xF5)  /* Configuration Register */
#define BMP280_PRESSURE_MSB_REG              (0xF7)  /* Pressure MSB Register */
#define BMP280_PRESSURE_LSB_REG              (0xF8)  /* Pressure LSB Register */
#define BMP280_PRESSURE_XLSB_REG             (0xF9)  /* Pressure XLSB Register */
#define BMP280_TEMPERATURE_MSB_REG           (0xFA)  /* Temperature MSB Reg */
#define BMP280_TEMPERATURE_LSB_REG           (0xFB)  /* Temperature LSB Reg */
#define BMP280_TEMPERATURE_XLSB_REG          (0xFC)  /* Temperature XLSB Reg */
#define BMP280_FORCED_MODE                   (0x01)
#define BMP280_NORMAL_MODE                   (0x03)

#define BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG             (0x88)
#define BMP280_PRESSURE_TEMPERATURE_CALIB_DATA_LENGTH       (24)
#define BMP280_DATA_FRAME_SIZE               (6)

#define BMP280_OVERSAMP_SKIPPED          (0x00)
#define BMP280_OVERSAMP_1X               (0x01)
#define BMP280_OVERSAMP_2X               (0x02)
#define BMP280_OVERSAMP_4X               (0x03)
#define BMP280_OVERSAMP_8X               (0x04)
#define BMP280_OVERSAMP_16X              (0x05)

#define BMP280_FILTER_COEFF_OFF               (0x00)
#define BMP280_FILTER_COEFF_2                 (0x01)
#define BMP280_FILTER_COEFF_4                 (0x02)
#define BMP280_FILTER_COEFF_8                 (0x03)
#define BMP280_FILTER_COEFF_16                (0x04)

#define BMP280_STANDBY_MS_1      0x00
#define BMP280_STANDBY_MS_63     0x01
#define BMP280_STANDBY_MS_125    0x02
#define BMP280_STANDBY_MS_250    0x03
#define BMP280_STANDBY_MS_500    0x04
#define BMP280_STANDBY_MS_1000   0x05
#define BMP280_STANDBY_MS_2000   0x06
#define BMP280_STANDBY_MS_4000   0x07

// configure pressure and temperature oversampling, normal sampling mode
#define BMP280_PRESSURE_OSR              (BMP280_OVERSAMP_8X)
#define BMP280_TEMPERATURE_OSR           (BMP280_OVERSAMP_2X)
#define BMP280_MODE                      (BMP280_PRESSURE_OSR << 2 | BMP280_TEMPERATURE_OSR << 5 | BMP280_NORMAL_MODE)

//configure IIR pressure filter
#define BMP280_FILTER                    (BMP280_FILTER_COEFF_OFF)

// configure sampling interval for normal mode
#define BMP280_SAMPLING                  (BMP280_STANDBY_MS_1)

#define T_INIT_MAX                       (20)
// 20/16 = 1.25 ms
#define T_MEASURE_PER_OSRS_MAX           (37)
// 37/16 = 2.3125 ms
#define T_SETUP_PRESSURE_MAX             (10)
// 10/16 = 0.625 ms


bool bmp280GetUp(baroDev_t * baro)
{
  uint8_t data[BMP280_DATA_FRAME_SIZE];

  //read data from sensor (Both pressure and temperature, at once)
  bool ack = busReadBuf(baro->busDev, BMP280_PRESSURE_MSB_REG, data, BMP280_DATA_FRAME_SIZE);

  //check if pressure and temperature readings are valid, otherwise use previous measurements from the moment
  if (ack) {
    baro->chip.bmp280.up = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
    baro->chip.bmp280.ut = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
    baro->chip.bmp280.up_valid = baro->chip.bmp280.up;
    baro->chip.bmp280.ut_valid = baro->chip.bmp280.ut;
  }
  else
    handleSensorFailure();

  return ack;
}

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of "5123" equals 51.23 DegC
// t_fine carries fine temperature as global value
int32_t bmp280CompensateTemperature(baroDev_t * baro, int32_t adc_T)
{
  int32_t var1, var2, T;

  var1 = ((((adc_T >> 3) - ((int32_t)baro->chip.bmp280.cal.dig_T1 << 1))) * ((int32_t)baro->chip.bmp280.cal.dig_T2)) >> 11;
  var2  = (((((adc_T >> 4) - ((int32_t)baro->chip.bmp280.cal.dig_T1)) * ((adc_T >> 4) - ((int32_t)baro->chip.bmp280.cal.dig_T1))) >> 12) * ((int32_t)baro->chip.bmp280.cal.dig_T3)) >> 14;
  baro->chip.bmp280.cal.t_fine = var1 + var2;
  T = (baro->chip.bmp280.cal.t_fine * 5 + 128) >> 8;
  return T;
}

// NOTE: bmp280CompensateTemperature() must be called before this, so that t_fine is computed)
// Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits).
// Output value of "24674867" represents 24674867/256 = 96386.2 Pa = 963.862 hPa
uint32_t bmp280CompensatePressure(baroDev_t * baro, int32_t adc_P)
{
  int64_t var1, var2, p;
  var1 = ((int64_t)baro->chip.bmp280.cal.t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)baro->chip.bmp280.cal.dig_P6;
  var2 = var2 + ((var1 * (int64_t)baro->chip.bmp280.cal.dig_P5) << 17);
  var2 = var2 + (((int64_t)baro->chip.bmp280.cal.dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)baro->chip.bmp280.cal.dig_P3) >> 8) + ((var1 * (int64_t)baro->chip.bmp280.cal.dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)baro->chip.bmp280.cal.dig_P1) >> 33;
  if (var1 == 0) {
    return 0; // avoid exception caused by division by zero
  }

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)baro->chip.bmp280.cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)baro->chip.bmp280.cal.dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)baro->chip.bmp280.cal.dig_P7) << 4);
  return (uint32_t)p;
}

bool bmp280Calculate(baroDev_t * baro, float * pressure, float * temperature)
{
  int32_t t;
  uint32_t p;

  if (!bmp280GetUp(baro))
    return false;

  t = bmp280CompensateTemperature(baro, baro->chip.bmp280.ut); // Must happen before bmp280CompensatePressure() (see t_fine)
  p = bmp280CompensatePressure(baro, baro->chip.bmp280.up);

  if (pressure) {
    *pressure = (p / 256.0);
  }

  if (temperature) {
    *temperature = t / 100.0;
  }

  return true;
}


bool bmp280Detect(baroDev_t *baro, busDevice_t *busDev)
{
  for (int retry = 0; retry < DETECTION_MAX_RETRY_COUNT; retry++) {
    uint8_t chipId = 0;

    bool ack = busRead(busDev, BMP280_CHIP_ID_REG, &chipId);
    if (ack && chipId == BMP280_DEFAULT_CHIP_ID) {
      busPrint(busDev, PSTR("BMP280 Detected"));

      baro->busDev = busDev;

      // read calibration
      busReadBuf(baro->busDev, BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG, (uint8_t *)&baro->chip.bmp280.cal, 24);

      //set filter setting and sample rate
      busWrite(baro->busDev, BMP280_CONFIG_REG, BMP280_FILTER | BMP280_SAMPLING);

      // set oversampling + power mode (forced), and start sampling
      busWrite(baro->busDev, BMP280_CTRL_MEAS_REG, BMP280_MODE);

      baro->sensorType = SENSOR_BMP280;
      baro->calculate = bmp280Calculate;
      return true;
    }

    delay(100);
  }

  return false;
}

#define BMP388_DEFAULT_CHIP_ID                          (0x50) // from https://github.com/BoschSensortec/BMP3-Sensor-API/blob/master/bmp3_defs.h#L130

#define BMP388_CMD_REG                                  (0x7E)
#define BMP388_RESERVED_UPPER_REG                       (0x7D)
// everything between BMP388_RESERVED_UPPER_REG and BMP388_RESERVED_LOWER_REG is reserved.
#define BMP388_RESERVED_LOWER_REG                       (0x20)
#define BMP388_CONFIG_REG                               (0x1F)
#define BMP388_RESERVED_0x1E_REG                        (0x1E)
#define BMP388_ODR_REG                                  (0x1D)
#define BMP388_OSR_REG                                  (0x1C)
#define BMP388_PWR_CTRL_REG                             (0x1B)
#define BMP388_IF_CONFIG_REG                            (0x1A)
#define BMP388_INT_CTRL_REG                             (0x19)
#define BMP388_FIFO_CONFIG_2_REG                        (0x18)
#define BMP388_FIFO_CONFIG_1_REG                        (0x17)
#define BMP388_FIFO_WTM_1_REG                           (0x16)
#define BMP388_FIFO_WTM_0_REG                           (0x15)
#define BMP388_FIFO_DATA_REG                            (0x14)
#define BMP388_FIFO_LENGTH_1_REG                        (0x13)
#define BMP388_FIFO_LENGTH_0_REG                        (0x12)
#define BMP388_INT_STATUS_REG                           (0x11)
#define BMP388_EVENT_REG                                (0x10)
#define BMP388_SENSORTIME_3_REG                         (0x0F) // BME780 only
#define BMP388_SENSORTIME_2_REG                         (0x0E)
#define BMP388_SENSORTIME_1_REG                         (0x0D)
#define BMP388_SENSORTIME_0_REG                         (0x0C)
#define BMP388_RESERVED_0x0B_REG                        (0x0B)
#define BMP388_RESERVED_0x0A_REG                        (0x0A)

// see friendly register names below
#define BMP388_DATA_5_REG                               (0x09)
#define BMP388_DATA_4_REG                               (0x08)
#define BMP388_DATA_3_REG                               (0x07)
#define BMP388_DATA_2_REG                               (0x06)
#define BMP388_DATA_1_REG                               (0x05)
#define BMP388_DATA_0_REG                               (0x04)

#define BMP388_STATUS_REG                               (0x03)
#define BMP388_ERR_REG                                  (0x02)
#define BMP388_RESERVED_0x01_REG                        (0x01)
#define BMP388_CHIP_ID_REG                              (0x00)

// friendly register names, from datasheet 4.3.4
#define BMP388_PRESS_MSB_23_16_REG                      BMP388_DATA_2_REG
#define BMP388_PRESS_LSB_15_8_REG                       BMP388_DATA_1_REG
#define BMP388_PRESS_XLSB_7_0_REG                       BMP388_DATA_0_REG

// friendly register names, from datasheet 4.3.5
#define BMP388_TEMP_MSB_23_16_REG                       BMP388_DATA_5_REG
#define BMP388_TEMP_LSB_15_8_REG                        BMP388_DATA_4_REG
#define BMP388_TEMP_XLSB_7_0_REG                        BMP388_DATA_3_REG

#define BMP388_DATA_FRAME_SIZE                          ((BMP388_DATA_5_REG - BMP388_DATA_0_REG) + 1) // +1 for inclusive

// from Datasheet 3.3
#define BMP388_MODE_SLEEP                               (0x00)
#define BMP388_MODE_FORCED                              (0x01)
#define BMP388_MODE_NORMAL                              (0x03)

#define BMP388_CALIRATION_LOWER_REG                     (0x30) // See datasheet 4.3.19, "calibration data"
#define BMP388_TRIMMING_NVM_PAR_T1_LSB_REG              (0x31) // See datasheet 3.11.1 "Memory map trimming coefficients"
#define BMP388_TRIMMING_NVM_PAR_P11_REG                 (0x45) // See datasheet 3.11.1 "Memory map trimming coefficients"
#define BMP388_CALIRATION_UPPER_REG                     (0x57)

#define BMP388_TRIMMING_DATA_LENGTH                     ((BMP388_TRIMMING_NVM_PAR_P11_REG - BMP388_TRIMMING_NVM_PAR_T1_LSB_REG) + 1) // +1 for inclusive

#define BMP388_OVERSAMP_1X               (0x00)
#define BMP388_OVERSAMP_2X               (0x01)
#define BMP388_OVERSAMP_4X               (0x02)
#define BMP388_OVERSAMP_8X               (0x03)
#define BMP388_OVERSAMP_16X              (0x04)
#define BMP388_OVERSAMP_32X              (0x05)

// INT_CTRL register
#define BMP388_INT_OD_BIT                   0
#define BMP388_INT_LEVEL_BIT                1
#define BMP388_INT_LATCH_BIT                2
#define BMP388_INT_FWTM_EN_BIT              3
#define BMP388_INT_FFULL_EN_BIT             4
#define BMP388_INT_RESERVED_5_BIT           5
#define BMP388_INT_DRDY_EN_BIT              6
#define BMP388_INT_RESERVED_7_BIT           7


// ODR register
#define BMP388_TIME_STANDBY_5MS        0x00
#define BMP388_TIME_STANDBY_10MS       0x01
#define BMP388_TIME_STANDBY_20MS       0x02
#define BMP388_TIME_STANDBY_40MS       0x03
#define BMP388_TIME_STANDBY_80MS       0x04
#define BMP388_TIME_STANDBY_160MS      0x05
#define BMP388_TIME_STANDBY_320MS      0x06
#define BMP388_TIME_STANDBY_640MS      0x07
#define BMP388_TIME_STANDBY_1280MS     0x08
#define BMP388_TIME_STANDBY_2560MS     0x09
#define BMP388_TIME_STANDBY_5120MS     0x0A
#define BMP388_TIME_STANDBY_10240MS    0x0B
#define BMP388_TIME_STANDBY_20480MS    0x0C
#define BMP388_TIME_STANDBY_40960MS    0x0D
#define BMP388_TIME_STANDBY_81920MS    0x0E
#define BMP388_TIME_STANDBY_163840MS   0x0F
#define BMP388_TIME_STANDBY_327680MS   0x10
#define BMP388_TIME_STANDBY_655360MS   0x11

#define BMP388_FILTER_COEFF_OFF              (0x00)
#define BMP388_FILTER_COEFF_1                (0x01)
#define BMP388_FILTER_COEFF_3                (0x02)
#define BMP388_FILTER_COEFF_7                (0x03)
#define BMP388_FILTER_COEFF_15               (0x04)
#define BMP388_FILTER_COEFF_31               (0x05)
#define BMP388_FILTER_COEFF_63               (0x06)
#define BMP388_FILTER_COEFF_127              (0x07)

#define BMP388_RESET_CODE 0xB6

// Indoor navigation
// Normal : Mode
// x16    : osrs_pos
// x2     : rs_t
// 4      : IIR filtercoeff.(see4.3.18)
// 560    : IDD[μA](see 3.8)
// 25     : ODR [Hz](see 3.4.1)
// 5      : RMS Noise [cm](see 3.4.4)


bool bmp388GetUP(baroDev_t *baro)
{
  uint8_t data[BMP388_DATA_FRAME_SIZE]; //, status;

  //if (busReadBuf(baro->busDev, BMP388_INT_STATUS_REG, &status, 1))
  //{
  //  if (0 == (status &  (1 << 3)))
  //    busPrint(baro->busDev, PSTR("Data is NOT ready"));
  //}

  bool ack = busReadBuf(baro->busDev, BMP388_DATA_0_REG, data, BMP388_DATA_FRAME_SIZE);
  if (ack)
  {
    baro->chip.bmp388.ut = (int32_t)data[5] << 16 | (int32_t)data[4] << 8 | (int32_t)data[3];  // Copy the temperature and pressure data into the adc variables
    baro->chip.bmp388.up = (int32_t)data[2] << 16 | (int32_t)data[1] << 8 | (int32_t)data[0];
  }
  else
    handleSensorFailure();


  return ack;
}

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of "5123" equals 51.23 DegC
int64_t bmp388CompensateTemperature(baroDev_t *baro)
{
  float partial_data1 = (float)baro->chip.bmp388.ut - baro->chip.bmp388.cal.param_T1;
  float partial_data2 = partial_data1 * baro->chip.bmp388.cal.param_T2;
  return partial_data2 + partial_data1 * partial_data1 * baro->chip.bmp388.cal.param_T3;
}

float bmp388CompensatePressure(baroDev_t *baro, float t_lin)
{
  float uncomp_pressure = (float)baro->chip.bmp388.up;
  float partial_data1 = baro->chip.bmp388.cal.param_P6 * t_lin;
  float partial_data2 = baro->chip.bmp388.cal.param_P7 * t_lin * t_lin;
  float partial_data3 = baro->chip.bmp388.cal.param_P8 * t_lin * t_lin * t_lin;
  float partial_out1 = baro->chip.bmp388.cal.param_P5 + partial_data1 + partial_data2 + partial_data3;
  partial_data1 = baro->chip.bmp388.cal.param_P2 * t_lin;
  partial_data2 = baro->chip.bmp388.cal.param_P3 * t_lin * t_lin;
  partial_data3 = baro->chip.bmp388.cal.param_P4 * t_lin * t_lin * t_lin;
  float partial_out2 = uncomp_pressure * (baro->chip.bmp388.cal.param_P1 +
                                          partial_data1 + partial_data2 + partial_data3);
  partial_data1 = uncomp_pressure * uncomp_pressure;
  partial_data2 = baro->chip.bmp388.cal.param_P9 + baro->chip.bmp388.cal.param_P10 * t_lin;
  partial_data3 = partial_data1 * partial_data2;
  float partial_data4 = partial_data3 + uncomp_pressure * uncomp_pressure * uncomp_pressure * baro->chip.bmp388.cal.param_P11;
  return partial_out1 + partial_out2 + partial_data4;
}



bool bmp388Calculate(baroDev_t * baro, float * pressure, float * temperature)
{
  float t;

  if (!bmp388GetUP(baro))
    return false;

  t = bmp388CompensateTemperature(baro);

  if (pressure) {
    *pressure = bmp388CompensatePressure(baro, t);
  }

  if (temperature) {
    *temperature = t / 100.0;
  }

  return true;
}

bool bmp388Reset(busDevice_t * busDev)
{
  uint8_t event;
  busWrite(busDev, BMP388_CMD_REG, BMP388_RESET_CODE);
  delay(10);
  if (busRead(busDev, BMP388_EVENT_REG, &event))
    return event ? true : false;
  return false;
}



// see Datasheet 3.11.1 Memory Map Trimming Coefficients
typedef struct bmp388_raw_param_s {
  uint16_t param_T1;
  uint16_t param_T2;
  int8_t param_T3;
  int16_t param_P1;
  int16_t param_P2;
  int8_t param_P3;
  int8_t param_P4;
  uint16_t param_P5;
  uint16_t param_P6;
  int8_t param_P7;
  int8_t param_P8;
  int16_t param_P9;
  int8_t param_P10;
  int8_t param_P11;
} __attribute__((packed)) bmp388_raw_param_t;


bool bmp388Detect(baroDev_t *baro, busDevice_t *busDev)
{
  for (int retry = 0; retry < DETECTION_MAX_RETRY_COUNT; retry++) {
    uint8_t chipId = 0;

    bmp388Reset(busDev);

    bool ack = busRead(busDev, BMP388_CHIP_ID_REG, &chipId);
    if (ack && chipId == BMP388_DEFAULT_CHIP_ID) {
      bmp388_raw_param_t params;

      busPrint(busDev, PSTR("BMP388 Detected"));

      baro->busDev = busDev;

      // read calibration

      busReadBuf(baro->busDev, BMP388_TRIMMING_NVM_PAR_T1_LSB_REG, (unsigned char *)&params, sizeof(params));

      baro->chip.bmp388.cal.param_T1 = (float)params.param_T1 / powf(2.0f, -8.0f); // Calculate the floating point trim parameters
      baro->chip.bmp388.cal.param_T2 = (float)params.param_T2 / powf(2.0f, 30.0f);
      baro->chip.bmp388.cal.param_T3 = (float)params.param_T3 / powf(2.0f, 48.0f);
      baro->chip.bmp388.cal.param_P1 = ((float)params.param_P1 - powf(2.0f, 14.0f)) / powf(2.0f, 20.0f);
      baro->chip.bmp388.cal.param_P2 = ((float)params.param_P2 - powf(2.0f, 14.0f)) / powf(2.0f, 29.0f);
      baro->chip.bmp388.cal.param_P3 = (float)params.param_P3 / powf(2.0f, 32.0f);
      baro->chip.bmp388.cal.param_P4 = (float)params.param_P4 / powf(2.0f, 37.0f);
      baro->chip.bmp388.cal.param_P5 = (float)params.param_P5 / powf(2.0f, -3.0f);
      baro->chip.bmp388.cal.param_P6 = (float)params.param_P6 / powf(2.0f, 6.0f);
      baro->chip.bmp388.cal.param_P7 = (float)params.param_P7 / powf(2.0f, 8.0f);
      baro->chip.bmp388.cal.param_P8 = (float)params.param_P8 / powf(2.0f, 15.0f);
      baro->chip.bmp388.cal.param_P9 = (float)params.param_P9 / powf(2.0f, 48.0f);
      baro->chip.bmp388.cal.param_P10 = (float)params.param_P10 / powf(2.0f, 48.0f);
      baro->chip.bmp388.cal.param_P11 = (float)params.param_P11 / powf(2.0f, 65.0f);

      //set IIR Filter
      busWrite(baro->busDev, BMP388_CONFIG_REG, (BMP388_FILTER_COEFF_OFF) << 1);


      // Set Oversampling rate
      /* PRESSURE<<3 | TEMP */
      busWrite(baro->busDev, BMP388_OSR_REG,
               (BMP388_OVERSAMP_8X) | (BMP388_OVERSAMP_1X << 3)
              );


      // Set mode 0b00110011, normal, pressure and temperature
      busWrite(busDev, BMP388_PWR_CTRL_REG, 0x03);

      // Set Data Rate
      busWrite(baro->busDev, BMP388_ODR_REG, BMP388_TIME_STANDBY_20MS);

      // Set mode 0b00110011, normal, pressure and temperature
      busWrite(busDev, BMP388_PWR_CTRL_REG, 0x33);

      baro->sensorType = SENSOR_BMP388;
      baro->calculate = bmp388Calculate;
      return true;
    }

    delay(100);
  }

  return false;
}







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

// Do your venturi mapping here
#define VENTURI0 0
#define VENTURI1 1
#define VENTURI2 2
#define VENTURI3 3

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


busDevice_t *detectIndividualSensor(baroDev_t *baro, TwoWire *wire, uint8_t address, uint8_t channel = 0, busDevice_t *muxDevice = NULL, int8_t enablePin = -1)
{
  busDevice_t *device = busDeviceInitI2C(wire, address, channel, muxDevice, enablePin);

  if (!device)
    return NULL;

  busPrint(device, PSTR("Discovering sensor type"));
  if (!bmp280Detect(baro, device))
  {
    if (!bmp388Detect(baro, device))
    {
      if (!spl06Detect(baro, device))
      {
        busPrint(device, PSTR("sensor identification failed"));
        busDeviceFree(device);
        device = NULL;
      }
    }
  }
  device->hwType = HWTYPE_SENSOR;
  return device;
}

busDevice_t *detectEEPROM(TwoWire * wire, uint8_t address, uint8_t muxChannel = 0, busDevice_t *muxDevice = NULL, int8_t enablePin = -1)
{
  busDevice_t *thisDevice = busDeviceInitI2C(wire, address, muxChannel, muxDevice, enablePin);
  if (!busDeviceDetect(thisDevice))
  {
    free(thisDevice);
    return NULL;
  }
  thisDevice->hwType = HWTYPE_EEPROM;
  return thisDevice;
}


bool detectMuxedSensors(TwoWire *wire)
{
  int8_t enablePin = -1;

  // MUX has a switching chip that can have different adresses (including ones on our devices)
  // Could be 2 paths with 2 sensors each or 4 paths with 1 on each.

  busDevice_t *muxDevice = busDeviceInitI2C(wire, 0x70);
  if (!busDeviceDetect(muxDevice))
  {
    //debug(PSTR("Failed to find MUX on I2C, detecting enable pin"));
    enablePin = ENABLE_PIN_BUS_A;
    busDeviceFree(muxDevice);
    muxDevice = busDeviceInitI2C(wire, 0x70, 0, NULL, enablePin);
    if (!busDeviceDetect(muxDevice))
    {
      //debug(PSTR("Failed to detect MUX Based VISP"));
      busDeviceFree(muxDevice);
      return false;
    }
  }

  // Assign the device it's correct type
  muxDevice->hwType = HWTYPE_MUX;

  eeprom = detectEEPROM(wire, 0x54, 1, muxDevice, enablePin);


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
bool detectXLateSensors(TwoWire * wire)
{
  int enablePin = -1;
  // XLATE version has chips at 0x74, 0x75, 0x76, and 0x77
  // So, if we find 0x74... We are good to go

  busDevice_t *seventyFour = busDeviceInitI2C(wire, 0x74);
  if (!busDeviceDetect(seventyFour))
  {
    busDeviceFree(seventyFour);
    seventyFour = busDeviceInitI2C(wire, 0x74, 0, NULL, ENABLE_PIN_BUS_A);
    if (!busDeviceDetect(seventyFour))
    {
      //debug(PSTR("Failed to detect XLate Based VISP"));
      busDeviceFree(seventyFour);
      return false;
    }
    enablePin = ENABLE_PIN_BUS_A;
  }

  eeprom = detectEEPROM(wire, 0x54, 0, NULL, enablePin);

  // Detect U5, U6
  detectIndividualSensor(&sensors[SENSOR_U5], wire, 0x76, 0, NULL, enablePin);
  detectIndividualSensor(&sensors[SENSOR_U6], wire, 0x77, 0, NULL, enablePin);

  // Detect U7, U8
  detectIndividualSensor(&sensors[SENSOR_U7], wire, 0x74, 0, NULL, enablePin);
  detectIndividualSensor(&sensors[SENSOR_U8], wire, 0x75, 0, NULL, enablePin);

  detectedVispType = VISP_BUS_TYPE_XLATE;

  return true;
}

bool detectDualI2CSensors(TwoWire * wireA, TwoWire * wireB)
{
  int8_t enablePinA = -1;
  int8_t enablePinB = -1;


  eeprom = detectEEPROM(wireA, 0x54);
  if (!eeprom)
  {
    enablePinA = 4;
    enablePinB = 5;
    eeprom = detectEEPROM(wireA, 0x54, 0, NULL, enablePinA);
    if (!eeprom)
    {
      eeprom = detectEEPROM(wireA, 0x54, 0, NULL, enablePinB);
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
    debug(PSTR("Enable pins detected!"));
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

// FUTURE: read EEPROM and determine what type of VISP it is.
void detectSensors(TwoWire * i2cBusA, TwoWire * i2cBusB)
{
  bool formatVisp = false;
  memset(&sensors, 0, sizeof(sensors));

  debug(PSTR("Detecting sensors"));

  if (!detectMuxedSensors(i2cBusA))
    if (!detectXLateSensors(i2cBusA))
      if (!detectDualI2CSensors(i2cBusA, i2cBusB))
        return;

  // Make sure they are all there
  if (sensors[0].busDev == NULL) {
    warning(PSTR("Sensor 0 missing"));
    return;
  }
  if (sensors[1].busDev == NULL) {
    warning(PSTR("Sensor 1 missing"));
    return;
  }
  if (sensors[2].busDev == NULL) {
    warning(PSTR("Sensor 2 missing"));
    return;
  }
  if (sensors[3].busDev == NULL) {
    warning(PSTR("Sensor 3 missing"));
    return;
  }

  switch (detectedVispType) {
    case VISP_BUS_TYPE_I2C:   debug(PSTR("DUAL I2C VISP detected"));    break;
    case VISP_BUS_TYPE_XLATE: debug(PSTR("XLate Based VISP Detected")); break;
    case VISP_BUS_TYPE_MUX:   debug(PSTR("MUX Based VISP Detected"));   break;
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
      formatVisp = true;
      warning(PSTR("ERROR eeprom not formatted"));
    }
  }
  else
  {
    warning(PSTR("ERROR eeprom not available"));
    // Actually, just provide soem sane numbers for the system
    formatVisp = true;
  }

  if (formatVisp)
    formatVispEEPROM(detectedVispType, VISP_BODYTYPE_VENTURI);


  // We store the calibration data in the VISP eeprom variable to conserve ram space
  // We only have 2KB on an Arduino NANO/UNO
  clearCalibrationData();

  sensorsFound = true;

  // Just put it out there, what type we are for the status system to figure out
  info(PSTR("Sensors detected"));
  sendCurrentSystemHealth();
}




bool timeToReadSensors = false;


/*** Timer callback subsystem ***/

typedef void (*tCBK)() ;
typedef struct t  {
  unsigned long tStart;
  unsigned long tTimeout;
  tCBK cbk;
} t_t;


void tCheck (struct t * t ) {
  if (millis() > t->tStart + t->tTimeout)
  {
    t->cbk();
    t->tStart = millis();
  }
}

// Periodically pulse a pin
void timeToPulseWatchdog()
{
  if (sensorsFound)
  {
    digitalWrite(MISSING_PULSE_PIN, HIGH);
    delayMicroseconds(1);
    digitalWrite(MISSING_PULSE_PIN, LOW);
  }
}

void timeToReadVISP()
{
  timeToReadSensors = true;
}

void timeToCheckSensors()
{
  // If debug is on, and a VISP is NOT connected, we flood the system with sensor scans.
  // Do it every half second (or longer)
  if (!sensorsFound)
    detectSensors(i2cBus1, i2cBus2);
}

// Timer Driven Tasks and their Schedules.
// These are checked and executed in order.
// If something takes priority over another task, put it at the top of the list
t tasks[] = {
  {0, 20, timeToReadVISP},
  {0, 100, timeToPulseWatchdog},
  {0, 500, timeToCheckSensors},
  {0, 0, NULL} // End of list
};

/*** End of timer callback subsystem ***/
void saveParameters()
{
  visp_eeprom.checksum = 0;
  // TODO: compute checksum
  writeEEPROM(eeprom, (unsigned short)0, (unsigned char *)&visp_eeprom, sizeof(visp_eeprom));
}


void formatVispEEPROM(uint8_t busType, uint8_t bodyType)
{
  memset(&visp_eeprom, 0, sizeof(visp_eeprom));

  visp_eeprom.VISP[0] = 'V';
  visp_eeprom.VISP[1] = 'I';
  visp_eeprom.VISP[2] = 'S';
  visp_eeprom.VISP[3] = 'p';
  visp_eeprom.busType = busType;
  visp_eeprom.bodyType = bodyType;
  visp_eeprom.bodyVersion = '0';

  // TODO: scan sensor mappings and populate the eeprom with what we have detected
  // TODO: checksum;
  // volume;
  // rate;
  // breath_interval;
  // breath_threshold;
  // motor_speed;    // For demonstration purposes, run motor at a fixed speed...
  // mode;

  visp_eeprom.debug = DEBUG_DISABLED;

  // Make *sure* that the calibration data is formatted properly
  clearCalibrationData();

  writeEEPROM(eeprom, (unsigned short)0, (unsigned char *)&visp_eeprom, sizeof(visp_eeprom));
}


void clearCalibrationData()
{
  runState = RUNSTATE_CALIBRATE;
  calibrationSampleCounter = 0;
  memset(&visp_eeprom.calibrationOffsets, 0, sizeof(visp_eeprom.calibrationOffsets));
}

#ifdef ARDUINO_TEENSY40
IntervalTimer myIntervalTimer;

void doStepperStuff()
{
  mystepper.runSpeed();
}
void setupTimer1()
{
  myIntervalTimer.begin(doStepperStuff, 1000); // microseconds
}
#else
ISR(Timer1_COMPA_vect) // timer compare interrupt service routine
{
  mystepper.runSpeed();
}

void setupTimer1() // WARNING: NOTE: Conflicts with Servo library
{
  noInterrupts(); //Disable global interrupts
  TCCR1A = 0; //Register set to 0
  TCCR1B = 0; //Register set to 0
  TCNT1 = 0;

  OCR1A = 15999; //Counter for 1KHz interrupt 16*10^6/1000-1 no prescaler
  TCCR1B |= (1 << WGM12); //CTC mode
  TCCR1B |= (1 << CS10); //No prescaler
  TIMSK1 |= (1 << OCIE1A); //Compare interrupt mode

  interrupts(); //Enable global interrupts
}
#endif

void setup() {
  // Address select lines for Dual I2C switching using NPN Transistors
  pinMode(ENABLE_PIN_BUS_A, OUTPUT);
  digitalWrite(ENABLE_PIN_BUS_A, LOW);
  pinMode(ENABLE_PIN_BUS_B, OUTPUT);
  digitalWrite(ENABLE_PIN_BUS_B, LOW);
  pinMode(MISSING_PULSE_PIN, OUTPUT);
  digitalWrite(MISSING_PULSE_PIN, LOW);
  pinMode(M_PWM_1, OUTPUT);
  pinMode(M_PWM_2, OUTPUT);
  pinMode(M_DIR_1, OUTPUT);
  pinMode(M_DIR_2, OUTPUT);

  memset(&sensors, 0, sizeof(sensors));

  mystepper.setAcceleration(3000);
  mystepper.setMaxSpeed(1000);
  //  setupTimer1();

  hwSerial.begin(SERIAL_BAUD);
  respond('I', PSTR("VISP Core,%d,%d,%c"), VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);

  i2cBus1->begin();
  i2cBus1->setClock(400000); // Typical


  // Some reset conditions do not reset our globals.
  sensorsFound = false;
  visp_eeprom.debug = DEBUG_DISABLED;
  clearCalibrationData();
}

void dataSend(unsigned long sampleTime, float pressure, float volumeSmoothed, float tidalVolume, float *P)
{
  // Take some time to write to the serial port
  hwSerial.print('d');
  hwSerial.print(',');
  hwSerial.print(sampleTime);
  hwSerial.print(',');
  hwSerial.print(pressure, 4);
  hwSerial.print(',');
  hwSerial.print(volumeSmoothed, 4);
  hwSerial.print(',');
  if (visp_eeprom.debug == DEBUG_DISABLED)
    hwSerial.println(tidalVolume, 4);
  else
  {
    hwSerial.print(tidalVolume, 4);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U5], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U6], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U7], 1);
    hwSerial.print(',');
    hwSerial.println(P[SENSOR_U8], 1);
  }
}



void doCalibration(float *P)
{
  if (calibrationSampleCounter == 0)
  {
    respond('C', PSTR("0,Starting Calibration"));
    clearCalibrationData();
  }
  visp_eeprom.calibrationOffsets[0] += P[0];
  visp_eeprom.calibrationOffsets[1] += P[1];
  visp_eeprom.calibrationOffsets[2] += P[2];
  visp_eeprom.calibrationOffsets[3] += P[3];
  calibrationSampleCounter++;
  if (calibrationSampleCounter == 99) {
    float average = (visp_eeprom.calibrationOffsets[0] + visp_eeprom.calibrationOffsets[1] + visp_eeprom.calibrationOffsets[2] + visp_eeprom.calibrationOffsets[3]) / 400.0;
    for (int x = 0; x < 4; x++)
    {
      visp_eeprom.calibrationOffsets[x] = average - visp_eeprom.calibrationOffsets[x] / 100.0;
    }
    calibrationSampleCounter = 0;
    runState = RUNSTATE_RUN;
    respond('C', PSTR("2,Calibration Finished"));
  }
}


// Use these definitions to map sensors to their usage
#define PATIENT_PRESSURE SENSOR_U5
#define AMBIANT_PRESSURE SENSOR_U6
#define PITOT1           SENSOR_U7
#define PITOT2           SENSOR_U8

void loopPitotVersion(float *P, float *T)
{
  float  airflow, volume, pitot_diff, ambientPressure, pitot1, pitot2, patientPressure, pressure;

  switch (runState)
  {
    case RUNSTATE_CALIBRATE:
      doCalibration(P);
      break;

    case RUNSTATE_RUN:
      const float paTocmH2O = 0.0101972;
      //    static float paTocmH2O = 0.00501972;
      ambientPressure = P[AMBIANT_PRESSURE] - visp_eeprom.calibrationOffsets[AMBIANT_PRESSURE];
      pitot1 = P[PITOT1] - visp_eeprom.calibrationOffsets[PITOT1];
      pitot2 = P[PITOT2] - visp_eeprom.calibrationOffsets[PITOT2];
      patientPressure = P[PATIENT_PRESSURE] - visp_eeprom.calibrationOffsets[PATIENT_PRESSURE];

      pitot_diff = (pitot1 - pitot2) / 100.0; // pascals to hPa

      airflow = ((0.05 * pitot_diff * pitot_diff) - (0.0008 * pitot_diff)); // m/s
      //airflow=sqrt(2.0*pitot_diff/2.875);
      if (pitot_diff < 0) {
        airflow = -airflow;
      }
      //airflow = -(-0.0008+sqrt(0.0008*0.0008-4*0.05*0.0084+4*0.05*pitot_diff))/2*0.05;

      volume = airflow * 0.25 * 60.0; // volume for our 18mm orfice, and 60s/min
      pressure = (patientPressure - ambientPressure) * paTocmH2O; // average of all sensors in the tube for pressure reading

      // Take some time to write to the serial port
      dataSend(millis(), pressure, volume, 0.0, P);
      break;
  }
}

//U7 is input tube, U8 is output tube, U5 is venturi, U6 is ambient
#define VENTURI_SENSOR  SENSOR_U5
#define VENTURI_AMBIANT SENSOR_U6
#define VENTURI_INPUT   SENSOR_U7
#define VENTURI_OUTPUT  SENSOR_U8
void loopVenturiVersion(float * P, float * T)
{
  static float volumeSmoothed = 0.0; // It only needs to be in this function, but needs to be persistant, so make it static
  static float tidalVolume = 0.0;
  static unsigned long lastSampleTime = 0;
  float volume, inletPressure, outletPressure, throatPressure, ambientPressure, patientPressure, pressure;
  unsigned long sampleTime = millis();

  switch (runState)
  {
    case RUNSTATE_CALIBRATE:
      doCalibration(P);
      break;

    case RUNSTATE_RUN:
      digitalWrite(M_DIR_1, 0);
      digitalWrite(M_DIR_2, 1);
      analogWrite(M_PWM_1, 200);
      analogWrite(M_PWM_2, 200);
      P[0] += visp_eeprom.calibrationOffsets[0];
      P[1] += visp_eeprom.calibrationOffsets[1];
      P[2] += visp_eeprom.calibrationOffsets[2];
      P[3] += visp_eeprom.calibrationOffsets[3];
      const float paTocmH2O = 0.0101972;
      // venturi calculations
      const float a1 = 232.35219306; // area of pipe
      const float a2 = 56.745017403; // area of restriction

      const float a_diff = (a1 * a2) / sqrt((a1 * a1) - (a2 * a2)); // area difference

      //    static float paTocmH2O = 0.00501972;
      ambientPressure = P[VENTURI_AMBIANT];
      inletPressure = P[VENTURI_INPUT];
      outletPressure = P[VENTURI_OUTPUT];
      patientPressure = P[VENTURI_SENSOR];   // This is not used?
      throatPressure = P[VENTURI_SENSOR];

      //float h= ( inletPressure-throatPressure )/(9.81*998); //pressure head difference in m
      //airflow = a_diff * sqrt(2.0 * (inletPressure - throatPressure)) / 998.0) * 600000.0; // airflow in cubic m/s *60000 to get L/m
      // Why multiply by 2 then devide by a number, why not just divide by half the number?
      //airflow = a_diff * sqrt((inletPressure - throatPressure) / (449.0*1.2)) * 600000.0; // airflow in cubic m/s *60000 to get L/m


      if (inletPressure > outletPressure && inletPressure > throatPressure)
      {
        volume = a_diff * sqrt((inletPressure - throatPressure) / (449.0 * 1.2)) * 0.6; // instantaneous volume
      }
      else if (outletPressure > inletPressure && outletPressure > throatPressure)
      {
        volume = -a_diff * sqrt((outletPressure - throatPressure) / (449.0 * 1.2)) * 0.6;
      }
      else
      {
        volume = 0.0;
      }
      if (isnan(volume) || abs(volume) < 1.0 )
      {
        volume = 0.0;
      }

      const float alpha = 0.15; // smoothing factor for exponential filter
      volumeSmoothed = volume * alpha + volumeSmoothed * (1.0 - alpha);

      if (lastSampleTime)
      {
        tidalVolume = tidalVolume + volumeSmoothed * (sampleTime - lastSampleTime) / 60 - 0.05; // tidal volume is the volume delivered to the patient at this time.  So it is cumulative.
      }
      if (tidalVolume < 0.0)
      {
        tidalVolume = 0.0;
      }
      lastSampleTime = sampleTime;
      pressure = ((inletPressure + outletPressure) / 2.0 - ambientPressure) * paTocmH2O;

      dataSend(sampleTime, pressure, volumeSmoothed, tidalVolume, P);
      break;
  }
}

typedef enum parser_state_e {
  PARSE_COMMAND,
  PARSE_IGNORE_TILL_LF,
  PARSE_SINGLE_ARG,
  PARSE_FIRST_ARG,
  PARSE_SECOND_ARG,
  PARSE_EEPROM_ARGS
} parser_state_t;



void sendEEPROMdata(uint16_t address, uint16_t len)
{
  uint8_t *cbuf = (uint8_t *)&visp_eeprom;
  hwSerial.print(PSTR("E,"));
  hwSerial.print(millis());
  hwSerial.print(PSTR(", 0x"));
  hwSerial.print(address, HEX);
  for (int x = 0; x < len; x++)
  {
    hwSerial.print(PSTR(",0x"));
    hwSerial.print(cbuf[address + x], HEX);
  }
  hwSerial.println();
}

void updateEEPROMdata(uint16_t address, uint8_t data)
{
  uint8_t *cbuf = (uint8_t *)&visp_eeprom;
  cbuf[address] = data;
}

#define RESPOND_LIMITS             1
#define RESPOND_DEBUG              2
#define RESPOND_MODE               4
#define RESPOND_VOLUME             8
#define RESPOND_RATE              16
#define RESPOND_BREATH_INTERVAL   32
#define RESPOND_BREATH_THRESHOLD  64
#define RESPOND_MOTOR_SPEED      128
#define RESPOND_BODYTYPE         256
#define RESPOND_CALIB0           512
#define RESPOND_CALIB1          1024
#define RESPOND_CALIB2          2048
#define RESPOND_CALIB3          4096
#define RESPOND_SENSOR0         8192
#define RESPOND_SENSOR1        16384
#define RESPOND_SENSOR2        32768
#define RESPOND_SENSOR3        65536


struct dictionary_s {
  int theAssociatedValue; // -1 is END OF LIST marker
  const char * PUTINFLASH theWord;
  const char * PUTINFLASH theDescription;
} ;

// Put the strings in flash instead of SRAM
const char strPCCMV [] PUTINFLASH = "PC-CMV";
const char strPCCMVdesc [] PUTINFLASH = "Pressure Controlled";
const char strVCCMV [] PUTINFLASH = "VC-CMV";
const char strVCCMVdesc [] PUTINFLASH = "Volume Controlled";
const char strDisable [] PUTINFLASH = "Disable";
const char strEnable [] PUTINFLASH = "Enable";
const char strDisabled [] PUTINFLASH = "Disabled";
const char strEnabled [] PUTINFLASH = "Enabled";
const char strUnknown [] PUTINFLASH = "Unknown";
const char strPitot [] PUTINFLASH = "Pitot";
const char strPitotDesc [] PUTINFLASH = "Pitot body style";
const char strVenturi [] PUTINFLASH = "Venturi";
const char strVenturiDesc [] PUTINFLASH = "Venturi body style";
const char strSensor0 [] PUTINFLASH = "sensor0";
const char strSensor1 [] PUTINFLASH = "sensor1";
const char strSensor2 [] PUTINFLASH = "sensor2";
const char strSensor3 [] PUTINFLASH = "sensor3";
const char strBMP388 [] PUTINFLASH = "BMP388";
const char strBMP280 [] PUTINFLASH = "BMP280";
const char strSPL06 [] PUTINFLASH = "SPL06";


const struct dictionary_s sensorDict[] PUTINFLASH = {
  {SENSOR_UNKNOWN, strUnknown, strUnknown},
  {SENSOR_BMP388, strBMP388, strBMP388},
  {SENSOR_BMP280, strBMP280, strBMP280},
  {SENSOR_SPL06, strSPL06, strSPL06},
  { -1, NULL, NULL}
};

const struct dictionary_s modeDict[] PUTINFLASH = {
  {MODE_UNKNOWN, strUnknown, strUnknown},
  {MODE_PCCMV,   strPCCMV,   strPCCMVdesc},
  {MODE_VCCMV,   strVCCMV,   strVCCMVdesc},
  { -1, NULL, NULL}
};

const struct dictionary_s enableDict[] PUTINFLASH = {
  {DEBUG_DISABLED, strDisable, strDisabled},
  {DEBUG_ENABLED, strEnable,  strEnabled},
  { -1, NULL, NULL}
};

const struct dictionary_s bodyDict[] PUTINFLASH = {
  {VISP_BODYTYPE_UNKNOWN, strUnknown, strUnknown},
  {VISP_BODYTYPE_PITOT,   strPitot, strPitotDesc},
  {VISP_BODYTYPE_VENTURI, strVenturi, strVenturiDesc},
  { -1, NULL, NULL}
};




typedef bool (*verifyCallback)(struct settingsEntry_s *, const char *);
typedef void (*respondCallback)(struct settingsEntry_s *);

struct settingsEntry_s {
  const uint32_t PUTINFLASH bitmask;
  const char * const PUTINFLASH theName;
  const int16_t PUTINFLASH theMin;
  const int16_t PUTINFLASH theMax;
  const PUTINFLASH struct dictionary_s  *personalDictionary;
  const verifyCallback PUTINFLASH verifyIt;
  const respondCallback PUTINFLASH respondIt;
  const respondCallback PUTINFLASH actionIt;
  void * const PUTINFLASH data;
};

const char strMode [] PUTINFLASH = "mode";
const char strDebug [] PUTINFLASH = "debug";
const char strBodyType [] PUTINFLASH = "bodytype";
const char strVolume [] PUTINFLASH = "volume";
const char strRate [] PUTINFLASH = "rate";
const char strBreathInterval [] PUTINFLASH = "breath_interval";
const char strBreathThreshold [] PUTINFLASH = "breath_threshold";
const char strMotorSpeed [] PUTINFLASH = "motor_speed";
const char strCalib0 [] PUTINFLASH = "calib0";
const char strCalib1 [] PUTINFLASH = "calib1";
const char strCalib2 [] PUTINFLASH = "calib2";
const char strCalib3 [] PUTINFLASH = "calib3";

bool noSet(struct settingsEntry_s *entry, const char *arg);
bool verifyDictWordToInt8(struct settingsEntry_s *entry, const char *arg);
bool verifyDictWordToInt16(struct settingsEntry_s *entry, const char *arg);
bool verifyLimitsToInt16(struct settingsEntry_s *entry);
void respondFloat(struct settingsEntry_s *entry);
void respondInt8(struct settingsEntry_s *entry);
void respondInt16(struct settingsEntry_s *entry);
void respondInt8ToDict(struct settingsEntry_s *entry);

void motorAction(struct settingsEntry_s *entry)
{
  info(PSTR("Setting motor vales to %d"), visp_eeprom.motor_speed);
  digitalWrite(M_DIR_1, 0);
  if (visp_eeprom.motor_speed == 0)
    digitalWrite(M_DIR_2, 0);
  else
    digitalWrite(M_DIR_2, 1);
  analogWrite(M_PWM_1, visp_eeprom.motor_speed);
  analogWrite(M_PWM_2, visp_eeprom.motor_speed);
  mystepper.setSpeed(visp_eeprom.motor_speed);
}

//const char *const string_table[] PUTINFLASH = {string_0, string_1, string_2, string_3, string_4, string_5};
const struct settingsEntry_s settings[] PUTINFLASH = {
  {RESPOND_MODE,             strMode, 0, 0, modeDict, verifyDictWordToInt8, respondInt8ToDict, NULL, &visp_eeprom.mode},
  {RESPOND_DEBUG,            strDebug, 0, 0, enableDict, verifyDictWordToInt8, respondInt8ToDict, NULL, &visp_eeprom.debug},
  {RESPOND_BODYTYPE,         strBodyType, 0, 0, bodyDict, verifyDictWordToInt8, respondInt8ToDict, NULL, &visp_eeprom.bodyType},
  {RESPOND_VOLUME,           strVolume, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, NULL, &visp_eeprom.volume},
  {RESPOND_RATE,             strRate, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, NULL, &visp_eeprom.rate},
  {RESPOND_BREATH_INTERVAL,  strBreathInterval, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, NULL, &visp_eeprom.breath_interval},
  {RESPOND_BREATH_THRESHOLD, strBreathThreshold, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, NULL, &visp_eeprom.breath_threshold},
  {RESPOND_MOTOR_SPEED,      strMotorSpeed, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, motorAction, &visp_eeprom.motor_speed},
  {RESPOND_CALIB0,           strCalib0, -1000, 1000, NULL, noSet, respondFloat, NULL, &visp_eeprom.calibrationOffsets[0]},
  {RESPOND_CALIB1,           strCalib1, -1000, 1000, NULL, noSet, respondFloat, NULL, &visp_eeprom.calibrationOffsets[1]},
  {RESPOND_CALIB2,           strCalib2, -1000, 1000, NULL, noSet, respondFloat, NULL, &visp_eeprom.calibrationOffsets[2]},
  {RESPOND_CALIB3,           strCalib3, -1000, 1000, NULL, noSet, respondFloat, NULL, &visp_eeprom.calibrationOffsets[3]},
  {RESPOND_SENSOR0,          strSensor0, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[0].sensorType},
  {RESPOND_SENSOR1,          strSensor1, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[1].sensorType},
  {RESPOND_SENSOR2,          strSensor2, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[2].sensorType},
  {RESPOND_SENSOR3,          strSensor3, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[3].sensorType},
  {0, NULL, 0, 0, NULL, NULL, NULL, NULL}
};


bool noSet(struct settingsEntry_s *entry, const char *arg)
{
  info(PSTR(SFLASH " is read-only"), entry->theName);
  return true;
}

bool verifyDictWordToInt8(struct settingsEntry_s *entry, const char *arg)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;

  do
  {
    memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
    d++;
    if (dict.theWord && strcasecmp_P(arg, dict.theWord) == 0)
    {
      *(uint8_t *)entry->data = dict.theAssociatedValue;
      return true;
    }
  } while (dict.theWord);
  return false;
}

bool verifyDictWordToInt16(struct settingsEntry_s *entry, const char *arg)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;

  do
  {
    memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
    d++;
    if (dict.theWord && strcasecmp_P(arg, dict.theWord) == 0)
    {
      *(uint16_t *)entry->data = dict.theAssociatedValue;
      return true;
    }
  } while (dict.theWord);
  return false;
}

bool verifyLimitsToInt16(struct settingsEntry_s *entry, const char *arg)
{
  int value = strtol(arg, NULL, 0);
  // TODO: other checks on the argument...
  if (value >= entry->theMin && value <= entry->theMax)
  {
    *(uint16_t *)entry->data = value;
    return true;
  }
  return false;
}

void respondInt8(struct settingsEntry_s *entry)
{
  respond('S', PSTR("" SFLASH ",%d"), entry->theName, *(int8_t *)entry->data);
}
void respondInt16(struct settingsEntry_s *entry)
{
  respond('S', PSTR("" SFLASH ",%d"), entry->theName, *(int16_t *)entry->data);
}
void respondFloat(struct settingsEntry_s *entry)
{
  char buff[32];
  *buff = 0;
  dtostrf(*(float*)entry->data, 7, 2, buff);
  respond('S', PSTR("" SFLASH ",%s"), entry->theName, buff);
}

void respondInt8ToDict(struct settingsEntry_s *entry)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;

  do
  {
    // Must copy from flash to access it
    memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
    d++;
    if (dict.theWord)
    {
      if (dict.theAssociatedValue == *(int8_t*)entry->data)
      {
        respond('S', PSTR("" SFLASH "," SFLASH ""), entry->theName, dict.theWord);
        return;
      }
    }
  } while (dict.theWord);
}

void respondSettingLimits(struct settingsEntry_s *entry)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;
  if (entry->personalDictionary)
  {
    char buffer[80];

    hwSerial.print('S');
    hwSerial.print(',');
    hwSerial.print(millis());
    hwSerial.print(',');
    snprintf_P(buffer, sizeof(buffer), PSTR("" SFLASH ""), entry->theName);
    hwSerial.print(buffer);
    hwSerial.print(F("_dict"));
    do
    {
      memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
      d++;
      if (dict.theWord)
      {
        // Must copy the string from flash memory
        snprintf_P(buffer, sizeof(buffer), PSTR("," SFLASH "," SFLASH ""), dict.theWord, dict.theDescription);
        hwSerial.print(buffer);
      }
    } while (dict.theWord);
    hwSerial.println();
  }
  else
  {
    respond('S', PSTR("" SFLASH "_min,%d"), entry->theName, entry->theMin);
    respond('S', PSTR("" SFLASH "_max,%d"), entry->theName, entry->theMax);
  }
}


void respondAppropriately(uint32_t flags)
{
  uint8_t x = 0;
  struct settingsEntry_s entry = {0};

  do
  {
    // We must copy the struct from flash to access it
    memcpy_P(&entry, & settings[x], sizeof(entry));
    x++;

    if (entry.bitmask & flags)
    {
      if (flags & RESPOND_LIMITS)
        respondSettingLimits(&entry);
      entry.respondIt(&entry);
    }
  }
  while (entry.bitmask != 0);
}

void handleSettingCommand(const char *arg1, const char *arg2)
{
  struct settingsEntry_s entry = {0};
  uint8_t x = 0;
  if (*arg1 == 0)
  {
    // Same as Q without the Limits
    respondAppropriately(0xFFFFFFFF ^ RESPOND_LIMITS);
    return;
  }

  do
  {
    // We must copy the struct from flash to access it
    memcpy_P(&entry, & settings[x], sizeof(entry));
    x++;
    if (entry.bitmask)
    {
      if ((strcasecmp_P(arg1, entry.theName) == 0))
      {
        if (*arg2 == 0)
        {
          entry.respondIt(&entry);
          return;
        }
        else if (entry.verifyIt(&entry, arg2))
        {
          saveParameters();
          entry.respondIt(&entry);
          // Maybe enable a motor or something?
          if (entry.actionIt)
            entry.actionIt(&entry);
          return;
        }
        else
          warning(PSTR("Invalid " SFLASH " value %s"), entry.theName, arg2);
      }
    }
  } while (entry.bitmask);

  warning(PSTR("Unknown setting '%s'"), arg1);
}


void commandParser(int cmdByte)
{
  static parser_state_e currentState = PARSE_COMMAND;
  static uint8_t command = 0;
  static char arg1[16] = "", arg2[16] = "";
  static uint8_t arg1Pos = 0, arg2Pos = 0;
  static uint16_t eepromAddress = 0xFFFF; // Invalid Address
  static uint16_t eepromCount = 0;
  const void(* resetFunc) (void) = 0;//declare reset function at address 0

  // Any kind of garbage input resets the state machine
  if (!isprint(cmdByte))
    currentState = PARSE_IGNORE_TILL_LF;

  // Ignore DOS CR
  if (cmdByte == '\r')
    return;

  // Need to be able to gracefully handle empty lines
  if (cmdByte == '\n')
  {
    // Process the command here
    currentState = PARSE_COMMAND;

    switch (command) {
      case 'P': respond('P', PSTR("I am alive!")); break;
      case 'I':  respond('I', PSTR("VISP Core,%d,%d,%c"), VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION); break;
      case 'C':
        clearCalibrationData();
        break;
      case 'E':
        if (eepromAddress == 0xFFFF)
        {
          // arg1 has an address
          if (*arg1)
            sendEEPROMdata(strtol(arg1, NULL, 0), 16); // If user supplied an address, only 16 bytes
          else
            sendEEPROMdata(0, 128);
        }
        else
        {
          if (*arg1)
          {
            updateEEPROMdata(eepromAddress + eepromCount, strtol(arg1, NULL, 0));
            eepromCount++;
            // Write the whole thing.   Kinda wasteful as we should really do it on a Commit...
            writeEEPROM(eeprom, (unsigned short)0, (unsigned char *)&visp_eeprom, sizeof(visp_eeprom));
          }
          sendEEPROMdata(eepromAddress, eepromCount);
        }

        break;
      case 'Q':
        sendCurrentSystemHealth();
        respondAppropriately(0xFFFFFFFF);
        respond('Q', PSTR("Finished"));
        break;
      case 'S': // Set parameter
        handleSettingCommand(arg1, arg2); break;
        break;
      case 'R': // Reboot chip
        resetFunc(); //call reset
        break;
    }
    return;
  }

  switch (currentState) {
    case PARSE_COMMAND:
      if (cmdByte == ',')
      {
        // Command with arguments
        switch (command) {
          case 'S':  currentState = PARSE_FIRST_ARG;    break;
          case 'M':  currentState = PARSE_SINGLE_ARG;   break;
          case 'E':  currentState = PARSE_EEPROM_ARGS;  break;
          default: currentState = PARSE_IGNORE_TILL_LF; break;
        }
      }
      else
      {
        // And reset!
        command = cmdByte;
        arg1Pos = 0;
        memset(arg1, 0, sizeof(arg1));
        arg2Pos = 0;
        memset(arg2, 0, sizeof(arg2));
        eepromAddress = 0xFFFF;
        eepromCount = 0;
      }
      break;
    case PARSE_FIRST_ARG:
    case PARSE_SINGLE_ARG:
      if (cmdByte == ',')
      {
        currentState = (currentState == PARSE_SINGLE_ARG ? PARSE_IGNORE_TILL_LF : PARSE_SECOND_ARG); // Default
      }
      else
      {
        if (arg1Pos < (sizeof(arg1) - 1))
        {
          arg1[arg1Pos++] = cmdByte;
        }
      }
      break;
    case PARSE_SECOND_ARG:
      if (cmdByte == ',')
      {
        currentState = PARSE_IGNORE_TILL_LF; // Default
      }
      else
      {
        if (arg2Pos < (sizeof(arg2) - 1))
        {
          arg2[arg2Pos++] = cmdByte;
        }
      }
      break;
    case PARSE_EEPROM_ARGS:
      if (cmdByte == ',')
      {
        if (eepromAddress == 0xFFFF)
          eepromAddress = strtol(arg1, NULL, 0);
        else
        {
          updateEEPROMdata(eepromAddress + eepromCount, strtol(arg1, NULL, 0));
          eepromCount++;
        }
        arg1Pos = 0;
        memset(arg1, 0, sizeof(arg1));
      }
      else
      {
        if (arg1Pos < (sizeof(arg1) - 1))
          arg1[arg1Pos++] = cmdByte;
      }
      break;
    case PARSE_IGNORE_TILL_LF:
      break;
  }
}


void loop() {
  static float P[4], T[4];

  for (int x = 0; tasks[x].cbk; x++)
    tCheck(&tasks[x]);

  if (timeToReadSensors)
  {
    timeToReadSensors = false;

    // Read them all NOW
    if (sensorsFound)
    {
      for (int x = 0; x < 4; x++)
      {
        P[x] = 0;
        // If any of the sensors fail, stop trying to do others
        if (!sensors[x].calculate(&sensors[x], &P[x], &T[x]))
          break;
      }
    }

    // OK, the cable might have just been unplugged, and the sensors have gone away.
    // Hence the double checks one above, and this one below
    if (sensorsFound)
    {
      switch (visp_eeprom.bodyType)
      {
        case 'P':
          loopPitotVersion(P, T);
          break;
        case 'V':
        default:
          loopVenturiVersion(P, T);
          break;
      }
    }
  }

  // Handle user input, 1 character at a time
  while (hwSerial.available())
    commandParser(hwSerial.read());
}
