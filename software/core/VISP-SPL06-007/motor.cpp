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

#define STEPPER_STEPS_PER_REV 200
#define SWEEP_SPEED 150 // Ford F150 motor wiper
#define HOMING_SPEED 255

// ok, we can have a simple H bridge  HiLetGo BTS7960
#define MOTOR_HBRIDGE_R_EN    MOTOR_PIN_B   // R_EN: forware drive enable input, high-level enable, low level off  (ACTIVE_HIGH)
#define MOTOR_HBRIDGE_L_EN    MOTOR_PIN_C   // L_EN: Reverse drive enable input, high-level enable, low level off  (ACTIVE HIGH)
#define MOTOR_HBRIDGE_PWM     MOTOR_PIN_PWM // PWM: PWM signal, active high, attach to BOTH LPWM and RPWM
// WARNING: If you enable R_EN and L_EN at the same time, you fry the chip, so always set both to 0 first, then enable delay(1) and then set the direction pin

// We can have a stepper motor - Schmalz easy driver (Does not fry H if used mistakenly)
#define MOTOR_STEPPER_ENABLE  MOTOR_PIN_A  //  enable input, low-level enable, high level off   (ACTIVE LOW)
#define MOTOR_STEPPER_DIR     MOTOR_PIN_C
#define MOTOR_STEPPER_STEP    MOTOR_PIN_PWM

// Same pins as the stepper.   Except that when watching for the FEEDBACK, the pin is high, disabling the stepper
#define MOTOR_BLDC_FEEDBACK   MOTOR_PIN_A  // Must be IRQ capable
#define MOTOR_BLDC_DIR        MOTOR_PIN_C
#define MOTOR_BLDC_PWM        MOTOR_PIN_PWM

AccelStepper mystepper(AccelStepper::DRIVER, MOTOR_STEPPER_STEP, MOTOR_STEPPER_DIR); // Direction: High means forward.  setEnablePin(MOTOR_STEPPER_ENABLE)


volatile bool motorFound = false;

static bool motorWasGoingForward = false;
static uint8_t motorSpeed = 0;
static int8_t motorState = 0;

void hbridgeReverseDirection()
{
  motorWasGoingForward = !motorWasGoingForward;

  // Stop the motor
  analogWrite(MOTOR_HBRIDGE_PWM, 0);

  // If it was movong, git it a bit to actually stop, so we don't fry the controlling chip
  if (motorSpeed)
    delay(10);

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

void hbridgeStop()
{
  digitalWrite(MOTOR_HBRIDGE_L_EN, 0);
  digitalWrite(MOTOR_HBRIDGE_R_EN, 0);

  motorSpeed = 0;
  analogWrite(MOTOR_HBRIDGE_PWM, motorSpeed);
}

void hbridgeSpeedUp()
{
  if (motorSpeed < 255)
  {
    motorSpeed++;
    analogWrite(MOTOR_HBRIDGE_PWM, motorSpeed);
  }
}

void hbridgeSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
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

void bldcSpeedUp()
{
  if (motorSpeed < 255)
  {
    motorSpeed++;
    analogWrite(MOTOR_BLDC_PWM, motorSpeed);
  }
}

void bldcSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    analogWrite(MOTOR_BLDC_PWM, motorSpeed);
  }
}

unsigned long bldcCount=0;

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
  mystepper.stop(); // Stop as fast as possible: sets new target
  motorSpeed = 0;
  mystepper.setSpeed(motorSpeed);
}

void stepperRun()
{
  mystepper.runSpeed();
}

void stepperSpeedUp()
{
  if (motorSpeed < 255)
  {
    motorSpeed++;
    analogWrite(MOTOR_HBRIDGE_PWM, (motorWasGoingForward ? motorSpeed : -motorSpeed));
  }
}

void stepperSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    analogWrite(MOTOR_HBRIDGE_PWM, (motorWasGoingForward ? motorSpeed : -motorSpeed));
  }
}


// Default no motor function callbacks
static void doNothing()
{
    /* This is what is called when the motor has not been identified */
}




void motorGoHome()
{
  if (digitalRead(HOME_SENSOR) == HIGH)
  {
    motorSpeed = HOMING_SPEED;
    motorReverseDirection();
  }
}

bool motorDetectionInProgress()
{
  return motorState>0;
}


// Will get called repeatedly
void motorDetect()
{
  static unsigned long timeout = 0;
  switch (motorState)
  {
    case -1:
      return;
    case 0: // Initialize
      digitalWrite(MOTOR_HBRIDGE_R_EN, LOW);
      digitalWrite(MOTOR_HBRIDGE_L_EN, LOW);
      digitalWrite(MOTOR_STEPPER_ENABLE, HIGH);
      if (!calibrateInProgress())
        motorState++;
      break;
    case 1: // Get the bldc motor running (MUST BE DONE BEFORE STEPPER)
      info(PSTR("Detecting BLDC motor"));
      motorFound = false;
      bldcCount = 0;
      pinMode(MOTOR_BLDC_FEEDBACK, INPUT_PULLUP); // Short to ground to trigger.  This is also STEPPER_ENABLE output if the stepper is connected
      attachInterrupt(digitalPinToInterrupt(MOTOR_BLDC_FEEDBACK), bldcTriggered, FALLING);
      motorSpeed=255;
      bldcReverseDirection(); // Get your motor running...
      timeout = millis() + 2500; // 2.5 seconds of waiting
      motorState++;
      break;
    case 2:
      if (motorFound && bldcCount>0)
      {
        motorSpeedUp = bldcSpeedUp;
        motorSlowDown = bldcSlowDown;
        motorReverseDirection = bldcReverseDirection;
        motorStop = bldcStop;
        motorRun = doNothing;
        motorState = -1; // YEA! Motor has been found!
        info(PSTR("Detected BLDC Motor (%s %d)"), (motorFound?"Home":""),bldcCount);
        return;
      }
      // Tis a crying shame, must be a wiper motor
      if (millis() > timeout)
      {
        bldcStop();
        detachInterrupt(digitalPinToInterrupt(MOTOR_BLDC_FEEDBACK));
        pinMode(MOTOR_BLDC_FEEDBACK, OUTPUT);
        digitalWrite(MOTOR_BLDC_FEEDBACK, LOW);
        motorState++;
      }
      break;

    case 3: // Get the stepper motor running
      info(PSTR("Detecting Stepper motor"));
      motorFound = false;
      mystepper.setSpeed(SWEEP_SPEED);
      timeout = millis() + 2500; // 2.5 seconds of waiting
      motorState++;
      break;
    case 4:
      if (motorFound)
      {
        motorSpeedUp = stepperSpeedUp;
        motorSlowDown = stepperSlowDown;
        motorReverseDirection = stepperReverseDirection;
        motorStop = stepperStop;
        motorRun = stepperRun;
        motorState = -1; // YEA! Motor has been found!
        info(PSTR("Detected Stepper Motor"));
        return;
      }
      // Tis a crying shame, must be a wiper motor
      if (millis() > timeout)
      {
        mystepper.stop();
        motorState++;
      }
      break;

    case 5: // Detect wiper motor
      info(PSTR("Detecting Wiper motor"));
      motorFound = false;
      motorSpeed = SWEEP_SPEED; // As fast as she can go!
      hbridgeReverseDirection();
      timeout = millis() + 2500; // 2.5 seconds of waiting
      motorState++;
      break;
    case 6:
      if (motorFound)
      {
        motorSpeedUp = hbridgeSpeedUp;
        motorSlowDown = hbridgeSlowDown;
        motorReverseDirection = hbridgeReverseDirection;
        motorStop = hbridgeStop;
        motorRun = doNothing; // HBRIDGE does not need to be told to step
        motorState = -1; // YEA! It's found!
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
    case 7:
      timeout = millis() + 2500; // Wait 2.5 seconds before trying again (10-second motor detection cycle)
      motorState++;
      critical(PSTR("Motor not detected"));
      break;
    case 8: // When timed out, go back to state 0 and retry
      if (millis() > timeout)
        motorState = 0;
      break;
  }
}


motorFunction motorSpeedUp = doNothing;
motorFunction motorSlowDown = doNothing;
motorFunction motorReverseDirection = doNothing;
motorFunction motorStop = doNothing;
motorFunction motorRun = motorDetect;

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
  mystepper.setMaxSpeed(STEPPER_STEPS_PER_REV * 3);

  motorSpeedUp = doNothing;
  motorSlowDown = doNothing;
  motorReverseDirection = doNothing;
  motorStop = doNothing;

  motorRun = motorDetect; // Default to detection

}
