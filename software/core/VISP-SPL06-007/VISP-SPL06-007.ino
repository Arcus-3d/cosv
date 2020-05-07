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




//The Arduino has 3 timers and 6 PWM output pins. The relation between timers and PWM outputs is:
//
//    Pins 5 and 6: controlled by Timer0
//    Pins 9 and 10: controlled by Timer1
//    Pins 11 and 3: controlled by Timer2
//
// We should rebalance the PWM's to give Timer1 or Timer2 to AccelStepper.

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





void handleSensorFailure();


uint8_t calibrationSampleCounter = 0;





runState_e runState = RUNSTATE_CALIBRATE;












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

int16_t modeBreathRateToMotorSpeed()
{
//  if (visp_eeprom.mode == MODE_PCCMV)

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

  if (visp_eeprom.mode == MODE_OFF)
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


void clearCalibrationData()
{
  runState = RUNSTATE_CALIBRATE;
  calibrationSampleCounter = 0;
  memset(&visp_eeprom.calibrationOffsets, 0, sizeof(visp_eeprom.calibrationOffsets));
}


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
  visp_eeprom.debug = DEBUG_DISABLED;
  clearCalibrationData();
  sanitizeVispData(); // Apply defaults

  homeThisPuppy(true);

  for (unsigned long timeout = millis() + 6000; millis() < timeout && !motorFound; )
    motorRunStepper();

  motorStop(); /* sensor detached, motor unplugged, etc */
  info(PSTR("motor state = %S"), motorFound ? PSTR("Found") : PSTR("Missing"));
  calibrateMotorSpeeds();
}

void dataSend(unsigned long sampleTime, float pressure, float volumeSmoothed, float tidalVolume, float * P)
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
  hwSerial.print(tidalVolume, 4);
  if (visp_eeprom.debug == DEBUG_DISABLED)
  {
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U5], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U6], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U7], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U8], 1);
  }
  hwSerial.println();
}



void doCalibration(float * P)
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

void loopPitotVersion(float * P, float * T)
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
  float volume, inletPressure, outletPressure, throatPressure, ambientPressure, pressure;
  unsigned long sampleTime = millis();

  switch (runState)
  {
    case RUNSTATE_CALIBRATE:
      doCalibration(P);
      break;

    case RUNSTATE_RUN:
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
      // patientPressure = P[VENTURI_SENSOR];   // This is not used?
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
      if (visp_eeprom.bodyType == 'P')
        loopPitotVersion(P, T);
      else
        loopVenturiVersion(P, T);
    }
  }

  // Command parser uses 6530 bytes of flash... This is a LOT
  // Handle user input, 1 character at a time
  while (hwSerial.available())
    commandParser(hwSerial.read());
}
