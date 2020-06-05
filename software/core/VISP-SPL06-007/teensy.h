#ifndef __TEENSY_H__
#define __TEENSY_H__

#include <Wire.h>
#include <SPI.h>

#define hwSerial Serial1
//#define hwSerial Serial

#include <IntervalTimer.h>

#define PUTINFLASH

#define WANT_BMP388 1
#define WANT_BMP280 1
#define WANT_SPL06  1

#define MAX_ANALOG 4096
#define MAX_PWM 65536


// If ADC_MODE is set to zero, then the display controls things
#define ADC_VOLUME   A0 // How much volume when in VC-CMV mode, 0->1023 milliliters
#define ADC_PRESSURE A1 // How much pressure when in PC-CMV mode
#define SCL1         A2 // Used by I2C
#define SDA1         A3 // Used by I2C
#define SDA0         A4 // Used by I2C
#define SCL0         A5 // Used by I2C
#define ADC_RATIO    A6 // How much time to push, scaled to the MIN_BREATH_RATIO to MAX_BREATH_RATIO
#define ADC_RATE     A7 // Breaths Per Minute (Scaled to MIN_BREATH_RATE and MAX_BREATH_RATE)
#define ADC_MODE     A8 // Crude mode selection divided down into 3 'modes', 'display-controlled', 'forced-pc-cmv', 'forced-vc-cmv'
#define ADC_BATTERY  A9 // Battery monitoring of portable unit

#define HWSERIAL_RX 0
#define HWSERIAL_TX 1

#define HOME_SENSOR  2 // IRQ on low.


// All motors use these 4 signals.
#define MOTOR_PIN_A    3 // MUST BE IRQ CAPABLE!  (ENCODER_FEEDBACK - IRQ on low.)
#define MOTOR_PIN_B    4 // (MOTOR_HBRIDGE_R_EN, MOTOR_STEPPER_ENABLE)
#define MOTOR_PIN_C    5 // (MOTOR_HBRIDGE_L_EN, MOTOR_STEPPER_DIR)
#define MOTOR_PIN_PWM  6 // HARDWARE PWM Capable output required (MOTOR_HBRIDGE_PWM, MOTOR_STEPPER_STEP)

#define LINE_POWER_DETECTION 7 // Future: need to detect line power
#define MISSING_PULSE_PIN    8 // Output a pulse every time we check the sensors.


// This leaves the entire SPI bus available for a display or alternate IO:  9, 10, 11, 12, and 13
// DC/RS=D9
// SS=D10
// MOSI=D11
// MISO=D12
// SCL=D13
// 2.8" screen needs another signal...
// DC/RS: LCD register / data selection signal, high level: register, low level: data

#endif
