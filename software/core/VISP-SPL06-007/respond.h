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

#include <stdarg.h>

void printp(const char *data);
void respond(char command, PGM_P fmt, ...);

// critical alerts should result in the display indicating such (big red X)
// Think sensor communication loss or motor not detected (VISP not attached to bag while motor detection is happening?)
// We also need a way to clear the error.

#define info(...)     respond('i', __VA_ARGS__)
#define debug(...)    respond('g', __VA_ARGS__)
#define warning(...)  respond('w', __VA_ARGS__)
#define critical(...) respond('c', __VA_ARGS__)
