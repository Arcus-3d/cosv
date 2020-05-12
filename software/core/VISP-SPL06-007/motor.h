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

#ifndef __MOTOR_H__
#define __MOTOR_H__

extern volatile bool motorFound;

void motorSetup(); // call in setup()
void motorGoHome(); 
bool motorDetectionInProgress(); // calibration cannot happen while we are running motors for detection


typedef void (*motorFunction)();

extern motorFunction motorSpeedUp;
extern motorFunction motorSlowDown;
extern motorFunction motorStop;
extern motorFunction motorReverseDirection;

extern motorFunction motorRun; // call in loop()


extern int8_t motorType;
#define MOTOR_UNKNOWN -1
#define MOTOR_BLDC     1
#define MOTOR_STEPPER  2
#define MOTOR_WIPER    3
 
#endif
