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

// Chip type detection
#define DETECTION_MAX_RETRY_COUNT   2



#ifdef WANT_SPL06

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

bool spl06Calculate(baroDev_t * baro)
{

    // Is the pressure ready?
    //if (!(busRead(baro->busDev, SPL06_MODE_AND_STATUS_REG, &sstatus) && (sstatus & SPL06_MEAS_CFG_PRESSURE_RDY)))
    //  return false;   // error reading status or pressure is not ready
    // if (!spl06_read_pressure(baro))
    //  return false;
    if (!spl06_read_pressure(baro))
      return false;
    baro->pressure = spl06_compensate_pressure(baro, baro->chip.spl06.pressure_raw, baro->chip.spl06.temperature_raw);

    // Is the temperature ready?
    //if (!(busRead(baro->busDev, SPL06_MODE_AND_STATUS_REG, &sstatus) && (sstatus & SPL06_MEAS_CFG_TEMPERATURE_RDY)))
    //  return false;   // error reading status or pressure is not ready
    // if (!spl06_read_temperature(baro))
    //  return false;
    if (!spl06_read_temperature(baro))
      return false;
    baro->temperature = spl06_compensate_temperature(baro, baro->chip.spl06.temperature_raw);

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
#else
bool spl06Detect(baroDev_t *baro, busDevice_t *busDev)
{
  return false;
}
#endif


#ifdef WANT_BMP280
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

bool bmp280Calculate(baroDev_t * baro)
{
  int32_t t;
  uint32_t p;

  if (!bmp280GetUp(baro))
    return false;

  t = bmp280CompensateTemperature(baro, baro->chip.bmp280.ut); // Must happen before bmp280CompensatePressure() (see t_fine)
  p = bmp280CompensatePressure(baro, baro->chip.bmp280.up);

  baro->pressure = (p / 256.0);
  baro->temperature = t / 100.0;

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
#else
bool bmp280Detect(baroDev_t *baro, busDevice_t *busDev)
{
  return false;
}
#endif

#ifdef WANT_BMP388
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
  uint8_t status;
  if (busReadBuf(baro->busDev, BMP388_INT_STATUS_REG, &status, 1))
  {
    if (0 == (status &  (1 << 3)))
      busPrint(baro->busDev, PSTR("Data is NOT ready"));
  }

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



bool bmp388Calculate(baroDev_t * baro)
{
  float t;

  if (!bmp388GetUP(baro))
    return false;

  t = bmp388CompensateTemperature(baro);

  baro->pressure = bmp388CompensatePressure(baro, t);
  baro->temperature = t / 100.0;

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

#else
bool bmp388Detect(baroDev_t *baro, busDevice_t *busDev)
{
  return false;
}
#endif

void detectIndividualSensor(uint8_t devNum, uint8_t baroNum, TwoWire *wire, uint8_t address, uint8_t channel = 0, busDevice_t *muxDevice = NULL, busDeviceEnableCbk enableCbk=noEnableCbk)
{
  busDevice_t *device = busDeviceInitI2C(devNum, wire, address, channel, muxDevice, enableCbk);

  // busPrint(device, PSTR("Discovering sensor type"));
  if (!bmp280Detect(&sensors[baroNum], device))
    if (!bmp388Detect(&sensors[baroNum], device))
      if (!spl06Detect(&sensors[baroNum], device))
      {
        busPrint(device, PSTR("Unknown chip"));
        device = NULL;
      }
  device->hwType = HWTYPE_SENSOR;
  return;
}
