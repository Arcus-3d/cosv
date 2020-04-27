/*
   This file is part of VISP.

   VISP is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Cleanflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <Wire.h>
#include <SPI.h>




// Chip type detection
#define DETECTION_MAX_RETRY_COUNT   5

float calibrationTotals[4];
int calibrationSampleCounter = 0;
float calibrationOffset[4];
float volumeAvg[20];

typedef enum {
  RUNSTATE_STARTUP = 0,
  RUNSTATE_CALIBRATE = 1,
  RUNSTATE_RUN = 2
} runState_e;

runState_e runState=RUNSTATE_STARTUP;

typedef enum {
  BUSTYPE_ANY  = 0,
  BUSTYPE_NONE = 0,
  BUSTYPE_I2C  = 1,
  BUSTYPE_SPI  = 2
} busType_e;

typedef enum {
  HWTYPE_NONE = 0,
  HWTYPE_SENSOR  = 1,
  HWTYPE_MUX  = 2
} hwType_e;

typedef struct busDevice_s {
  busType_e busType;
  hwType_e hwType;
  uint8_t currentChannel; // If this device is a HWTYPE_MUX
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
    } i2c;
  } busdev;
} busDevice_t;


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

  bmp280_calib_param_t bmp280_cal;
  // uncompensated pressure and temperature
  int32_t bmp280_up;
  int32_t bmp280_ut;
  //error free measurements
  int32_t bmp280_up_valid;
  int32_t bmp280_ut_valid;

  spl06_coeffs_t spl06_cal;
  // uncompensated pressure and temperature
  int32_t spl06_pressure_raw;
  int32_t spl06_temperature_raw;
} baroDev_t;



uint8_t muxSelectChannel(busDevice_t *busDev, uint8_t channel)
{
  if (busDev && channel > 0 && channel < 5)
  {
    if (busDev->currentChannel != channel)
    {
      busDev->busdev.i2c.i2cBus->beginTransmission(busDev->busdev.i2c.address);
      busDev->busdev.i2c.i2cBus->write(1 << (channel - 1));
      return busDev->busdev.i2c.i2cBus->endTransmission();
    }
    return 0;
  }
  else {
    return 0xff;
  }
}


void busPrint(busDevice_t *bus, const char *function)
{
  Serial.print(function);
  if (bus->busType == BUSTYPE_I2C)
  {
    Serial.print("(I2C:");
    Serial.print(" address=");
    Serial.print(bus->busdev.i2c.address);
    // MUX based VISP
    if (bus->busdev.i2c.channel)
    {
      Serial.print(" channel=");
      Serial.print(bus->busdev.i2c.channel);
    }
    Serial.println(")");
    return;
  }
  if (bus->busType == BUSTYPE_SPI)
  {
    Serial.print("(SPI CSPIN=");
    Serial.print(bus->busdev.spi.csnPin);
    Serial.println(")");
    return;
  }
}

busDevice_t *busDeviceInitI2C(TwoWire *wire, uint8_t address, uint8_t channel = 0, busDevice_t *channelDev = NULL)
{
  busDevice_t *dev = (busDevice_t *)malloc(sizeof(busDevice_t));
  memset(dev, 0, sizeof(busDevice_t));
  dev->busType = BUSTYPE_I2C;
  dev->busdev.i2c.i2cBus = wire;
  dev->busdev.i2c.address = address;
  dev->busdev.i2c.channel = channel; // If non-zero, then it is a channel on a TCA9546 at mux
  dev->busdev.i2c.channelDev = channelDev;
  return dev;
}

busDevice_t *busDeviceInitSPI(SPIClass *spiBus, uint8_t csnPin)
{
  busDevice_t *dev = (busDevice_t *)malloc(sizeof(busDevice_t));
  memset(dev, 0, sizeof(busDevice_t));
  dev->busType = BUSTYPE_SPI;
  dev->busdev.spi.spiBus = spiBus;
  dev->busdev.spi.csnPin = csnPin;
  return dev;
}

// TODO: check to see if this is a mux...
void busDeviceFree(busDevice_t *dev)
{
  free(dev);
}



char busReadBuf(busDevice_t *busDev, unsigned char reg, unsigned char *values, uint8_t length)
{
  char x;
  int error;

  if (busDev->busType == BUSTYPE_I2C)
  {
    uint8_t address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);
    wire->write(reg);
    if (0 == (error = wire->endTransmission()))
    {
      wire->requestFrom(address, length);
      while (wire->available() != length) ; // wait until bytes are ready
      for (x = 0; x < length; x++)
      {
        values[x] = wire->read();
      }
      return (1);
    }
    Serial.print("endTransmission() returned ");
    Serial.println(error);
    return (0);
  }
  return (0);
}

char busWriteBuf(busDevice_t *busDev, unsigned char reg, unsigned char *values, char length)
{
  if (busDev->busType == BUSTYPE_I2C)
  {
    int address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);
    wire->write(values, length);
    if (0 == wire->endTransmission())
      return (1);
    else
      return (0);
  }
  return 1;
}


char busRead(busDevice_t *busDev, unsigned char reg, unsigned char *values)
{
  return busReadBuf(busDev, reg, values, 1);
}
char busWrite(busDevice_t *busDev, unsigned char reg, unsigned char value)
{
  unsigned char values[2] = {reg, value};
  return busWriteBuf(busDev, reg, values, 2);
}




void myprint(uint64_t value)
{
  const int NUM_DIGITS    = log10(value) + 1;

  char sz[NUM_DIGITS + 1];

  sz[NUM_DIGITS] =  0;
  for ( size_t i = NUM_DIGITS; i--; value /= 10)
  {
    sz[i] = '0' + (value % 10);
  }

  Serial.print(sz);
}
void myprintln(uint64_t value)
{
  myprint(value);
  Serial.println("");
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

// See datasheet Table 9
#define SPL06_PRESSURE_SAMPLING_RATE     SPL06_SAMPLE_RATE_16
#define SPL06_PRESSURE_OVERSAMPLING            32
#define SPL06_TEMPERATURE_SAMPLING_RATE     SPL06_SAMPLE_RATE_1
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
    baro->spl06_temperature_raw = spl06_temperature;
  }

  return ack;
}

bool spl06_read_pressure(baroDev_t * baro)
{
  uint8_t data[SPL06_PRESSURE_LEN];
  int32_t spl06_pressure;

  bool ack = busReadBuf(baro->busDev, SPL06_PRESSURE_START_REG, data, SPL06_PRESSURE_LEN);

  if (ack) {
    spl06_pressure = (int32_t)((data[0] & 0x80 ? 0xFF000000 : 0) | (((uint32_t)(data[0])) << 16) | (((uint32_t)(data[1])) << 8) | ((uint32_t)data[2]));
    baro->spl06_pressure_raw = spl06_pressure;
  }

  return ack;
}

// Returns temperature in degrees centigrade
float spl06_compensate_temperature(baroDev_t * baro, int32_t temperature_raw)
{
  const float t_raw_sc = (float)temperature_raw / spl06_raw_value_scale_factor(SPL06_TEMPERATURE_OVERSAMPLING);
  const float temp_comp = (float)baro->spl06_cal.c0 / 2 + t_raw_sc * baro->spl06_cal.c1;
  return temp_comp;
}

// Returns pressure in Pascal
float spl06_compensate_pressure(baroDev_t * baro, int32_t pressure_raw, int32_t temperature_raw)
{
  const float p_raw_sc = (float)pressure_raw / spl06_raw_value_scale_factor(SPL06_PRESSURE_OVERSAMPLING);
  const float t_raw_sc = (float)temperature_raw / spl06_raw_value_scale_factor(SPL06_TEMPERATURE_OVERSAMPLING);

  const float pressure_cal = (float)baro->spl06_cal.c00 + p_raw_sc * ((float)baro->spl06_cal.c10 + p_raw_sc * ((float)baro->spl06_cal.c20 + p_raw_sc * baro->spl06_cal.c30));
  const float p_temp_comp = t_raw_sc * ((float)baro->spl06_cal.c01 + p_raw_sc * ((float)baro->spl06_cal.c11 + p_raw_sc * baro->spl06_cal.c21));

  return pressure_cal + p_temp_comp;
}

bool spl06_calculate(baroDev_t * baro, float * pressure, float * temperature)
{
  if (pressure) {
    spl06_read_pressure(baro);
    *pressure = spl06_compensate_pressure(baro, baro->spl06_pressure_raw, baro->spl06_temperature_raw);
  }

  if (temperature) {
    spl06_read_temperature(baro);
    *temperature = spl06_compensate_temperature(baro, baro->spl06_temperature_raw);
  }

  return true;
}


bool read_calibration_coefficients(baroDev_t *baro) {
  uint8_t sstatus;
  if (!(busRead(baro->busDev, SPL06_MODE_AND_STATUS_REG, &sstatus) && (sstatus & SPL06_MEAS_CFG_COEFFS_RDY)))
    return false;   // error reading status or coefficients not ready

  uint8_t caldata[SPL06_CALIB_COEFFS_LEN];

  if (!busReadBuf(baro->busDev, SPL06_CALIB_COEFFS_START, (uint8_t *)&caldata, SPL06_CALIB_COEFFS_LEN)) {
    return false;
  }

  baro->spl06_cal.c0 = (caldata[0] & 0x80 ? 0xF000 : 0) | ((uint16_t)caldata[0] << 4) | (((uint16_t)caldata[1] & 0xF0) >> 4);
  baro->spl06_cal.c1 = ((caldata[1] & 0x8 ? 0xF000 : 0) | ((uint16_t)caldata[1] & 0x0F) << 8) | (uint16_t)caldata[2];
  baro->spl06_cal.c00 = (caldata[3] & 0x80 ? 0xFFF00000 : 0) | ((uint32_t)caldata[3] << 12) | ((uint32_t)caldata[4] << 4) | (((uint32_t)caldata[5] & 0xF0) >> 4);
  baro->spl06_cal.c10 = (caldata[5] & 0x8 ? 0xFFF00000 : 0) | (((uint32_t)caldata[5] & 0x0F) << 16) | ((uint32_t)caldata[6] << 8) | (uint32_t)caldata[7];
  baro->spl06_cal.c01 = ((uint16_t)caldata[8] << 8) | ((uint16_t)caldata[9]);
  baro->spl06_cal.c11 = ((uint16_t)caldata[10] << 8) | (uint16_t)caldata[11];
  baro->spl06_cal.c20 = ((uint16_t)caldata[12] << 8) | (uint16_t)caldata[13];
  baro->spl06_cal.c21 = ((uint16_t)caldata[14] << 8) | (uint16_t)caldata[15];
  baro->spl06_cal.c30 = ((uint16_t)caldata[16] << 8) | (uint16_t)caldata[17];

  return true;
}

bool spl06_configure_measurements(baroDev_t *baro)
{
  uint8_t reg_value;

  reg_value = SPL06_TEMP_USE_EXT_SENSOR | spl06_samples_to_cfg_reg_value(SPL06_TEMPERATURE_OVERSAMPLING);
  reg_value |= SPL06_TEMPERATURE_SAMPLING_RATE << 4;
  if (!busWrite(baro->busDev, SPL06_TEMPERATURE_CFG_REG, reg_value)) {
    return false;
  }

  reg_value = spl06_samples_to_cfg_reg_value(SPL06_PRESSURE_OVERSAMPLING);
  reg_value |= SPL06_PRESSURE_SAMPLING_RATE << 4;
  if (!busWrite(baro->busDev, SPL06_PRESSURE_CFG_REG, reg_value)) {
    return false;
  }

  reg_value = 0;
  if (SPL06_TEMPERATURE_OVERSAMPLING > 8) {
    reg_value |= SPL06_TEMPERATURE_RESULT_BIT_SHIFT;
  }
  if (SPL06_PRESSURE_OVERSAMPLING > 8) {
    reg_value |= SPL06_PRESSURE_RESULT_BIT_SHIFT;
  }
  if (!busWrite(baro->busDev, SPL06_INT_AND_FIFO_CFG_REG, reg_value)) {
    return false;
  }

  busWrite(baro->busDev, SPL06_MODE_AND_STATUS_REG, SPL06_MEAS_PRESSURE | SPL06_MEAS_TEMPERATURE | SPL06_MEAS_CFG_CONTINUOUS);
  return true;
}


bool spl06DeviceDetect(busDevice_t * busDev)
{
  uint8_t chipId;
  for (int retry = 0; retry < DETECTION_MAX_RETRY_COUNT; retry++) {
    delay(100);
    bool ack = busRead(busDev, SPL06_CHIP_ID_REG, &chipId);
    if (ack && chipId == SPL06_DEFAULT_CHIP_ID) {
      busPrint(busDev, "SPL206 Detected");
      return true;
    }
  }

  return false;
}


bool spl06Detect(baroDev_t *baro, busDevice_t *busDev)
{
  if (busDev == NULL) {
    return false;
  }

  if (!spl06DeviceDetect(busDev)) {
    return false;
  }
  baro->busDev = busDev;

  if (!(read_calibration_coefficients(baro) && spl06_configure_measurements(baro))) {
    baro->busDev = NULL;
    return false;
  }

  baro->calculate = spl06_calculate;
  return true;
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


bool bmp280_get_up(baroDev_t * baro)
{
  uint8_t data[BMP280_DATA_FRAME_SIZE];

  //read data from sensor (Both pressure and temperature, at once)
  bool ack = busReadBuf(baro->busDev, BMP280_PRESSURE_MSB_REG, data, BMP280_DATA_FRAME_SIZE);

  //check if pressure and temperature readings are valid, otherwise use previous measurements from the moment
  if (ack) {
    baro->bmp280_up = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
    baro->bmp280_ut = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
    baro->bmp280_up_valid = baro->bmp280_up;
    baro->bmp280_ut_valid = baro->bmp280_ut;
  }
  else {
    //assign previous valid measurements
    baro->bmp280_up = baro->bmp280_up_valid;
    baro->bmp280_ut = baro->bmp280_ut_valid;
  }

  return ack;
}

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of "5123" equals 51.23 DegC
// t_fine carries fine temperature as global value
int32_t bmp280_compensate_T(baroDev_t * baro, int32_t adc_T)
{
  int32_t var1, var2, T;

  var1 = ((((adc_T >> 3) - ((int32_t)baro->bmp280_cal.dig_T1 << 1))) * ((int32_t)baro->bmp280_cal.dig_T2)) >> 11;
  var2  = (((((adc_T >> 4) - ((int32_t)baro->bmp280_cal.dig_T1)) * ((adc_T >> 4) - ((int32_t)baro->bmp280_cal.dig_T1))) >> 12) * ((int32_t)baro->bmp280_cal.dig_T3)) >> 14;
  baro->bmp280_cal.t_fine = var1 + var2;
  T = (baro->bmp280_cal.t_fine * 5 + 128) >> 8;
  return T;
}

// NOTE: bmp280_compensate_T() must be called before this, so that t_fine is computed)
// Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits).
// Output value of "24674867" represents 24674867/256 = 96386.2 Pa = 963.862 hPa
uint32_t bmp280_compensate_P(baroDev_t * baro, int32_t adc_P)
{
  int64_t var1, var2, p;
  var1 = ((int64_t)baro->bmp280_cal.t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)baro->bmp280_cal.dig_P6;
  var2 = var2 + ((var1 * (int64_t)baro->bmp280_cal.dig_P5) << 17);
  var2 = var2 + (((int64_t)baro->bmp280_cal.dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)baro->bmp280_cal.dig_P3) >> 8) + ((var1 * (int64_t)baro->bmp280_cal.dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)baro->bmp280_cal.dig_P1) >> 33;
  if (var1 == 0) {
    return 0; // avoid exception caused by division by zero
  }

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)baro->bmp280_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)baro->bmp280_cal.dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)baro->bmp280_cal.dig_P7) << 4);
  return (uint32_t)p;
}

bool bmp280_calculate(baroDev_t * baro, float * pressure, float * temperature)
{
  int32_t t;
  uint32_t p;

  bmp280_get_up(baro);
  t = bmp280_compensate_T(baro, baro->bmp280_ut); // Must happen before bmp280_compensate_P() (see t_fine)
  p = bmp280_compensate_P(baro, baro->bmp280_up);

  if (pressure) {
    *pressure = (p / 256.0);
  }

  if (temperature) {
    *temperature = t / 100.0;
  }

  return true;
}

bool bmp280DeviceDetect(busDevice_t * busDev)
{
  for (int retry = 0; retry < DETECTION_MAX_RETRY_COUNT; retry++) {
    uint8_t chipId = 0;

    delay(100);

    bool ack = busRead(busDev, BMP280_CHIP_ID_REG, &chipId);
    if (ack && chipId == BMP280_DEFAULT_CHIP_ID) {
      return true;
    }
  }

  return false;
}

bool bmp280Detect(baroDev_t *baro, busDevice_t *busDev)
{
  if (busDev == NULL) {
    return false;
  }

  if (!bmp280DeviceDetect(busDev)) {
    return false;
  }
  baro->busDev = busDev;

  busPrint(busDev, "BMP280 Detected");

  // read calibration
  busReadBuf(baro->busDev, BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG, (uint8_t *)&baro->bmp280_cal, 24);

  //set filter setting and sample rate
  busWrite(baro->busDev, BMP280_CONFIG_REG, BMP280_FILTER | BMP280_SAMPLING);

  // set oversampling + power mode (forced), and start sampling
  busWrite(baro->busDev, BMP280_CTRL_MEAS_REG, BMP280_MODE);

  delay(100);

  baro->calculate = bmp280_calculate;
  return true;
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
// Index mapping of the sensors to the board (See schematic)
#define SENSOR_U5 0
#define SENSOR_U6 1
#define SENSOR_U7 2 // PITOT TUBE
#define SENSOR_U8 3 // PITOT TUBE
baroDev_t sensors[4];

bool detectMuxedSensors(TwoWire *wire)
{
  uint8_t channel, address;
  // MUX has a switching chip that can have different adresses (including ones on our devices)
  // Could be 2 paths with 2 sensors each or 4 paths with 1 on each.
  wire->beginTransmission(0x70);
  if (0 == wire->endTransmission())
  {
    busDevice_t *muxDevice = busDeviceInitI2C(wire, 0x70);
    Serial.println("MUX Based VISP Detected");
    for (channel = 1; channel < 3; channel++)
    {
      muxSelectChannel(muxDevice, channel);
      for (address = 0x76; address < 0x78; address++)
      {
        wire->beginTransmission(address);
        if (0 == wire->endTransmission())
        {
          uint8_t sensorNum = (address - 0x76) + ((channel - 1) * 2);
          busDevice_t *device = busDeviceInitI2C(wire, address, channel, muxDevice);
          if (!bmp280Detect(&sensors[sensorNum], device))
            if (!spl06Detect(&sensors[sensorNum], device))
            {
              busDeviceFree(device);
              Serial.print("Device refused to be detected at 0x");
              Serial.println(address, HEX);
            }
        }
      }
    }
    return true;
  }

  return false;
}

bool detectXLateSensors(TwoWire *wire)
{
  uint8_t channel, address;
  // XLATE version has chips at 0x74, 0x75, 0x76, and 0x77
  // So, if we find 0x74... We are good to go
  wire->beginTransmission(0x74);
  if (0 == wire->endTransmission())
  {
    for (address = 0x74; address < 0x78; address++)
    {
      wire->beginTransmission(address);
      if (0 == wire->endTransmission())
      {
        busDevice_t *device = busDeviceInitI2C(wire, address);
        if (!bmp280Detect(&sensors[address - 0x74], device))
        {
          if (!spl06Detect(&sensors[address - 0x74], device))
          {
            Serial.print("Device refused to be detected at 0x");
            Serial.println(address, HEX);
            busDeviceFree(device);
          }
        }
      }
    }
    return true;
  }
  return false;
}

bool detectDualI2CSensors(TwoWire *wireA, TwoWire *wireB)
{
  int channel, sensorCount = 0;
  uint8_t address;

  Serial.println("Assuming DUAL I2C VISP");
  for (address = 0x76; address < 0x78; address++)
  {
    // First Bus
    wireA->beginTransmission(address);
    if (0 == wireA->endTransmission())
    {
      busDevice_t *device = busDeviceInitI2C(wireA, address);
      if (!bmp280Detect(&sensors[address - 0x76], device))
      {
        if (!spl06Detect(&sensors[address - 0x76], device))
        {
          Serial.print("Device refused to be detected at 0x");
          Serial.println(address, HEX);
          busDeviceFree(device);
        }
      }
    }

    // Optional Second bus (TEENSY, MEGA, etc...)...
    if (wireB)
    {
      wireB->beginTransmission(address);
      if (0 == wireB->endTransmission())
      {
        busDevice_t *device = busDeviceInitI2C(wireB, address);
        if (!bmp280Detect(&sensors[(address - 0x76) + 2], device))
        {
          if (!spl06Detect(&sensors[(address - 0x76) + 2], device))
            busDeviceFree(device);
        }
      }
    }
  }

  return true;
}

// FUTURE: read EEPROM and determine what type of VISP it is.
bool detectSensors()
{
  memset(&sensors, 0, sizeof(sensors));

  if (detectMuxedSensors(&Wire))
    return true;

  if (detectXLateSensors(&Wire))
    return true;
#ifdef TEENSY
  if (detectDualI2CSensors(&Wire, &Wire1))
    return true;
#else
  if (detectDualI2CSensors(&Wire, NULL))
    return true;
#endif
  return false;
}




bool timeToReadSensors = false;


void scan_i2c(TwoWire *wire)
{
  byte error, address; //variable for error and I2C address
  int nDevices;

  Serial.println("Scanning Hardware I2C bus...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    wire->beginTransmission(address);
    error = wire->endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");

}


/*** Timer callback subsystem ***/

typedef void (*tCBK)() ;
typedef struct t  {
  unsigned long tStart;
  unsigned long tTimeout;
  tCBK cbk;
};


bool tCheck (struct t * t ) {
  if (millis() > t->tStart + t->tTimeout)
  {
    t->cbk();
    t->tStart = millis();
  }
}


void timeToReadVISP()
{
  timeToReadSensors = true;
}

// Timer Driven Tasks and their Schedules.
// These are checked and executed in order.
// If somethign takes priority over another task, put it at the top of the list
t tasks[] = {
  {0, 20, timeToReadVISP},
  { -1, 0, NULL} // End of list
};

/*** End of timer callback subsystem ***/



void setup() {
  uint8_t pbar = 0;
  uint64_t chipid;

  Serial.begin(230400);
  Serial.println("VISP Sensor Reader Test Application V0.1a");
  Wire.begin();
  Wire.setClock(400000); // Typical
#ifdef TEENSY
  Wire1.begin();
#endif

  Wire.beginTransmission(0x70);
  if (0 == Wire.endTransmission())
  {
    busDevice_t *mux = busDeviceInitI2C(&Wire, 0x70);
    Serial.println("Scanning MUX Bus 1");
    muxSelectChannel(mux, 1);
    scan_i2c(&Wire);

    Serial.println("Scanning MUX Bus 2");
    muxSelectChannel(mux, 2);
    scan_i2c(&Wire);
  }
  else
    scan_i2c(&Wire);

#ifdef TEENSY
  Wire1.begin();
  Wire1.setClock(400000); // Typical
  Serial.println("Scanning Second I2C bus");
  Wire1.beginTransmission(0x70);
  if (0 == Wire1.endTransmission())
  {
    busDevice_t *mux = busDeviceInitI2C(&Wire1, 0x70);
    Serial.println("Scanning MUX Bus 1");
    muxSelectChannel(mux, 1);
    scan_i2c(&Wire1);

    Serial.println("Scanning MUX Bus 2");
    muxSelectChannel(mux, 2);
    scan_i2c(&Wire1);
  }
  else
    scan_i2c(&Wire1);
#endif

  delay(200);

  detectSensors();
}

void loop() {
  String command;
  float P[4], T[4], airflow, volume, pitot_diff, ambientPressure, pitot1, pitot2, patientPressure, pressure;

  for (int x = 0; tasks[x].cbk; x++)
    tCheck(&tasks[x]);

  if (timeToReadSensors)
  {
    timeToReadSensors = false;

    // Read them all NOW
    for (int x = 0; x < 4; x++)
    {
      P[x] = 0;
      if (sensors[x].busDev)
        sensors[x].calculate(&sensors[x], &P[x], &T[x]);
    }
    if (Serial.available())
    {
      command = Serial.readString(); //reads serial input
      if (command == 'calibrate')
      {
        runState = RUNSTATE_CALIBRATE;
      }
    }
    if (runState == RUNSTATE_STARTUP || runState == RUNSTATE_CALIBRATE)
    {
      calibrationTotals[0] += P[0];
      calibrationTotals[1] += P[1];
      calibrationTotals[2] += P[2];
      calibrationTotals[3] += P[3];
      calibrationSampleCounter++;
      if (calibrationSampleCounter == 99) {
        float average = (calibrationTotals[0]+calibrationTotals[1]+calibrationTotals[2]+calibrationTotals[3])/400;
        for (int x = 0; x < 4; x++)
        {
          calibrationOffset[x] = calibrationTotals[x]/100 - average;
        }
        calibrationSampleCounter=0;
        runState = RUNSTATE_RUN;
      }
    }
    static float paTocmH2O = 0.0101972;
//    static float paTocmH2O = 0.00501972;
    ambientPressure = P[1] - calibrationOffset[1];
    pitot1 = P[2] - calibrationOffset[2];
    pitot2 = P[3] - calibrationOffset[3];
    patientPressure = P[0] - calibrationOffset[0];

    pitot_diff=(pitot1-pitot2); // pascals to hPa
    
    airflow = (0.05*pitot_diff*pitot_diff - 0.0008*pitot_diff); // m/s
    airflow=20*sqrt(abs(pitot_diff/patientPressure));
    if (pitot_diff < 0) {
      airflow = -airflow;
    }
    //airflow = -(-0.0008+sqrt(0.0008*0.0008-4*0.05*0.0084+4*0.05*pitot_diff))/2*0.05;
    
    volume=airflow * 0.25 * 60; // volume for our 18mm orfice, and 60s/min
    pressure = (patientPressure - ambientPressure)*paTocmH2O; // average of all sensors in the tube for pressure reading
    
    // Take some time to write to the serial port
    Serial.print(millis());
    Serial.print(",");
    Serial.print(pressure, 4);
    Serial.print(",");
    Serial.print(volume, 4);
    Serial.print(",");
    Serial.print(ambientPressure, 1);
    Serial.print(",");
    Serial.print(patientPressure, 1);
    Serial.print(",");
    Serial.print(pitot1, 1);
    Serial.print(",");
    Serial.println(pitot2, 1);
  }
}