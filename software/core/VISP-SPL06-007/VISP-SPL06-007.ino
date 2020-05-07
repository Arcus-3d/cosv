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
#include <AccelStepper.h>

#ifdef ARDUINO_TEENSY40
TwoWire *i2cBus1 = &Wire1;
TwoWire *i2cBus2 = &Wire2;
#elif ARDUINO_AVR_NANO
TwoWire *i2cBus1 = &Wire;
TwoWire *i2cBus2 = NULL;
#elif ARDUINO_AVR_UNO
TwoWire *i2cBus1 = &Wire;
TwoWire *i2cBus2 = NULL;
#else
#error Unsupported board selection.
#endif

#define STEPPER_STEPS_PER_REV 200
#define SWEEP_SPEED 150 // Ford F150 motor wiper
#define HOMING_SPEED 255

volatile bool motorFound = false;
volatile bool motorHoming = false;
uint8_t motorMinSpeedDetected = 0; // Max speed is 255 on the PWM
uint8_t motorMaxSpeedDetected = 0; // Max speed is 255 on the PWM

uint8_t currentMode = MODE_OFF;
debugState_e debug = DEBUG_DISABLED;


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

// Pins D4 and D5 are enable pins for NANO's NPN SCL enable pins
// This is detected if needed
#define ENABLE_PIN_BUS_A 4
#define ENABLE_PIN_BUS_B 5

#define M_PWM_1 6 // Hardware PWM on a Nano
#define M_PWM_2 9 // Hardware PWM on a Nano
#define M_DIR_1 12
#define M_DIR_2 13

#define STEPPER_DIR  7
#define STEPPER_STEP 8
#define HOME_SENSOR   3 // IRQ on low.

#define BLDC_DIR     10 // 
#define BLDC_PWM     11 // Hardware PWM output for BLDC motor
#define BLDC_FEEDBACK 2 // Interrupt pin, let's us know the core motor has done 1 Rev

// If all of the ADC's are set to zero, then the display controls things
#define ADC_VOLUME   A0 // How much to push, 0->1023 milliliters, if it reads 0, then display can control this (parameter)
#define ADC_RATIO    A1 // How much time to push, 0->1023.   Scaled to 1/2 the interval???
#define ADC_RATE     A2 // When to timeout and trigger a new breath.   0..1023 is scaled from 0 to 3 seconds.  0=display controls the parameter
#define ADC_HALL     A3 // Used for sensing home with Hall Effect Sensors
#define ADC_SDA      A4
#define ADC_SCL      A5
#define ADC_UNKNOWN1 A6
#define ADC_UNKNOWN2 A7


#define MISSING_PULSE_PIN  2   // Output a pulse every time we check the sensors.

// We only have ADC inputs A6 and A7 left...

AccelStepper mystepper(AccelStepper::DRIVER, STEPPER_STEP, STEPPER_DIR); // Direction: High means forward.
// mystepper adds 3674 bytes of code and 94 bytes of ram

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
  if (sensorsFound && motorFound)
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

int16_t modeBreathRateToMotorSpeed()
{
  //  if (currentMode == MODE_PCCMV)

  return motorMinSpeedDetected;
}

void homeThisPuppy(bool forced);


unsigned long timeToInhale = 0;
unsigned long timeToStopInhale = 0;
bool lastBreathMotorDirection = false;
void timeToCheckPatient()
{
  unsigned long theMillis = millis();
  // breathRate is in breaths per minute. timeout= 60*1000/bpm
  // breatRation is a 1:X where 1=inhale, and X=exhale.  So a 1:2 is 50% inhaling and 50% exhaling

  if (currentMode == MODE_OFF)
    return;

  if (theMillis > timeToInhale)
  {
    unsigned long nextBreathCycle = ((60.0 / (float)visp_eeprom.breath_rate) * 1000.0);
    timeToInhale = nextBreathCycle;
    timeToStopInhale = (nextBreathCycle / visp_eeprom.breath_ratio);

    info(PSTR("brate=%d  bratio=%d nextBreathCycle=%l timeToStopInhale=%l millis"), visp_eeprom.breath_rate, visp_eeprom.breath_ratio, nextBreathCycle, timeToStopInhale);

    timeToInhale += theMillis;
    timeToStopInhale += theMillis;

    // Switch direction each time.
    if (lastBreathMotorDirection)
    {
      motorForward(modeBreathRateToMotorSpeed());
      lastBreathMotorDirection = false;
    }
    else
    {
      motorReverse(modeBreathRateToMotorSpeed());
      lastBreathMotorDirection = true;
    }
  }

  if (timeToStopInhale >= 0 && theMillis > timeToStopInhale)
  {
    motorStop();
    homeThisPuppy(false);
    timeToStopInhale = -1;
  }
}

void timeToCheckSensors()
{
  // If debug is on, and a VISP is NOT connected, we flood the system with sensor scans.
  // Do it every half second (or longer)
  if (!sensorsFound)
    detectVISP(i2cBus1, i2cBus2, ENABLE_PIN_BUS_A, ENABLE_PIN_BUS_B);
}

// Timer Driven Tasks and their Schedules.
// These are checked and executed in order.
// If something takes priority over another task, put it at the top of the list
t tasks[] = {
  {0, 20, timeToReadVISP},
  {0, 5,  timeToCheckPatient},
  {0, 100, timeToPulseWatchdog},
  {0, 500, timeToCheckSensors},
  {0, 0, NULL} // End of list
};

/*** End of timer callback subsystem ***/



// Let the motor run until we get the correct number of pulses
void homeTriggered() // IRQ function
{
  // This is tested a lot, we need to fail on not homing.
  if (motorHoming)
  {
    motorFound = true;
    motorHoming = false;
  }

  motorStop();
}

bool motorWasGoingForward = false;
void motorReverse(int rate)
{
  motorWasGoingForward = false;
  digitalWrite(M_DIR_1, 0);
  digitalWrite(M_DIR_2, 1);
  //digitalWrite(BLDC_DIR, HIGH);

  analogWrite(M_PWM_1, rate);
  analogWrite(M_PWM_2, rate);

  //  analogWrite(BLDC_PWM, rate);
  rate = -rate;
  mystepper.setSpeed(rate);
}

void motorForward(int rate)
{
  motorWasGoingForward = true;
  digitalWrite(M_DIR_1, 1);
  digitalWrite(M_DIR_2, 0);
  //  digitalWrite(BLDC_DIR, LOW);

  analogWrite(M_PWM_1, rate);
  analogWrite(M_PWM_2, rate);

  //  analogWrite(BLDC_PWM, rate);
  mystepper.setSpeed(rate);
}

void motorStop()
{
  // Short the motor, so it stops faster
  digitalWrite(M_DIR_1, 0);
  digitalWrite(M_DIR_2, 0);

  analogWrite(M_PWM_1, 0);
  analogWrite(M_PWM_2, 0);

  //  analogWrite(BLDC_PWM, 0);
  mystepper.stop(); // Stop as fast as possible: sets new target
  mystepper.setSpeed(0);
}

void motorRunStepper()
{
  mystepper.runSpeed();
}

bool runWhileHomeSensorReads(int value)
{
  unsigned long timeout = millis() + 6000; // 6 seconds...
  do
  {
    if (digitalRead(HOME_SENSOR) != value)
      return true;
    motorRunStepper();
  }
  while (millis() < timeout);
  return false;
}

// If forced, this is for the initial setup
void homeThisPuppy(bool forced)
{
  if (forced || digitalRead(HOME_SENSOR) == HIGH)
  {
    motorHoming = true;
    if (motorWasGoingForward)
      motorReverse(HOMING_SPEED);
    else
      motorForward(HOMING_SPEED);
  }
}

unsigned long timeTillNextMagnet(uint8_t motorSpeed)
{
  unsigned long startTime, stopTime;

  if (motorFound)
  {
    startTime = millis();
    motorForward(motorSpeed);
    runWhileHomeSensorReads(LOW);
    runWhileHomeSensorReads(HIGH);
    stopTime = millis();

    return (stopTime - startTime);
  }

  return 0L;
}

void calibrateMotorSpeeds()
{
  if (motorFound)
  {
    unsigned long minTimeToGetToOtherMagnet = ((60000.0 / (float)MIN_BREATH_RATE) / MIN_BREATH_RATIO); // 10 and 1:2
    unsigned long maxTimeToGetToOtherMagnet = ((60000.0 / (float)MAX_BREATH_RATE) / MAX_BREATH_RATIO); // 40 and 1:5
    unsigned long runTime = 0;
    for (uint8_t x = 250; x > 10; x -= 10)
    {

      runTime = timeTillNextMagnet(x);
      if (runTime < maxTimeToGetToOtherMagnet || motorMaxSpeedDetected == 0)
      {
        if (motorMaxSpeedDetected == 0 && runTime > maxTimeToGetToOtherMagnet)
          info(PSTR("Motor not fast enough for fastest %l compress time"), maxTimeToGetToOtherMagnet);
        motorMaxSpeedDetected = x;
      }

      info(PSTR("Calibrated speed %d=%l millis"), x, runTime);

      if (runTime > minTimeToGetToOtherMagnet)
      {
        info(PSTR("Motor range is %d to %d for %l to %l compress times"), motorMinSpeedDetected, motorMaxSpeedDetected, minTimeToGetToOtherMagnet, maxTimeToGetToOtherMagnet);
        return;
      }
      motorMinSpeedDetected = x;
    }

    info(PSTR("Motor not slow enough for lowest %l compress time"), minTimeToGetToOtherMagnet);
  }
}


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
  //  pinMode(BLDC_DIR, OUTPUT);

  // Setup the motor output
  pinMode(HOME_SENSOR, INPUT_PULLUP); // Short to ground to trigger

  // Setup the home sensor as an interrupt
  attachInterrupt(digitalPinToInterrupt(HOME_SENSOR), homeTriggered, FALLING);

  hwSerial.begin(SERIAL_BAUD);
  respond('I', PSTR("VISP Core,%d,%d,%d"), VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);

  busDeviceInit();
  vispInit();

  mystepper.setAcceleration(2000);
  mystepper.setMaxSpeed(STEPPER_STEPS_PER_REV * 3);

  i2cBus1->begin();
  i2cBus1->setClock(400000); // Typical

  // Some reset conditions do not reset our globals.
  sensorsFound = false;
  debug = DEBUG_DISABLED;
  sanitizeVispData(); // Apply defaults

  homeThisPuppy(true);

  for (unsigned long timeout = millis() + 6000; millis() < timeout && !motorFound; )
    motorRunStepper();

  motorStop(); /* sensor detached, motor unplugged, etc */
  info(PSTR("motor state = %S"), motorFound ? PSTR("Found") : PSTR("Missing"));
  calibrateMotorSpeeds();

  // Start the VISP calibration process  
  calibrateClear();
}






void loop() {
  static float P[4], T[4];

  motorRunStepper();

  for (t_t *entry = tasks; entry->cbk; entry++)
    tCheck(entry);

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
      if (calibrateInProgress())
          calibrateSensors(P);
      else
      {
          calibrateApply(P);

          if (visp_eeprom.bodyType == 'P')
            calculatePitotValues(P);
          else
            calculateVenturiValues(P);
      }

      // TidalVolume is the same for both versions
      calculateTidalVolume();      

      // Take some time to write to the serial port
      dataSend(P);
    }
  }
#ifdef NEWISH
  if (timeToCheckMotors && sensorsFound)
  {
    switch (currentMode == MODE_PCCMV)
    {
      case PC_CMV:
        if (pressure < visp_eeprom.pressure)
          motorSpeedUp();
        if (pressure < visp_eeprom.pressure)
          motorSpeedDown();
        break;
      case VC_CMV:
        if (volume < visp_eeprom.volume)
          motorSpeedUp();
        if (volume < visp_eeprom.volume)
          motorSpeedDown();
        break;
    }
  }
#endif
  // Command parser uses 6530 bytes of flash... This is a LOT
  // Handle user input, 1 character at a time
  while (hwSerial.available())
    commandParser(hwSerial.read());
}
