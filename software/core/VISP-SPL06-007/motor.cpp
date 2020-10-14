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


// motorDetectionState's
#define DO_NOTHING     0
#define DETECT_START   1
#define DETECT_HBRIDGE 2
#define WAIT_HBRIDGE   3
#define DETECT_STEPPER 4
#define WAIT_STEPPER   5
#define DETECT_TIMEOUT 6
#define WAIT_TIMEOUT   7
static int8_t motorDetectionState = DO_NOTHING;

int8_t motorRunState = MOTOR_STOPPED;

volatile bool motorFound = false;
volatile bool homeHasBeenTriggered = false;
volatile uint32_t lastHomeTime = 0;

static bool motorWasGoingForward = false;

// Defaults for Daren's hardware
int8_t motorSpeed = 0; // 0->100 as a percentage
int8_t motorType = MOTOR_HBRIDGE;
int8_t motorHomingSpeed = 15; // 0->100 as a percentage
int8_t motorMinSpeed = 11; // 0->100 as a percentage
int16_t motorStepsPerRev = 200;
int16_t STEPPER_MAX_SPEED = motorStepsPerRev*3;

volatile unsigned long encoderCount = 0;

void encoderTriggered() // IRQ function (Future finding the perfect home)
{
  encoderCount++;
}

void homeTriggered() // IRQ function
{
  uint32_t currentTime = millis();
  if ( lastHomeTime > currentTime )
  {
    lastHomeTime = currentTime;
  }
  if ( (currentTime - lastHomeTime) > 250)
  {
    homeHasBeenTriggered = true;
  }
  lastHomeTime = currentTime;
}

void __NOINLINE hbridgeGo()
{
  if (motorWasGoingForward)
  {
    digitalWrite(MOTOR_HBRIDGE_L_EN, 0); // Set thes in opposite order of hbridgeReverse() so we don't have both pins active at the same time
    digitalWrite(MOTOR_HBRIDGE_R_EN, 1); // Disables Stepper if mistakenly connected and we are forced int HBRIDGE mode
  }
  else
  {
    digitalWrite(MOTOR_HBRIDGE_R_EN, 0);
    digitalWrite(MOTOR_HBRIDGE_L_EN, 1); // Set these in opposite order of hbridgeReverse() so we don't have both pins active at the same time
  }

  analogWrite(MOTOR_HBRIDGE_PWM, scaleAnalog(motorSpeed, 0, MAX_PWM));
  updateMotorSpeed();
}


void __NOINLINE hbridgeReverseDirection()
{
  motorWasGoingForward = !motorWasGoingForward;

  // Stop the motor
  analogWrite(MOTOR_HBRIDGE_PWM, 0);

  // If it was movong, give it a bit to actually stop, so we don't fry the controlling chip
  if (motorSpeed)
    delay(10);
  info(PSTR("Motor Reversing Direction (Going home)"));
  motorRunState = MOTOR_HOMING;
  hbridgeGo();
}

void __NOINLINE hbridgeStop()
{
  analogWrite(MOTOR_HBRIDGE_PWM, 0);
  digitalWrite(MOTOR_HBRIDGE_L_EN, 0);
  digitalWrite(MOTOR_HBRIDGE_R_EN, 0);

  motorSpeed = 0;
  motorRunState = MOTOR_STOPPED;
  updateMotorSpeed();
}

void __NOINLINE hbridgeSpeedUp()
{
  if (motorSpeed < 100)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    motorRunState = MOTOR_RUNNING;
    hbridgeGo();
  }
}

void __NOINLINE hbridgeSlowDown()
{
  if (motorSpeed > motorMinSpeed)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    motorRunState = MOTOR_RUNNING;
    hbridgeGo();
  }
}





void __NOINLINE stepperGo()
{
  int theSpeed = scaleAnalog(motorSpeed, 0, STEPPER_MAX_SPEED);
  stepper_setSpeed((motorWasGoingForward ? theSpeed : -theSpeed));
  updateMotorSpeed();
}

void __NOINLINE stepperReverseDirection()
{
  motorWasGoingForward = !motorWasGoingForward;
  stepperGo();
}

void __NOINLINE stepperStop()
{
  stepper_setSpeed(0);
  stepper_stop(); // Stop as fast as possible: sets new target (not runSpeed)
  motorSpeed = 0;
  motorRunState = MOTOR_STOPPED;
  updateMotorSpeed();
}

void __NOINLINE stepperRun()
{
  motorRunState = MOTOR_HOMING;
  stepper_runSpeed();
}


void __NOINLINE stepperSpeedUp()
{
  if (motorSpeed <= 100)
  {
    motorSpeed++;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    motorRunState = MOTOR_RUNNING;
    stepperGo();
  }
}

void __NOINLINE stepperSlowDown()
{
  if (motorSpeed > 0)
  {
    motorSpeed--;
    if (motorSpeed < motorMinSpeed)
      motorSpeed = motorMinSpeed;
    motorRunState = MOTOR_RUNNING;
    stepperGo();
  }
}


void __NOINLINE motorGoHomeReal()
{
  if (digitalRead(HOME_SENSOR) == HIGH)
  {
    motorSpeed = motorHomingSpeed;
    motorRunState = MOTOR_HOMING;
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
  return motorDetectionState >= DETECT_START;
}

// Will get called repeatedly
void motorDetect()
{
  static unsigned long timeout = 0;

  // Stepper enable is same as R_EN.   When that pin in LOW, stepper is enabled.   When HIGH, the HBRIDGE is activated.
  // To identify a stepper, set R_EN to LOW, and L_EN to LOW, and pulse the PWM line.  If we see activity on the encoder, we have a stepper attached.
  // Otherwise, assume HBRIDGE.
  // NOTE: We run the stepper in CCW mode (speed is negative) as tat pushes the DIR pin to LOW, so we do not activate the HBridge

  
  switch (motorDetectionState)
  {
    case DO_NOTHING:
      return;
    case DETECT_START: // Initialize
      motorType = MOTOR_UNKNOWN;
      digitalWrite(MOTOR_HBRIDGE_R_EN, HIGH); // Stepper enabled when LOW, so only detect HBridge in one directional context
      digitalWrite(MOTOR_HBRIDGE_L_EN, LOW);
      if (!calibrateInProgress())
        motorDetectionState=DETECT_HBRIDGE;
      break;
    case DETECT_HBRIDGE: // Get the DC motor running
      info(PSTR("Detecting HBRIDGE motor"));
      motorFound = false;
      encoderCount = 0;
      motorSpeed = HBRIDGE_SWEEP_SPEED;
      analogWrite(MOTOR_HBRIDGE_PWM, motorSpeed); // Get your motor running...
      timeout = millis() + 2500; // 2.5 seconds of waiting
      motorDetectionState=WAIT_HBRIDGE;
      break;
    case WAIT_HBRIDGE:
      // motorFound is done by...
      if (motorFound || encoderCount > 2)
      {
        hbridgeStop();
        motorType = MOTOR_HBRIDGE;
        motorSetup();
        motorMinSpeed = HBRIDGE_SWEEP_SPEED;
        motorHomingSpeed = HBRIDGE_SWEEP_SPEED+1;
        info(PSTR("Detected HBRIDGE Motor (%s %d)"), (motorFound ? "Home" : ""), encoderCount);
        return;
      }
      // Tis a crying shame, must be a stepper motor
      if (millis() > timeout)
      {
        hbridgeStop();
        motorDetectionState=DETECT_STEPPER;
      }
      break;
    case DETECT_STEPPER: // Get the stepper motor running
      info(PSTR("Detecting Stepper motor"));
      motorFound = false;
      timeout = millis() + 2500; // 2.5 seconds of waiting
      stepper_enableOutputs();
      stepper_setSpeed(-STEPPER_SWEEP_SPEED); // Run negative (CCW) so that DIR signal is LOW as to not activate L_EN (HBridge = FRY BABY FRY!)
      encoderCount = 0;
      delay(100);
      motorDetectionState=WAIT_STEPPER;
      break;
    case WAIT_STEPPER:
      stepper_runSpeed();
      if (motorFound || encoderCount > 2)
      {
        stepper_stop();
        motorType = MOTOR_STEPPER;
        motorSetup();
        motorMinSpeed = STEPPER_SWEEP_SPEED;
        motorHomingSpeed = STEPPER_SWEEP_SPEED+1;
        info(PSTR("Detected STEPPER Motor (%s %d)"), (motorFound ? "Home" : ""), encoderCount);
        return;
      }
      // Tis a crying shame, must be a H-Bridge motor, or not connected
      if (millis() > timeout)
      {
        stepper_stop();
        stepper_disableOutputs(); // WARNING, Enabled HBRIDGE R_EN as it sets stepper enable to HIGH!!!
        motorDetectionState=DETECT_TIMEOUT;
      }
      break;    

    case DETECT_TIMEOUT:
      timeout = millis() + 2500; // Wait 2.5 seconds before trying again (10-second motor detection cycle)
      motorDetectionState=WAIT_TIMEOUT;
      critical(PSTR("Motor not detected"));
      break;
    case WAIT_TIMEOUT: // When timed out, go back to state 0 and retry
      if (millis() > timeout)
        motorDetectionState = DETECT_START;
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

  // Setup the home sensor as an interrupt
  // NOTE: these may be wired backwards and need to be 'identified' as such
  pinMode(HOME_SENSOR, INPUT_PULLUP); // Short to ground to trigger
  pinMode(MOTOR_ENCODER_FEEDBACK, INPUT_PULLUP); // Short to ground to trigger.
  delay(5); // Give it a chance to be pulled up
  attachInterrupt(digitalPinToInterrupt(MOTOR_ENCODER_FEEDBACK), encoderTriggered, FALLING);
  attachInterrupt(digitalPinToInterrupt(HOME_SENSOR), homeTriggered, FALLING);

  stepper_setAcceleration(2000);
  stepper_setMaxSpeed(STEPPER_MAX_SPEED);
  stepper_setSpeed(0);
  stepper_disableOutputs();

  motorFound = true;
  motorGoHome = motorGoHomeReal;
  switch (motorType)
  {
    case MOTOR_HBRIDGE:
      motorSpeedUp = hbridgeSpeedUp;
      motorSlowDown = hbridgeSlowDown;
      motorReverseDirection = hbridgeReverseDirection;
      motorStop = hbridgeStop;
      motorRun = doNothing; // HBRIDGE does not need to be told to step
      motorDetectionState = DO_NOTHING; // YEA! It's found!
      motorGo = hbridgeGo;
      break;
    case MOTOR_STEPPER:
      motorSpeedUp = stepperSpeedUp;
      motorSlowDown = stepperSlowDown;
      motorReverseDirection = stepperReverseDirection;
      motorStop = stepperStop;
      motorRun = stepperRun;
      motorDetectionState = DO_NOTHING; // YEA! Motor has been found!
      stepper_initialize();
      motorGo = stepperGo;
      break;
    default:
      motorSpeedUp = doNothing;
      motorSlowDown = doNothing;
      motorReverseDirection = doNothing;
      motorStop = doNothing;
      motorGo = doNothing;
      motorGoHome = doNothing;
      motorDetectionState = DETECT_START;
      motorRun = (motorType == MOTOR_AUTODETECT ? motorDetect : doNothing);
      motorFound = false;
      break;
  }

  primeTheFrontEnd(); // Updates all of the buttons...
  sendCurrentSystemHealth();
}
