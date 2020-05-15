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

#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

// One display lives on the VISP, the other on the unit
// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS_VISP 0x3C
#define I2C_ADDRESS_MAIN 0x3D

// Define proper RST_PIN if required.
#define RST_PIN -1

SSD1306AsciiWire oledVISP, oledMain;

void displaySetup()
{
#if RST_PIN >= 0
  oledMain.begin(&Adafruit128x64, I2C_ADDRESS_MAIN, RST_PIN);
  oledVISP.begin(&Adafruit128x32, I2C_ADDRESS_VISP, RST_PIN);
#else // RST_PIN >= 0
  oledMain.begin(&Adafruit128x64, I2C_ADDRESS_MAIN);
  oledVISP.begin(&Adafruit128x32, I2C_ADDRESS_VISP);
#endif // RST_PIN >= 0

  oledMain.setFont(Adafruit5x7);
  oledVISP.setFont(Adafruit5x7);
}

void displayToThis(SSD1306AsciiWire *lcd, bool isVISP)
{
  char modeBuff[16] = {0};

  // 4 lines on a VISP, 8 on a Main
  lcd->setCursor(0,0);
  lcd->print((isVISP ? F("VISP:") : F("Boxy:")));
  lcd->print(currentModeStr(modeBuff, sizeof(modeBuff)));
  lcd->clearToEOL();
  lcd->println();
  
  lcd->print(F("IE 1:"));
  lcd->print(visp_eeprom.breath_ratio);
  lcd->print(F("  Rate "));
  lcd->print(visp_eeprom.breath_rate);
  lcd->clearToEOL();
  lcd->println();
  
  lcd->print(F("Pressure "));
  lcd->print(pressure);
  lcd->print('/');
  lcd->print(visp_eeprom.breath_pressure);
  lcd->clearToEOL();
  lcd->println();
  
  lcd->print(F("Volume   "));
  lcd->print(volume);
  lcd->print('/');
  lcd->print(visp_eeprom.breath_volume);
  lcd->clearToEOL();
  lcd->println();
}

void displayUpdate()
{
  displayToThis(&oledMain, false);
  displayToThis(&oledVISP, true);
}
