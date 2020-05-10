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

#ifndef __EEPROM_H__
#define __EEPROM_H__

// We are using the M24C01 128 byte eprom
#define EEPROM_PAGE_SIZE 16


static_assert (sizeof(visp_eeprom_t) == 128, "Size is not correct");

extern visp_eeprom_t visp_eeprom;

bool readEEPROM(busDevice_t *busDev, unsigned short reg, unsigned char *values, uint8_t length);
bool writeEEPROM(busDevice_t *busDev, unsigned short reg, unsigned char *values, char length);

#endif
