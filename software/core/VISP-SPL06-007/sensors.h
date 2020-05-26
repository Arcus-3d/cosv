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

#ifndef __SENSORS_H__
#define __SENSORS_H__


#define SENSOR_UNKNOWN 0
#define SENSOR_BMP388  1
#define SENSOR_BMP280  2
#define SENSOR_SPL06   3

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
typedef bool (*baroCalculateFuncPtr)(struct baroDev_s * baro);


typedef struct baroDev_s {
  busDevice_t * busDev;
  baroCalculateFuncPtr calculate;
  uint8_t sensorType; // Used by the interface to get the types of sensors we have
  float pressure;  // valid after calculate
  float temperature; // valid after calculate

  union {
#ifdef WANT_BMP280
    struct {
      bmp280_calib_param_t cal;
      // uncompensated pressure and temperature
      int32_t up;
      int32_t ut;
      //error free measurements
      int32_t up_valid;
      int32_t ut_valid;
    } bmp280;
#endif
#ifdef WANT_BMP388
    struct {
      bmp388_calib_param_t cal;
      // uncompensated pressure and temperature
      int32_t up;
      int32_t ut;
      //error free measurements
      int32_t up_valid;
      int32_t ut_valid;
    } bmp388;
#endif
#ifdef WANT_SPL06
    struct {
      spl06_coeffs_t cal;
      // uncompensated pressure and temperature
      int32_t pressure_raw;
      int32_t temperature_raw;
    } spl06;
#endif
  } chip;
} baroDev_t;

void detectIndividualSensor(uint8_t devNum, uint8_t sensorNum, TwoWire *wire, uint8_t address, uint8_t channel, busDevice_t *muxDevice, busDeviceEnableCbk enableCbk);

#endif
