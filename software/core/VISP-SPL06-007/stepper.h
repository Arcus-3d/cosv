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
#ifndef __STEPPER_H__
#define __STEPPER_H__
void stepper_moveTo(long absolute);
void stepper_move(long relative);
bool stepper_runSpeed();
long stepper_distanceToGo();
long stepper_targetPosition();
long stepper_currentPosition();
void stepper_setCurrentPosition(long position);
void stepper_computeNewSpeed();
bool stepper_run();
void stepper_setMaxSpeed(float speed);
float stepper_maxSpeed();
void stepper_setAcceleration(float acceleration);
void stepper_setSpeed(float speed);
float stepper_speed();
void stepper_step(long step);
void stepper_setOutputPins(uint8_t mask);
void stepper_setMinPulseWidth(unsigned int minWidth);
void stepper_runToPosition();
bool stepper_runSpeedToPosition();
void stepper_runToNewPosition(long position);
void stepper_stop();
bool stepper_isRunning();


void    stepper_disableOutputs();
void    stepper_enableOutputs();
void stepper_initialize();

#endif
