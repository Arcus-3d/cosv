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

bool readEEPROM(busDevice_t *busDev, unsigned short reg, unsigned char *values, uint8_t length)
{
  // loop through the length using EEPROM_PAGE_SIZE intervals
  // The lower level WIRE library has a buffer limitation, so staying in EEPROM_PAGE_SIZE intervals is a good thing
  for (unsigned short x = 0; x < length; x += EEPROM_PAGE_SIZE)
  {
    if (!busReadBuf(busDev, reg + x, &values[x], min(EEPROM_PAGE_SIZE, (length - x))))
      return false;
  }
  return true;
}

bool writeEEPROM(busDevice_t *busDev, unsigned short reg, unsigned char *values, char length)
{
  // loop through the length using EEPROM_PAGE_SIZE intervals
  // Writes must align on page size boundaries (otherwise it wraps to the beginning of the page)
  if (reg % EEPROM_PAGE_SIZE)
    return false;

  for (unsigned short x = 0; x < length; x += EEPROM_PAGE_SIZE)
  {
    if (!busWriteBuf(busDev, reg + x, &values[x], min(EEPROM_PAGE_SIZE, (length - x))))
      return false;
  }
  return true;
}
