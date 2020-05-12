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


// fyi: mystepper adds 3674 bytes of code and 94 bytes of ram
#define WIPER_SWEEP_SPEED 150 // Ford F150 motor wiper
#define STEPPER_SWEEP_SPEED 128
#define BLDC_SWEEP_SPEED   128

// ok, we can have a simple H bridge  HiLetGo BTS7960
#define MOTOR_HBRIDGE_R_EN    MOTOR_PIN_B   // R_EN: forware drive enable input, high-level enable, low level off  (ACTIVE_HIGH)
#define MOTOR_HBRIDGE_L_EN    MOTOR_PIN_C   // L_EN: Reverse drive enable input, high-level enable, low level off  (ACTIVE HIGH)
#define MOTOR_HBRIDGE_PWM     MOTOR_PIN_PWM // PWM: PWM signal, active high, attach to BOTH LPWM and RPWM
// WARNING: If you enable R_EN and L_EN at the same time, you fry the chip, so always set both to 0 first, then enable delay(1) and then set the direction pin

// We can have a stepper motor - Schmalz easy driver (Does not fry H if used mistakenly)
#define MOTOR_STEPPER_ENABLE  MOTOR_PIN_A  //  enable input, low-level enable, high level off   (ACTIVE LOW)<-- This is important as BLDC detection pulls this pin HIGH and looks for a pull to zero
#define MOTOR_STEPPER_DIR     MOTOR_PIN_C
#define MOTOR_STEPPER_STEP    MOTOR_PIN_PWM

// Same pins as the stepper.   Except that when watching for the FEEDBACK, the pin is high, disabling the stepper
#define MOTOR_BLDC_FEEDBACK   MOTOR_PIN_A  // Must be IRQ capable
#define MOTOR_BLDC_DIR        MOTOR_PIN_C
#define MOTOR_BLDC_PWM        MOTOR_PIN_PWM

AccelStepper mystepper(AccelStepper::DRIVER, MOTOR_STEPPER_STEP, MOTOR_STEPPER_DIR); // Direction: High means forward.  setEnablePin(MOTOR_STEPPER_ENABLE)


volatile bool motorFound = false;

static bool motorWasGoingForward = false;
int motorSpeed = 0;
static int8_t motorState = 0;

int8_t motorType = -1;
int motorHomingSpeed = 150;
int motorMinSpeed = 150;
int motorStepsPerRev = 200;

void hbridgeGo()
{
  if (motorWasGoingForward)
  {
    digitalWrite(MOTOR_HBRIDGE_L_EN, 0); // Set thes in opposite order of hbridgeReverse() so we don't have both pins active at the same time
    digitalWrite(MOTOR_HBRIDGE_R_EN, 1);
  }
  else
  {
    digitalWrite(MOTOR_HBRIDGE_R_EN, 0);
    digitalWrite(MOTOR_HBRIDGE_L_EN, 1); // Set these in opposite order of hbridgeReverse() so we don't have both pins active at the same time

  }
  analogWrite(MOTOR_HBRIDGE_PWM, motorSpeed);
}


void hbridgeReverseDirection()
{
  motorWasGoingForward = !motorWasGoingForward;

  // Stop the motor
  analogWrite(MOTOR_HBRIDGE_PWM, 0);

  // If it was movong, git it a bit to actually stop, so we don't fry the controlling chip
  if (motorSpeed)
    delay(10);

  hbridgeGo();
}

void hbridgeStop()
{
  digitalWrite(MOTOR_HBRIDGE_L_EN, 0);
  digitalWrite(MOTOR_HBRIDGE_R_EN, 0);

  motorSpeed = 0;
  analogWrite(MOTOR_HBRIDGE_PWM, motorSpeed);
}

void hbridgeSpeedUp()
{
  if (motorSpeed < MAX_PWM)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_HBRIDGE_PWM, motorSpeed);
  }
}

void hbridgeSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_HBRIDGE_PWM, motorSpeed);
  }
}




void bldcReverseDirection()
{
  motorWasGoingForward = !motorWasGoingForward;

  // Stop the motor
  analogWrite(MOTOR_BLDC_PWM, 0);

  // If it was movong, git it a bit to actually stop, so we don't fry the controlling chip
  if (motorSpeed)
    delay(10);

  digitalWrite(MOTOR_BLDC_DIR, (motorWasGoingForward ? LOW : HIGH));
  analogWrite(MOTOR_BLDC_PWM, motorSpeed);
}

void bldcStop()
{
  motorSpeed = 0;
  analogWrite(MOTOR_BLDC_PWM, motorSpeed);
}

void bldcGo()
{
  analogWrite(MOTOR_BLDC_PWM, motorSpeed);
}

void bldcSpeedUp()
{
  if (motorSpeed < MAX_PWM)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_BLDC_PWM, motorSpeed);
  }
}

void bldcSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_BLDC_PWM, motorSpeed);
  }
}

unsigned long bldcCount = 0;

void bldcTriggered() // IRQ function (Future finding the perfect home)
{
  bldcCount++;
}




void stepperReverseDirection()
{
  motorWasGoingForward != motorWasGoingForward;
  mystepper.setSpeed((motorWasGoingForward ? motorSpeed : -motorSpeed));
}

void stepperStop()
{
  motorSpeed = 0;
  mystepper.setSpeed(0);
  mystepper.stop(); // Stop as fast as possible: sets new target (not runSpeed)
}

void stepperRun()
{
  mystepper.runSpeed();
}

void stepperGo()
{
  mystepper.setSpeed((motorWasGoingForward ? motorSpeed : -motorSpeed));
}

void stepperSpeedUp()
{
  if (motorSpeed < MAX_PWM)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    mystepper.setSpeed((motorWasGoingForward ? motorSpeed : -motorSpeed));
  }
}

void stepperSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    mystepper.setSpeed((motorWasGoingForward ? motorSpeed : -motorSpeed));
  }
}


void motorGoHomeReal()
{
  if (digitalRead(HOME_SENSOR) == HIGH)
  {
    motorSpeed = motorHomingSpeed;
    motorReverseDirection();
  }
}

// Default no motor function callbacks
static void doNothing()
{
  /* This is what is called when the motor has not been identified */
}





bool motorDetectionInProgress()
{
  return motorState > 0;
}

#define DETECT_START   0
#define DETECT_BLDC    1
#define WAIT_BLDC      2
#define DETECT_STEPPER 3
#define WAIT_STEPPER   4
#define DETECT_WIPER   5
#define WAIT_WIPER     6
#define DETECT_TIMEOUT 7
#define WAIT_TIMEOUT   8

// Will get called repeatedly
void motorDetect()
{
  static unsigned long timeout = 0;
  switch (motorState)
  {
    case -1:
      return;
    case DETECT_START: // Initialize
      motorType = MOTOR_UNKNOWN;
      digitalWrite(MOTOR_HBRIDGE_R_EN, LOW);
      digitalWrite(MOTOR_HBRIDGE_L_EN, LOW);
      digitalWrite(MOTOR_STEPPER_ENABLE, HIGH);
      if (!calibrateInProgress())
        motorState++;
      break;
    case DETECT_BLDC: // Get the bldc motor running (MUST BE DONE BEFORE STEPPER)
      info(PSTR("Detecting BLDC motor"));
      motorFound = false;
      bldcCount = 0;
      motorSpeed = BLDC_SWEEP_SPEED;
      bldcReverseDirection(); // Get your motor running...
      pinMode(MOTOR_BLDC_FEEDBACK, INPUT_PULLUP); // Short to ground to trigger.  This is also STEPPER_ENABLE output if the stepper is connected
      delay(5); // Give it a chance to be pulled up
      attachInterrupt(digitalPinToInterrupt(MOTOR_BLDC_FEEDBACK), bldcTriggered, FALLING);
      timeout = millis() + 2500; // 2.5 seconds of waiting
      motorState++;
      break;
    case WAIT_BLDC:
      if (motorFound && bldcCount > 2)
      {
        motorType = MOTOR_BLDC;
        motorSetup();
        info(PSTR("Detected BLDC Motor (%s %d)"), (motorFound ? "Home" : ""), bldcCount);
        return;
      }
      // Tis a crying shame, must be a wiper motor
      if (millis() > timeout)
      {
        bldcStop();
        detachInterrupt(digitalPinToInterrupt(MOTOR_BLDC_FEEDBACK));
        pinMode(MOTOR_BLDC_FEEDBACK, OUTPUT);
        motorState++;
      }
      break;

    case DETECT_STEPPER: // Get the stepper motor running
      info(PSTR("Detecting Stepper motor"));
      motorFound = false;
      timeout = millis() + 2500; // 2.5 seconds of waiting
      mystepper.enableOutputs();
      mystepper.setSpeed(STEPPER_SWEEP_SPEED);
      delay(100);
      motorState++;
      break;
    case WAIT_STEPPER:
      mystepper.runSpeed();
      if (motorFound)
      {
        motorType = MOTOR_STEPPER;
        motorSetup();
        info(PSTR("Detected Stepper Motor"));
        return;
      }
      // Tis a crying shame, must be a wiper motor
      if (millis() > timeout)
      {
        mystepper.stop();
        digitalWrite(MOTOR_STEPPER_ENABLE, HIGH); // LOW enable...

        motorState++;
      }
      break;

    case DETECT_WIPER: // Detect wiper motor
      info(PSTR("Detecting Wiper motor"));
      motorFound = false;
      motorSpeed = WIPER_SWEEP_SPEED; // As fast as she can go!
      hbridgeReverseDirection();
      timeout = millis() + 2500; // 2.5 seconds of waiting
      motorState++;
      break;
    case WAIT_WIPER:
      if (motorFound)
      {
        motorType = MOTOR_WIPER;
        motorSetup();
        info(PSTR("Detected Wiper Motor"));
        break;
      }
      // Tis a crying shame, must be a wiper motor
      if (millis() > timeout)
      {
        hbridgeStop();
        motorState++;
      }
      break;
    case DETECT_TIMEOUT:
      timeout = millis() + 2500; // Wait 2.5 seconds before trying again (10-second motor detection cycle)
      motorState++;
      critical(PSTR("Motor not detected"));
      break;
    case WAIT_TIMEOUT: // When timed out, go back to state 0 and retry
      if (millis() > timeout)
        motorState = 0;
      break;
  }
}


motorFunction motorSpeedUp = doNothing;
motorFunction motorSlowDown = doNothing;
motorFunction motorReverseDirection = doNothing;
motorFunction motorStop = doNothing;
motorFunction motorGo = doNothing;
motorFunction motorGoHome = doNothing;
motorFunction motorRun = doNothing;

void motorSetup()
{
  // Setup outputs, etc
  pinMode(MOTOR_PIN_A, OUTPUT);
  pinMode(MOTOR_PIN_B, OUTPUT);
  pinMode(MOTOR_PIN_C, OUTPUT);
  pinMode(MOTOR_PIN_PWM, OUTPUT);
  digitalWrite(MOTOR_PIN_A, LOW);
  digitalWrite(MOTOR_PIN_B, LOW);
  digitalWrite(MOTOR_PIN_C, LOW);
  digitalWrite(MOTOR_PIN_PWM, LOW);

  mystepper.setEnablePin(MOTOR_STEPPER_ENABLE);
  mystepper.setAcceleration(2000);
  mystepper.setMaxSpeed(motorStepsPerRev * 3);
  mystepper.setSpeed(0);
  mystepper.disableOutputs();
  mystepper.setPinsInverted(false, false, true);

  motorFound = true;
  motorGoHome = motorGoHomeReal;
  switch (motorType)
  {
    case MOTOR_BLDC:
      motorSpeedUp = bldcSpeedUp;
      motorSlowDown = bldcSlowDown;
      motorReverseDirection = bldcReverseDirection;
      motorStop = bldcStop;
      motorRun = doNothing;
      motorState = -1; // YEA! Motor has been found!
      motorMinSpeed = BLDC_SWEEP_SPEED;
      motorHomingSpeed = BLDC_SWEEP_SPEED+1;
      motorGo = bldcGo;
      break;
    case MOTOR_STEPPER:
      motorSpeedUp = stepperSpeedUp;
      motorSlowDown = stepperSlowDown;
      motorReverseDirection = stepperReverseDirection;
      motorStop = stepperStop;
      motorRun = stepperRun;
      motorState = -1; // YEA! Motor has been found!
      motorMinSpeed = STEPPER_SWEEP_SPEED;
      motorHomingSpeed = STEPPER_SWEEP_SPEED+1;
      motorGo = stepperGo;
      break;
    case MOTOR_WIPER:
      motorSpeedUp = hbridgeSpeedUp;
      motorSlowDown = hbridgeSlowDown;
      motorReverseDirection = hbridgeReverseDirection;
      motorStop = hbridgeStop;
      motorRun = doNothing; // HBRIDGE does not need to be told to step
      motorState = -1; // YEA! It's found!
      motorMinSpeed = WIPER_SWEEP_SPEED;
      motorHomingSpeed = WIPER_SWEEP_SPEED+1;
      motorGo = hbridgeGo;
      break;
    default:
      motorHomingSpeed = 0;
      motorSpeed=0;
      motorSpeedUp = doNothing;
      motorSlowDown = doNothing;
      motorReverseDirection = doNothing;
      motorStop = doNothing;
      motorGo = doNothing;
      motorGoHome = doNothing;
      motorRun = (motorType == MOTOR_AUTODETECT ? motorDetect : doNothing);
      motorFound = false;
      break;
  }

  motorGoHome();
}
