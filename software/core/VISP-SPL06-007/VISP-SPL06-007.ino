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

// TODO: Better input validation
// TODO: Failure modes (all of them) need to be accounted for and baked into the system
// TODO: design a board that has TEENSY/NANO/MapleLeaf sockets with a missing pulse detection alarm circuit and integrated motor drivers for steppers and DC motors
// TODO: Get a (very tiny code wise) I2C LCD working for basic text output.

#include "config.h"

#ifdef ARDUINO_TEENSY40
TwoWire *i2cBus1 = &Wire1;
TwoWire *i2cBus2 = &Wire2;
#elif ARDUINO_BLUEPILL_F103C8
TwoWire *i2cBus1 = &Wire;
TwoWire Wire2(PB11, PB10);
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


uint8_t currentMode = MODE_OFF;
debugState_e debug = DEBUG_DISABLED;




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
//
// Put a transistor inverter on ENABLE_PIN_BUS_A to eliminate needing the second ENABLE_PIN_BUS_B
//

/*** Timer callback subsystem ***/

typedef void (*tCBK)() ;
typedef struct t  {
  unsigned long tStart;
  unsigned int  tTimeout; // 64 second max timeout
  tCBK cbk;
} t_t;


unsigned long tCheck (struct t * t ) {
  unsigned long accrued = 0L;

  if (millis() > t->tStart + t->tTimeout)
  {
    unsigned long startTime = micros();
    t->cbk();
    t->tStart = millis();
    accrued += (micros() - startTime);
  }
  return accrued;
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
  // Read them all NOW
  if (sensorsFound)
  {
    // If any of the sensors fail, stop trying to do others
    for (int8_t x = 0; x < 4; x++)
    {
      if (!sensors[x].calculate(&sensors[x]))
        return;
    }
  }
  // OK, the cable might have just been unplugged, and the sensors have gone away.
  // Hence the double checks one above, and this one below
  if (sensorsFound)
  {
    if (calibrateInProgress())
      calibrateSensors();
    else
    {
      calibrateApply();

      if (visp_eeprom.bodyType == 'P')
        calculatePitotValues();
      else
        calculateVenturiValues();
      // TidalVolume is the same for both versions
      calculateTidalVolume();
      // Take some time to write to the serial port
      dataSend();
    }
  }
}
//Sketch uses 29394 bytes (95 % ) of program storage space. Maximum is 30720 bytes.
//Global variables use 1264 bytes (61 % ) of dynamic memory, leaving 784 bytes for local variables. Maximum is 2048 bytes.


unsigned long timeToInhale = 0;
unsigned long timeToStopInhale = -1;
bool isInInhaleCycle = false;

#define isInInhaleCycle() (timeToStopInhale < 0)

void timeToCheckPatient()
{
  unsigned long theMillis = millis();
  // breathRate is in breaths per minute. timeout= 60*1000/bpm
  // breatRation is a 1:X where 1=inhale, and X=exhale.  So a 1:2 is 50% inhaling and 50% exhaling

  if (currentMode == MODE_OFF)
    return;

  // The patenti hasn't tried to breath on their own...
  if (theMillis > timeToInhale)
  {
    unsigned long nextBreathCycle = ((60.0 / (float)visp_eeprom.breath_rate) * 1000.0);
    timeToInhale = nextBreathCycle;
    timeToStopInhale = (nextBreathCycle / visp_eeprom.breath_ratio);

    info(PSTR("brate=%d  bratio=%d nextBreathCycle=%l timeToStopInhale=%l millis"), visp_eeprom.breath_rate, visp_eeprom.breath_ratio, nextBreathCycle, timeToStopInhale);

    timeToInhale += theMillis;
    timeToStopInhale += theMillis;

    motorReverseDirection();
    motorSpeedUp();
  }

  if (timeToStopInhale >= 0 && theMillis > timeToStopInhale)
  {
    motorStop();
    motorGoHome();
    timeToStopInhale = -1;
  }

  // TODO: if in the middle of the inhalation time, and we don't have any pressure from the VISP,
  // TODO: either we have a motor fault or we have a disconnected tube

  if (isInInhaleCycle() && sensorsFound)
  {
    switch (currentMode)
    {
      case MODE_MANUAL_PCCMV:
      case MODE_PCCMV:
        if (pressure < visp_eeprom.breath_pressure)
          motorSpeedUp();
        if (pressure > visp_eeprom.breath_pressure)
          motorSlowDown();
        break;
      case MODE_MANUAL_VCCMV:
      case MODE_VCCMV:
        if (volume < visp_eeprom.breath_volume)
          motorSpeedUp();
        if (volume > visp_eeprom.breath_volume)
          motorSlowDown();
        break;
    }
  }
}

// NANO uses NPN switches to enable/disable a bus for DUAL_I2C with a single hardware I2C bus
void enableI2cBusA(busDevice_t *busDevice, bool enableFlag)
{
#ifdef ENABLE_PIN_BUS_A
  digitalWrite(ENABLE_PIN_BUS_A, (enableFlag == true ? HIGH : LOW));
#endif
}
void enableI2cBusB(busDevice_t *busDevice, bool enableFlag)
{
#ifdef ENABLE_PIN_BUS_B
  digitalWrite(ENABLE_PIN_BUS_B, (enableFlag == true ? HIGH : LOW));
#endif
}

void timeToCheckSensors()
{
  // If debug is on, and a VISP is NOT connected, we flood the system with sensor scans.
  // Do it every half second (or longer)
  if (!sensorsFound)
  {
    detectVISP(i2cBus1, i2cBus2, enableI2cBusA, enableI2cBusB);
    if (sensorsFound)
      displaySetup(); // Need to setup the VISP I2C OLED that just attached
  }

  // Save some flash code space and do this every half second in this function
  displayUpdate();
}

// Scales the analog input to a range.
static int scaleAnalog(int analogIn, int minValue, int maxValue)
{
  float percentage = (float)analogIn / (float)MAX_ANALOG; // This is CPU dependent, 1024 on Nano, 4096 on STM32
  return minValue + (maxValue * percentage);
}

// 344 bytes
void timeToCheckADC()
{
  int analogMode = scaleAnalog(analogRead(ADC_MODE), 0, 3);

  // if analogMode==0 then 100% software control from the rPi
  if (analogMode)
  {
    currentMode = (analogMode == 1 ? MODE_MANUAL_PCCMV : MODE_MANUAL_VCCMV);
    visp_eeprom.breath_pressure = scaleAnalog(analogRead(ADC_PRESSURE), MIN_BREATH_PRESSURE, MAX_BREATH_PRESSURE);
    visp_eeprom.breath_volume = scaleAnalog(analogRead(ADC_VOLUME), MIN_BREATH_VOLUME, MAX_BREATH_VOLUME);
    visp_eeprom.breath_rate = scaleAnalog(analogRead(ADC_RATE), MIN_BREATH_RATE, MAX_BREATH_RATE);
    visp_eeprom.breath_ratio = scaleAnalog(analogRead(ADC_RATIO), MIN_BREATH_RATIO, MAX_BREATH_RATIO);
  }
}

void timeToSendHealthStatus()
{
  sendCurrentSystemHealth();
}


// Timer Driven Tasks and their Schedules.
// These are checked and executed in order.
// If something takes priority over another task, put it at the top of the list
t tasks[] = {
  {0, 20, timeToReadVISP},
  {0, 50,  timeToCheckPatient},
  {0, 100, timeToPulseWatchdog},
  //  {0, 200, timeToCheckADC}, // disabled for now
  {0, 500, timeToCheckSensors},
  {0, 3000, timeToSendHealthStatus},
  {0, 0, NULL} // End of list
};

/*** End of timer callback subsystem ***/



void homeTriggered() // IRQ function
{
  motorFound = true;

  motorStop();
}


void setup() {
  // Address select lines for Dual I2C switching using NPN Transistors
#ifdef ENABLE_PIN_BUS_A
  pinMode(ENABLE_PIN_BUS_A, OUTPUT);
  digitalWrite(ENABLE_PIN_BUS_A, LOW);
#endif
#ifdef ENABLE_PIN_BUS_B
  pinMode(ENABLE_PIN_BUS_B, OUTPUT);
  digitalWrite(ENABLE_PIN_BUS_B, LOW);
#endif
  pinMode(MISSING_PULSE_PIN, OUTPUT);
  digitalWrite(MISSING_PULSE_PIN, LOW);

  if (i2cBus1)
    i2cBus1->begin();

  if (i2cBus2)
    i2cBus2->begin();

  motorSetup();

  // Setup the home sensor as an interrupt
  pinMode(HOME_SENSOR, INPUT_PULLUP); // Short to ground to trigger
  attachInterrupt(digitalPinToInterrupt(HOME_SENSOR), homeTriggered, FALLING);

  hwSerial.begin(SERIAL_BAUD);
  respond('I', PSTR("VISP Core,%d,%d,%d"), VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);

  busDeviceInit();

  vispInit();

  i2cBus1->begin();
  i2cBus1->setClock(400000); // Typical

  // Some reset conditions do not reset our globals.
  sensorsFound = false;
  sanitizeVispData(); // Apply defaults

  // Start the VISP calibration process
  calibrateClear();

  displaySetup();
}




// Every second, compute how much time we spent working, and report the percentage
unsigned long currentUtilization = 0;
unsigned long utilizationTimeout = 0;

// loop() gets called repeatedly forever, so there is no 'idle' time
// do not compute the time checking for things to be done, only compute the time we do things.
void loop() {
  unsigned long startMicros;

  startMicros = micros();
  motorRun();
  currentUtilization += micros() - startMicros;

  for (t_t *entry = tasks; entry->cbk; entry++)
    currentUtilization += tCheck(entry);

  // Command parser uses >6K bytes of flash... This is a LOT
  // Handle user input, 1 character at a time
  startMicros = micros();
  while (hwSerial.available())
    commandParser(hwSerial.read());
  currentUtilization += micros() - startMicros;

  if (millis() > utilizationTimeout)
  {
    debug(PSTR("Utilization %l%%"), currentUtilization / 10000);
    currentUtilization = 0;
    utilizationTimeout = millis() + 1000;
  }
}
