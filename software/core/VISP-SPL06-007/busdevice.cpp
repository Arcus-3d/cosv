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
#define MAX_DEVICES 8
busDevice_t devices[MAX_DEVICES];


void busDeviceInit()
{
  memset(&devices, 0, sizeof(devices));
}

static uint8_t muxSelectChannel(busDevice_t *busDev, uint8_t channel)
{
  if (busDev && channel > 0 && channel < 5)
  {
    if (busDev->currentChannel != channel)
    {
      uint8_t error;

      busDev->busdev.i2c.i2cBus->beginTransmission(busDev->busdev.i2c.address);
      busDev->busdev.i2c.i2cBus->write(1 << (channel - 1));
      error = busDev->busdev.i2c.i2cBus->endTransmission();
      busDev->currentChannel = channel;
      // delay(1);
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
  {
    debug(PSTR("%S(busDev is NULL)"), function);
    return;
  }
  if (bus->busType == BUSTYPE_I2C)
  {
    debug(PSTR("%S(0x%x I2C: address=0x%x channel=%d enablePin=%d refCount=%d)"), function, bus, bus->busdev.i2c.address, bus->busdev.i2c.channel, bus->busdev.i2c.enablePin,bus->refCount);
    return;
  }

  if (bus->busType == BUSTYPE_SPI)
  {
    debug(PSTR("%S(SPI CSPIN=%d)"), function, bus->busdev.spi.csnPin);
    return;
  }
}

busDevice_t *busDeviceInitI2C(TwoWire *wire, uint8_t address, uint8_t channel, busDevice_t *channelDev, int8_t enablePin, hwType_e hwType)
{
  busDevice_t *dev = NULL;

  for (uint8_t x = 0; x < MAX_DEVICES; x++)
  {
    if (devices[x].refCount == 0)
    {
      dev = &devices[x];
      memset(dev, 0, sizeof(busDevice_t));
      dev->busType = BUSTYPE_I2C;
      dev->hwType = hwType;
      dev->busdev.i2c.i2cBus = wire;
      dev->busdev.i2c.address = address;
      dev->busdev.i2c.channel = channel; // If non-zero, then it is a channel on a TCA9546 at mux
      dev->busdev.i2c.channelDev = channelDev;
      dev->busdev.i2c.enablePin = enablePin;
      dev->refCount = 1;
      busPrint(dev, PSTR("busDeviceInitI2C()"));
      if (channelDev)
      {
        channelDev->refCount++;
      busPrint(dev, PSTR("   busDeviceInitI2C() SUPPORTING MUX="));
      }
      return dev;
    }
  }
  return NULL;
}

busDevice_t *busDeviceInitSPI(SPIClass *spiBus, uint8_t csnPin, hwType_e hwType)
{
  busDevice_t *dev = NULL;

  for (uint8_t x = 0; x < MAX_DEVICES; x++)
    if (devices[x].refCount == 0)
    {
      dev = &devices[x];
      memset(dev, 0, sizeof(busDevice_t));
      dev->busType = BUSTYPE_SPI;
      dev->hwType = hwType;
      dev->busdev.spi.spiBus = spiBus;
      dev->busdev.spi.csnPin = csnPin;
      dev->refCount = 1;
      return dev;
    }
  return NULL;

}

// TODO: check to see if this is a mux...
void busDeviceFree(busDevice_t *dev)
{
  if (dev != NULL)
  {
    dev->refCount--;
    if (dev->refCount == 0)
    {
      // If there is a muxDevice as a part of this channel, go free it.
      // Note: mux devices might be daisy chained and support multiple layers.
      // Consider a Core with a Mux to 2 VISP sensors with MUX's of their own
      if (dev->busType == BUSTYPE_I2C && dev->busdev.i2c.channelDev)
        busDeviceFree(dev->busdev.i2c.channelDev);
      memset(dev, 0, sizeof(busDevice_t));
    }
  }
}

// Simply detect if the device is present on this device assignment
bool busDeviceDetect(busDevice_t *busDev)
{
  if (busDev && busDev->busType == BUSTYPE_I2C)
  {
    int error;
    uint8_t address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    busPrint(busDev, PSTR("Detecting if present"));

    // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, HIGH);
    muxSelectChannel(busDev->busdev.i2c.channelDev, busDev->busdev.i2c.channel);

    wire->beginTransmission(address);
    error = wire->endTransmission();

    // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, LOW);

    if (error == 0)
      busPrint(busDev, PSTR("DETECTED!!!"));
    else
      busPrint(busDev, PSTR("MISSING..."));
    return (error == 0);
  }
  else
    warning(PSTR("busDeviceDetect() unsupported bustype"));

  return false;
}


bool busReadBuf(busDevice_t *busDev, unsigned short reg, unsigned char *values, uint8_t length)
{
  int error;

  if (busDev != NULL && busDev->busType == BUSTYPE_I2C)
  {
    uint8_t address = busDev->busdev.i2c.address;
    TwoWire *wire = busDev->busdev.i2c.i2cBus;

    // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, HIGH);

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

      // NANO uses NPN switches to enable/disable a bus for DUAL_I2C
      if (busDev->busdev.i2c.enablePin != -1)
        digitalWrite(busDev->busdev.i2c.enablePin, LOW);

      return true;
    }

    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, LOW);

    //debug(PSTR("endTransmission() returned %s"), error);
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

    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, HIGH);

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

    if (busDev->busdev.i2c.enablePin != -1)
      digitalWrite(busDev->busdev.i2c.enablePin, LOW);

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
