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
*/

#include "config.h"

// MOTOR_STEPPER_STEP, MOTOR_STEPPER_DIR MOTOR_STEPPER_ENABLE
typedef enum
{
  DIRECTION_CCW = 0,  ///< Counter-Clockwise
  DIRECTION_CW  = 1   ///< Clockwise
} Direction;

Direction _direction; // 1 == CW




/// The current absolution position in steps.
long           _currentPos;    // Steps

/// The target position in steps. The library will move the
/// motor from the _currentPos to the _targetPos, taking into account the
/// max speed, acceleration and deceleration
long           _targetPos;     // Steps

/// The current motos speed in steps per second
/// Positive is clockwise
float          _speed;         // Steps per second

/// The maximum permitted speed in steps per second. Must be > 0.
float          _maxSpeed;

/// The acceleration to use to accelerate or decelerate the motor in steps
/// per second per second. Must be > 0
float          _acceleration;
float          _sqrt_twoa; // Precomputed sqrt(2*_acceleration)

/// The current interval between steps in microseconds.
/// 0 means the motor is currently stopped with _speed == 0
unsigned long  _stepInterval;

/// The last step time in microseconds
unsigned long  _lastStepTime;

/// The minimum allowed pulse width in microseconds
unsigned int   _minPulseWidth;

/// The step counter for speed calculations
long _n;

/// Initial step size in microseconds
float _c0;

/// Last step size in microseconds
float _cn;

/// Min step size in microseconds based on maxSpeed
float _cmin; // at max speed



void stepper_moveTo(long absolute)
{
  if (_targetPos != absolute)
  {
    _targetPos = absolute;
    stepper_computeNewSpeed();
    // compute new n?
  }
}

void stepper_move(long relative)
{
  stepper_moveTo(_currentPos + relative);
}

// Implements steps according to the current step interval
// You must call this at least once per step
// returns true if a step occurred
bool stepper_runSpeed()
{
  // Dont do anything unless we actually have a step interval
  if (!_stepInterval)
    return false;

  unsigned long time = micros();
  if (time - _lastStepTime >= _stepInterval)
  {
    if (_direction == DIRECTION_CW)
    {
      // Clockwise
      _currentPos += 1;
    }
    else
    {
      // Anticlockwise
      _currentPos -= 1;
    }
    stepper_step(_currentPos);

    _lastStepTime = time; // Caution: does not account for costs in step()

    return true;
  }
  else
  {
    return false;
  }
}

long stepper_distanceToGo()
{
  return _targetPos - _currentPos;
}

long stepper_targetPosition()
{
  return _targetPos;
}

long stepper_currentPosition()
{
  return _currentPos;
}

// Useful during initialisations or after initial positioning
// Sets speed to 0
void stepper_setCurrentPosition(long position)
{
  _targetPos = _currentPos = position;
  _n = 0;
  _stepInterval = 0;
  _speed = 0.0;
}

void stepper_computeNewSpeed()
{
  long distanceTo = stepper_distanceToGo(); // +ve is clockwise from curent location

  long stepsToStop = (long)((_speed * _speed) / (2.0 * _acceleration)); // Equation 16

  if (distanceTo == 0 && stepsToStop <= 1)
  {
    // We are at the target and its time to stop
    _stepInterval = 0;
    _speed = 0.0;
    _n = 0;
    return;
  }

  if (distanceTo > 0)
  {
    // We are anticlockwise from the target
    // Need to go clockwise from here, maybe decelerate now
    if (_n > 0)
    {
      // Currently accelerating, need to decel now? Or maybe going the wrong way?
      if ((stepsToStop >= distanceTo) || _direction == DIRECTION_CCW)
        _n = -stepsToStop; // Start deceleration
    }
    else if (_n < 0)
    {
      // Currently decelerating, need to accel again?
      if ((stepsToStop < distanceTo) && _direction == DIRECTION_CW)
        _n = -_n; // Start accceleration
    }
  }
  else if (distanceTo < 0)
  {
    // We are clockwise from the target
    // Need to go anticlockwise from here, maybe decelerate
    if (_n > 0)
    {
      // Currently accelerating, need to decel now? Or maybe going the wrong way?
      if ((stepsToStop >= -distanceTo) || _direction == DIRECTION_CW)
        _n = -stepsToStop; // Start deceleration
    }
    else if (_n < 0)
    {
      // Currently decelerating, need to accel again?
      if ((stepsToStop < -distanceTo) && _direction == DIRECTION_CCW)
        _n = -_n; // Start accceleration
    }
  }

  // Need to accelerate or decelerate
  if (_n == 0)
  {
    // First step from stopped
    _cn = _c0;
    _direction = (distanceTo > 0) ? DIRECTION_CW : DIRECTION_CCW;
  }
  else
  {
    // Subsequent step. Works for accel (n is +_ve) and decel (n is -ve).
    _cn = _cn - ((2.0 * _cn) / ((4.0 * _n) + 1)); // Equation 13
    _cn = max(_cn, _cmin);
  }
  _n++;
  _stepInterval = _cn;
  _speed = 1000000.0 / _cn;
  if (_direction == DIRECTION_CCW)
    _speed = -_speed;
}

// Run the motor to implement speed and acceleration in order to proceed to the target position
// You must call this at least once per step, preferably in your main loop
// If the motor is in the desired position, the cost is very small
// returns true if the motor is still running to the target position.
bool stepper_run()
{
  if (stepper_runSpeed())
    stepper_computeNewSpeed();
  return _speed != 0.0 || stepper_distanceToGo() != 0;
}


void stepper_setMaxSpeed(float speed)
{
  if (speed < 0.0)
    speed = -speed;
  if (_maxSpeed != speed)
  {
    _maxSpeed = speed;
    _cmin = 1000000.0 / speed;
    // Recompute _n from current speed and adjust speed if accelerating or cruising
    if (_n > 0)
    {
      _n = (long)((_speed * _speed) / (2.0 * _acceleration)); // Equation 16
      stepper_computeNewSpeed();
    }
  }
}

float   stepper_maxSpeed()
{
  return _maxSpeed;
}

void stepper_setAcceleration(float acceleration)
{
  if (acceleration == 0.0)
    return;
  if (acceleration < 0.0)
    acceleration = -acceleration;
  if (_acceleration != acceleration)
  {
    // Recompute _n per Equation 17
    _n = _n * (_acceleration / acceleration);
    // New c0 per Equation 7, with correction per Equation 15
    _c0 = 0.676 * sqrt(2.0 / acceleration) * 1000000.0; // Equation 15
    _acceleration = acceleration;
    stepper_computeNewSpeed();
  }
}

void stepper_setSpeed(float speed)
{
  if (speed == _speed)
    return;
  speed = constrain(speed, -_maxSpeed, _maxSpeed);
  if (speed == 0.0)
    _stepInterval = 0;
  else
  {
    _stepInterval = fabs(1000000.0 / speed);
    _direction = (speed > 0.0) ? DIRECTION_CW : DIRECTION_CCW;
  }
  _speed = speed;
}

float stepper_speed()
{
  return _speed;
}


// You might want to override this to implement eg serial output
// bit 0 of the mask corresponds to MOTOR_STEPPER_STEP
// bit 1 of the mask corresponds to MOTOR_STEPPER_DIR
// ....
void stepper_setOutputPins(uint8_t mask)
{
    digitalWrite(MOTOR_STEPPER_STEP, (mask & (1 << 0)) ? HIGH : LOW);
    digitalWrite(MOTOR_STEPPER_DIR, (mask & (1 << 1)) ? HIGH : LOW);
}

// Subclasses can override
void stepper_step(long step)
{
  (void)(step); // Unused

  // MOTOR_STEPPER_STEP is step, MOTOR_STEPPER_DIR is direction
  stepper_setOutputPins(_direction ? 0b10 : 0b00); // Set direction first else get rogue pulses
  stepper_setOutputPins(_direction ? 0b11 : 0b01); // step HIGH
  // Caution 200ns setup time
  // Delay the minimum allowed pulse width
  delayMicroseconds(_minPulseWidth);
  stepper_setOutputPins(_direction ? 0b10 : 0b00); // step LOW
}



void stepper_setMinPulseWidth(unsigned int minWidth)
{
  _minPulseWidth = minWidth;
}


// Blocks until the target position is reached and stopped
void stepper_runToPosition()
{
  while (stepper_run())
    ;
}

bool stepper_runSpeedToPosition()
{
  if (_targetPos == _currentPos)
    return false;
  if (_targetPos > _currentPos)
    _direction = DIRECTION_CW;
  else
    _direction = DIRECTION_CCW;
  return stepper_runSpeed();
}

// Blocks until the new target position is reached
void stepper_runToNewPosition(long position)
{
  stepper_moveTo(position);
  stepper_runToPosition();
}

void stepper_stop()
{
  if (_speed != 0.0)
  {
    long stepsToStop = (long)((_speed * _speed) / (2.0 * _acceleration)) + 1; // Equation 16 (+integer rounding)
    if (_speed > 0)
      stepper_move(stepsToStop);
    else
      stepper_move(-stepsToStop);
  }
}

bool stepper_isRunning()
{
  return !(_speed == 0.0 && _targetPos == _currentPos);
}






// Prevents power consumption on the outputs
void    stepper_disableOutputs()
{
  stepper_setOutputPins(0); // Handles inversion automatically
  pinMode(MOTOR_STEPPER_ENABLE, OUTPUT);
  digitalWrite(MOTOR_STEPPER_ENABLE, HIGH);
}

void    stepper_enableOutputs()
{
  pinMode(MOTOR_STEPPER_STEP, OUTPUT);
  
  pinMode(MOTOR_STEPPER_DIR, OUTPUT);
  digitalWrite(MOTOR_STEPPER_DIR, LOW); // We do this, for motor detection, so we don't fry the HBridge
  
  pinMode(MOTOR_STEPPER_ENABLE, OUTPUT);
  digitalWrite(MOTOR_STEPPER_ENABLE, LOW);
}

// MOTOR_STEPPER_STEP, MOTOR_STEPPER_DIR
void stepper_initialize()
{
  _currentPos = 0;
  _targetPos = 0;
  _speed = 0.0;
  _maxSpeed = 1.0;
  _acceleration = 0.0;
  _sqrt_twoa = 1.0;
  _stepInterval = 0;
  _minPulseWidth = 1;
  _lastStepTime = 0;

  // NEW
  _n = 0;
  _c0 = 0.0;
  _cn = 0.0;
  _cmin = 1.0;
  _direction = DIRECTION_CCW;

  // Some reasonable default
  stepper_setAcceleration(1);
}
