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
#include "respond.h"

void printp(const char *data)
{
  while (pgm_read_byte(data) != 0x00)
    hwSerial.print((const char)pgm_read_byte(data++));
}

// Implement a lighter version of sprintf to save a couple KB of flash space.  sprintf is 2.5KB in size
void respond(char command, PGM_P fmt, ...)
{
  char c;
  if (command == 'g' && debug == DEBUG_DISABLED)
    return;

  hwSerial.print(command);
  hwSerial.print(',');
  hwSerial.print(millis());
  hwSerial.print(',');

  //Declare a va_list macro and initialize it with va_start
  va_list va;
  va_start(va, fmt);

  for (PGM_P data = fmt; (c = pgm_read_byte(data)); data++)
  {
    if (c != '%')
      hwSerial.print(c);
    else
    {
      data++;
      c = pgm_read_byte(data);
      // I don't know why, but the switch statement was not working for me
      if (c == '%') {
        hwSerial.print(c);
      } else if (c == 'c') {
        const char ch = va_arg(va, int);
        hwSerial.print(ch);
      } else if (c == 'd') {
        const int d = va_arg(va, int);
        hwSerial.print(d);
      } else if (c == 'l') {
        const long l = va_arg(va, long);
        hwSerial.print(l);
      } else if (c == 'x') {
        const int x = va_arg(va, int);
        hwSerial.print(x, HEX);
      } else if (c == 'S') {
        const char *S = va_arg(va, const char *);
        printp(S);
      } else if (c == 's') {
        char *s = va_arg(va, char *);
        hwSerial.print(s);
      } else if (c == 'f') {
        double f = va_arg(va, double);
        hwSerial.print(f, 2);
      } else if (c == 0) {
        va_end(va);
        return;
      }
    }
  }

  va_end(va);
  hwSerial.println();
}
