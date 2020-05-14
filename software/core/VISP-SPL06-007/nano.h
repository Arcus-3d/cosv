#ifndef __NANO_H__
#define __NANO_H__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <SPI.h>

#define hwSerial Serial

#define PUTINFLASH PROGMEM

#define WANT_BMP388 1 // 1858 bytes and 112bytes of ram
#define WANT_BMP280 1 // 2306 bytes
#define WANT_SPL06  1 // 1350 bytes

#define MAX_ANALOG 1024
#define MAX_PWM 255


//The Arduino has 3 timers and 6 PWM output pins. The relation between timers and PWM outputs is:
//
//    Pins 5 and 6: controlled by Timer0   (On the Uno and similar boards, pins 5 and 6 have a frequency of approximately 980 Hz.)
//    Pins 9 and 10: controlled by Timer1  (490 Hz)
//    Pins 11 and 3: controlled by Timer2  (490 Hz)
//
// We should rebalance the PWM's to give Timer1 or Timer2 to AccelStepper.
//
// The Arduino does not have a built-in digital-to-analog converter (DAC),
// but it can pulse-width modulate (PWM) a digital signal to achieve some
// of the functions of an analog output. The function used to output a
// PWM signal is analogWrite(pin, value). pin is the pin number used for
// the PWM output. value is a number proportional to the duty cycle of
// the signal.
// When value = 0, the signal is always off.
// When value = 255, the signal is always on.
// On most Arduino boards, the PWM function is available on pins 3, 5, 6, 9, 10, and 11. The frequency of the PWM signal on most pins is approximately 490 Hz.

// If ADC_MODE is set to zero, then the display controls things
#define ADC_VOLUME   A0 // How much volume when in VC-CMV mode, 0->1023 milliliters
#define ADC_PRESSURE A1 // How much pressure when in PC-CMV mode
#define ADC_RATIO    A2 // How much time to push, scaled to the MIN_BREATH_RATIO to MAX_BREATH_RATIO
#define ADC_RATE     A3 // Breaths Per Minute (Scaled to MIN_BREATH_RATE and MAX_BREATH_RATE)
#define ADC_SDA      A4 // Used by I2C
#define ADC_SCL      A5 // Used by I2C
#define ADC_MODE     A6 // Crude mode selection divided down into 3 'modes', 'display-controlled', 'forced-pc-cmv', 'forced-vc-cmv'
#define ADC_BATTERY  A7 // Battery monitoring of portable unit

#define HOME_SENSOR  2 // IRQ on low.


// All motors use these 4 signals.
#define MOTOR_PIN_A    3 // MUST BE IRQ CAPABLE!  (STEPPER_ENABLE, BLDC_FEEDBACK)  
#define MOTOR_PIN_B    4 // (MOTOR_HBRIDGE_R_EN)
#define MOTOR_PIN_C    5 // (MOTOR_HBRIDGE_L_EN, MOTOR_STEPPER_DIR, MOTOR_BLDC_DIR)
#define MOTOR_PIN_PWM  6 // HARDWARE PWM Capable output required (MOTOR_HBRIDGE_PWM, MOTOR_STEPPER_STEP, MOTOR_BLDC_PWM)

// Pins D4 and D5 are enable pins for NANO's NPN SCL enable pins
#define ENABLE_PIN_BUS_A 7 // Future: this toggles HIGH=busA, LOW=busB  NOTE: also is SPI 2.8" display DC/RS 
#define ENABLE_PIN_BUS_B 8

#define LINE_POWER_DETECTION 8 // Future: need to detect line power
#define MISSING_PULSE_PIN  9   // Output a pulse every time we check the sensors.

// This leaves the entire SPI bus available for a display or alternate IO:  10, 11, 12, and 13
// DC/RS =D7 // Same as ENABLE_PIN_BUS_A, fine to use for both
// SS=D10
// MOSI=D11
// MISO=D12
// SCL=D13
// 2.8" screen needs another signal...
// DC/RS: LCD register / data selection signal, high level: register, low level: data

#endif
