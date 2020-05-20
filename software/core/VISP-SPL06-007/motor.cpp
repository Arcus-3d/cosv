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
#include "stepper.h"

// fyi: mystepper adds 3674 bytes of code and 94 bytes of ram, integrating the code saves us ~1K bytes of flash and 45 bytes of ram.
// Flexibility has it's price...
int scaleAnalog(int analogIn, int minValue, int maxValue);


volatile bool motorFound = false;

static bool motorWasGoingForward = false;
int8_t motorSpeed = 0; // 0->100 as a percentage
static int8_t motorState = 0;

int8_t motorType = -1;
int8_t motorHomingSpeed = 75; // 0->100 as a percentage
int8_t motorMinSpeed = 60; // 0->100 as a percentage
int16_t motorStepsPerRev = 200;
int16_t STEPPER_MAX_SPEED = motorStepsPerRev*3;

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
  analogWrite(MOTOR_HBRIDGE_PWM, 0);
}

void hbridgeSpeedUp()
{
  if (motorSpeed < 100)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_HBRIDGE_PWM, scaleAnalog(motorSpeed, 0, MAX_PWM));
  }
}

void hbridgeSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_HBRIDGE_PWM, scaleAnalog(motorSpeed, 0, MAX_PWM));
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
  analogWrite(MOTOR_BLDC_PWM, scaleAnalog(motorSpeed, 0, MAX_PWM));
}

void bldcStop()
{
  motorSpeed = 0;
  analogWrite(MOTOR_BLDC_PWM, 0);
}

void bldcGo()
{
  analogWrite(MOTOR_BLDC_PWM, scaleAnalog(motorSpeed, 0, MAX_PWM));
}

void bldcSpeedUp()
{
  if (motorSpeed <= 100)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_BLDC_PWM, scaleAnalog(motorSpeed, 0, MAX_PWM));
  }
}

void bldcSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    analogWrite(MOTOR_BLDC_PWM, scaleAnalog(motorSpeed, 0, MAX_PWM));
  }
}

unsigned long bldcCount = 0;

void bldcTriggered() // IRQ function (Future finding the perfect home)
{
  bldcCount++;
}



void stepperReverseDirection()
{
  motorWasGoingForward = !motorWasGoingForward;
  stepper_setSpeed((motorWasGoingForward ? motorSpeed : -motorSpeed));
}

void stepperStop()
{
  motorSpeed = 0;
  stepper_setSpeed(0);
  stepper_stop(); // Stop as fast as possible: sets new target (not runSpeed)
}

void stepperRun()
{
  stepper_runSpeed();
}

void stepperGo()
{
  int theSpeed = scaleAnalog(motorSpeed, 0, STEPPER_MAX_SPEED);
  stepper_setSpeed((motorWasGoingForward ? theSpeed : -theSpeed));
}

void stepperSpeedUp()
{
  if (motorSpeed <= 100)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    int theSpeed = scaleAnalog(motorSpeed, 0, STEPPER_MAX_SPEED);
    stepper_setSpeed((motorWasGoingForward ? theSpeed : -theSpeed));
  }
}

void stepperSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    int theSpeed = scaleAnalog(motorSpeed, 0, STEPPER_MAX_SPEED);
    stepper_setSpeed((motorWasGoingForward ? theSpeed : -theSpeed));
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
      stepper_enableOutputs();
      stepper_setSpeed(STEPPER_SWEEP_SPEED);
      delay(100);
      motorState++;
      break;
    case WAIT_STEPPER:
      stepper_runSpeed();
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
        stepper_stop();
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

  stepper_setAcceleration(2000);
  stepper_setMaxSpeed(STEPPER_MAX_SPEED);
  stepper_setSpeed(0);
  stepper_disableOutputs();

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
      motorHomingSpeed = BLDC_SWEEP_SPEED + 1;
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
      motorHomingSpeed = STEPPER_SWEEP_SPEED + 1;
      stepper_initialize();
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
      motorHomingSpeed = WIPER_SWEEP_SPEED + 1;
      motorGo = hbridgeGo;
      break;
    default:
      motorHomingSpeed = 0;
      motorSpeed = 0;
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
