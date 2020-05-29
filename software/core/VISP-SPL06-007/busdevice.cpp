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

// Get rid of malloc() and free() issues
busDevice_t devices[DEVICE_MAX];


void busDeviceInit()
{
  memset(&devices, 0, sizeof(devices));
}

// Channel is the channel value for the mux chip
static uint8_t muxSelectChannel(busDevice_t *busDev, uint8_t channel)
{
  if (busDev && channel)
  {
    if (busDev->currentChannel != channel)
    {
      uint8_t error;
      busDev->busdev.i2c.i2cBus->beginTransmission(busDev->busdev.i2c.address);
      busDev->busdev.i2c.i2cBus->write(channel);
      error = busDev->busdev.i2c.i2cBus->endTransmission();
      busDev->currentChannel = channel;
      return error;
    }
    return 0;
  }
  else {
    return 0xff;
  }
}

void busPrint(busDevice_t *bus, const char *function)
{
  if (bus == NULL)
    return;

  if (bus->busType == BUSTYPE_I2C)
  {
    debug(PSTR("%S(I2C: bus=0x%x a=0x%x ch=%d)"), function, bus->busdev.i2c.i2cBus, bus->busdev.i2c.address, bus->busdev.i2c.channel);
    return;
  }
  //  if (bus->busType == BUSTYPE_SPI)
  //  {
  //    debug(PSTR("%S(SPI)"), function);
  //    return;
  //  }
}

void noEnableCbk(busDevice_t *busDevice, bool enableFlag)
{
  /* Do nothing */
}

busDevice_t *busDeviceInitI2C(uint8_t devNum, TwoWire *wire, uint8_t address, uint8_t channel, busDevice_t *channelDev, busDeviceEnableCbk enableCbk, hwType_e hwType)
{
  busDevice_t *dev = &devices[devNum];
  memset(dev, 0, sizeof(busDevice_t));
  dev->busType = BUSTYPE_I2C;
  dev->hwType = hwType;
  dev->enableCbk = enableCbk;
  dev->busdev.i2c.i2cBus = wire;
  dev->busdev.i2c.address = address;
  dev->busdev.i2c.channel = channel; // If non-zero, then it is a channel on a TCA9546 at mux
  dev->busdev.i2c.channelDev = channelDev;
  return dev;
}

busDevice_t *busDeviceInitSPI(uint8_t devNum, SPIClass *spiBus, busDeviceEnableCbk enableCbk, hwType_e hwType)
{
  busDevice_t *dev = &devices[devNum];
  memset(dev, 0, sizeof(busDevice_t));
  dev->busType = BUSTYPE_SPI;
  dev->hwType = hwType;
  dev->enableCbk = enableCbk;
  dev->busdev.spi.spiBus = spiBus;
  return dev;
}

// Simply detect if the device is present on this device assignment
bool busDeviceDetect(busDevice_t *busDev)
{
  int8_t error = 1;
  if (busDev->busType == BUSTYPE_I2C)
  {
    uint8_t address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
    busDev->enableCbk(busDev, true);
    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);
    error = wire->endTransmission();

    busDev->enableCbk(busDev, false);

    //busPrint(busDev, (error ? PSTR("MISSING...") : PSTR("DETECTED!!!")));
  }
  return (error == 0);
}

bool busReadBuf(busDevice_t *busDev, unsigned short reg, unsigned char *values, uint8_t length)
{
  int error;

  if (busDev && busDev->busType == BUSTYPE_I2C)
  {
    uint8_t address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    busDev->enableCbk(busDev, true);

    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);

    if (busDev->hwType == HWTYPE_EEPROM)
      wire->write( (byte) (reg >> 8) );   //high addr byte
    wire->write((uint8_t)(reg & 0xFF));

    if (0 == (error = wire->endTransmission()))
    {
      if (busDev->hwType == HWTYPE_EEPROM)
        delay(10);
      wire->requestFrom(address, length);

      while (wire->available() != length) ; // wait until bytes are ready
      for (uint8_t x = 0; x < length; x++)
        values[x] = wire->read();

      busDev->enableCbk(busDev, false);
      return true;
    }

    //debug(PSTR("endTransmission() returned %s"), error);
    busDev->enableCbk(busDev, false);
    return false;
  }
  return false;
}

bool busWriteBuf(busDevice_t *busDev, unsigned short reg, unsigned char *values, char length)
{
  int error;

  if (busDev != NULL && busDev->busType == BUSTYPE_I2C)
  {
    int address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    busDev->enableCbk(busDev, true);

    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);
    if (busDev->hwType == HWTYPE_EEPROM)
      wire->write( (byte) (reg >> 8) );   //high addr byte
    wire->write((uint8_t)(reg & 0xFF));
    wire->write(values, length);
    error = wire->endTransmission();

    // EEPROM needs this...
    if (busDev->hwType == HWTYPE_EEPROM && error == 0)
    {
      //wait up to 50ms for the write to complete
      for (uint8_t i = 100; i; --i) {
        delayMicroseconds(500);                     //no point in waiting too fast
        wire->beginTransmission(address);
        wire->write(0);        //high addr byte
        wire->write(0);        //low addr byte
        error = wire->endTransmission();
        if (error == 0) break;
      }
    }

    busDev->enableCbk(busDev, false);

    return (error == 0 ? true : false);
  }

  return false;
}


bool busRead(busDevice_t *busDev, unsigned short reg, unsigned char *values)
{
  return busReadBuf(busDev, reg, values, 1);
}
bool busWrite(busDevice_t *busDev, unsigned short reg, unsigned char value)
{
  return busWriteBuf(busDev, reg, &value, 1);
}
