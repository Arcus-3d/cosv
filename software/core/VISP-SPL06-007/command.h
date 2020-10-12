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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

bool coreLoadSettings();
bool coreSaveSettings();
void coreSaveSettingsStateMachine();

void commandParser(int cmdByte);
void sendCurrentSystemHealth();
void dataSend();

char *currentModeStr(char *buff, int buffSize); // Used by the displayUpdate()

void primeTheFrontEnd();
void updateMotorSpeed();

#define RESPOND_ALL           0xFFFFFFFFL
#define RESPOND_LIMITS             1UL<<0 // Special
#define SAVE_THIS                  1UL<<1 // Special
#define EXPERT                     1UL<<2 // Special
#define RESPOND_DEBUG              1UL<<3
#define RESPOND_MODE               1UL<<4
#define RESPOND_BREATH_VOLUME      1UL<<5
#define RESPOND_BREATH_PRESSURE    1UL<<6
#define RESPOND_BREATH_RATE        1UL<<7
#define RESPOND_BREATH_RATIO       1UL<<8
#define RESPOND_BREATH_THRESHOLD   1UL<<9
#define RESPOND_BODYTYPE           1UL<<10

#define RESPOND_BATTERY            1UL<<11
#define RESPOND_CALIB0             1UL<<12
#define RESPOND_CALIB1             1UL<<13
#define RESPOND_CALIB2             1UL<<14
#define RESPOND_CALIB3             1UL<<15
#define RESPOND_SENSOR0            1UL<<16
#define RESPOND_SENSOR1            1UL<<17
#define RESPOND_SENSOR2            1UL<<18
#define RESPOND_SENSOR3            1UL<<19

#define RESPOND_MOTOR_TYPE          1UL<<20
#define RESPOND_MOTOR_SPEED         1UL<<21
#define RESPOND_MOTOR_MIN_SPEED     1UL<<22
#define RESPOND_MOTOR_HOMING_SPEED  1UL<<23
#define RESPOND_MOTOR_STEPS_PER_REV 1UL<<24

#define RESPOND_FI02                1UL<<25

void respondAppropriately(uint32_t flags);

#endif
