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

typedef enum parser_state_e {
  PARSE_COMMAND,
  PARSE_IGNORE_TILL_LF,
  PARSE_FIRST_ARG,
  PARSE_SECOND_ARG
} parser_state_t;


// Defined once in flash instead of twice, hey, it saves 6 bytes!
const char strEEPROMcomma [] PUTINFLASH = ", 0x";

void sendEEPROMdata(uint16_t address, uint16_t len)
{
  uint8_t *cbuf = (uint8_t *)&visp_eeprom;
  hwSerial.print('E');
  hwSerial.print(',');
  hwSerial.print(millis());
  hwSerial.print(strEEPROMcomma);
  hwSerial.print(address, HEX);
  for (int x = 0; x < len; x++)
  {
    hwSerial.print(strEEPROMcomma);
    hwSerial.print(cbuf[address + x], HEX);
  }
  hwSerial.println();
}

void updateEEPROMdata(uint16_t address, uint8_t data)
{
  uint8_t *cbuf = (uint8_t *)&visp_eeprom;
  cbuf[address] = data;
}

#define RESPOND_ALL           0xFFFFFFFFL
#define RESPOND_LIMITS             1UL<<0
#define RESPOND_DEBUG              1UL<<1
#define RESPOND_MODE               1UL<<2
#define RESPOND_BREATH_VOLUME      1UL<<3
#define RESPOND_BREATH_PRESSURE    1UL<<4
#define RESPOND_BREATH_RATE        1UL<<5
#define RESPOND_BREATH_RATIO       1UL<<6
#define RESPOND_BREATH_THRESHOLD   1UL<<7
#define RESPOND_BODYTYPE           1UL<<8

#define RESPOND_CALIB0             1UL<<10
#define RESPOND_CALIB1             1UL<<11
#define RESPOND_CALIB2             1UL<<12
#define RESPOND_CALIB3             1UL<<13
#define RESPOND_SENSOR0            1UL<<14
#define RESPOND_SENSOR1            1UL<<15
#define RESPOND_SENSOR2            1UL<<16
#define RESPOND_SENSOR3            1UL<<17


struct dictionary_s {
  int theAssociatedValue; // -1 is END OF LIST marker
  const char * PUTINFLASH theWord;
  const char * PUTINFLASH theDescription;
} ;

// Put the strings in flash instead of SRAM
const char strPCCMV [] PUTINFLASH = "PC-CMV";
const char strPCCMVdesc [] PUTINFLASH = "Pressure Controlled CMV";
const char strVCCMV [] PUTINFLASH = "VC-CMV";
const char strVCCMVdesc [] PUTINFLASH = "Volume Controlled CMV";
const char strManual [] PUTINFLASH = "Manual";
const char strManualdesc [] PUTINFLASH = "Manual mode - Controlled by analog dials on the unit";
const char strDisable [] PUTINFLASH = "Disable";
const char strEnable [] PUTINFLASH = "Enable";
const char strDisabled [] PUTINFLASH = "Disabled";
const char strEnabled [] PUTINFLASH = "Enabled";
const char strUnknown [] PUTINFLASH = "Unknown";
const char strPitot [] PUTINFLASH = "Pitot";
const char strPitotDesc [] PUTINFLASH = "Pitot body style";
const char strVenturi [] PUTINFLASH = "Venturi";
const char strVenturiDesc [] PUTINFLASH = "Venturi body style";
const char strSensor0 [] PUTINFLASH = "sensor0";
const char strSensor1 [] PUTINFLASH = "sensor1";
const char strSensor2 [] PUTINFLASH = "sensor2";
const char strSensor3 [] PUTINFLASH = "sensor3";
const char strBMP388 [] PUTINFLASH = "BMP388";
const char strBMP280 [] PUTINFLASH = "BMP280";
const char strSPL06 [] PUTINFLASH = "SPL06";
const char strOff [] PUTINFLASH = "OFF";
const char strModeOffDesc [] PUTINFLASH = "Offline";

const struct dictionary_s sensorDict[] PUTINFLASH = {
  {SENSOR_UNKNOWN, strUnknown, strUnknown},
  {SENSOR_BMP388, strBMP388, strBMP388},
  {SENSOR_BMP280, strBMP280, strBMP280},
  {SENSOR_SPL06, strSPL06, strSPL06},
  { -1, NULL, NULL}
};

const struct dictionary_s modeDict[] PUTINFLASH = {
  {MODE_OFF,     strOff,     strModeOffDesc},
  {MODE_PCCMV,   strPCCMV,   strPCCMVdesc},
  {MODE_VCCMV,   strVCCMV,   strVCCMVdesc},
  {MODE_MANUAL,  strManual,  strManualdesc},
  { -1, NULL, NULL}
};

const struct dictionary_s enableDict[] PUTINFLASH = {
  {DEBUG_DISABLED, strDisable, strDisabled},
  {DEBUG_ENABLED, strEnable,  strEnabled},
  { -1, NULL, NULL}
};

const struct dictionary_s bodyDict[] PUTINFLASH = {
  {VISP_BODYTYPE_UNKNOWN, strUnknown, strUnknown},
  {VISP_BODYTYPE_PITOT,   strPitot, strPitotDesc},
  {VISP_BODYTYPE_VENTURI, strVenturi, strVenturiDesc},
  { -1, NULL, NULL}
};

const char strBreathRatio2 [] PUTINFLASH = "1:2";
const char strBreathRatio3 [] PUTINFLASH = "1:3";
const char strBreathRatio4 [] PUTINFLASH = "1:4";
const char strBreathRatio5 [] PUTINFLASH = "1:5";

const char strBreathRatioDesc2 [] PUTINFLASH = "50% duty cyce";
const char strBreathRatioDesc3 [] PUTINFLASH = "33% duty cyce";
const char strBreathRatioDesc4 [] PUTINFLASH = "25% duty cyce";
const char strBreathRatioDesc5 [] PUTINFLASH = "20% duty cyce";

const struct dictionary_s breathRatioDict[] PUTINFLASH = {
  {2,  strBreathRatio2, strBreathRatioDesc2},
  {3,  strBreathRatio3, strBreathRatioDesc3},
  {4,  strBreathRatio4, strBreathRatioDesc4},
  {5,  strBreathRatio5, strBreathRatioDesc5},
  { -1, NULL, NULL}
};


typedef bool (*verifyCallback)(struct settingsEntry_s *, const char *);
typedef void (*respondCallback)(struct settingsEntry_s *);

struct settingsEntry_s {
  const uint32_t bitmask;
  const int      validModes;
  const char * const theName;
  const int16_t theMin;
  const int16_t theMax;
  const struct dictionary_s  *personalDictionary;
  const verifyCallback  verifyIt;
  const respondCallback respondIt;
  const respondCallback actionIt;
  void * const data;
};

const char strMode [] PUTINFLASH = "mode";
const char strDebug [] PUTINFLASH = "debug";
const char strBodyType [] PUTINFLASH = "bodytype";
const char strBreathVolume [] PUTINFLASH = "volume";
const char strBreathPressure [] PUTINFLASH = "pressure";
const char strBreathRate [] PUTINFLASH = "rate";
const char strBreathRatio [] PUTINFLASH = "ie"; // Inhale/exhale ratio
const char strBreathThreshold [] PUTINFLASH = "breath_threshold";
const char strCalib0 [] PUTINFLASH = "calib0";
const char strCalib1 [] PUTINFLASH = "calib1";
const char strCalib2 [] PUTINFLASH = "calib2";
const char strCalib3 [] PUTINFLASH = "calib3";

bool noSet(struct settingsEntry_s * entry, const char *arg);
bool verifyDictWordToInt8(struct settingsEntry_s * entry, const char *arg);
bool verifyDictWordToInt16(struct settingsEntry_s * entry, const char *arg);
bool verifyLimitsToInt8(struct settingsEntry_s * entry, const char *arg);
bool verifyLimitsToInt16(struct settingsEntry_s * entry, const char *arg);
void respondFloat(struct settingsEntry_s * entry);
void respondInt8(struct settingsEntry_s * entry);
void respondInt16(struct settingsEntry_s * entry);
void respondInt8ToDict(struct settingsEntry_s * entry);
void actionUpdateDisplayIcons(struct settingsEntry_s *);


void handleNewVolume(struct settingsEntry_s * entry)
{
  //volumeTimeout=((motorCycleTime/2)*(visp_eeprom.vreath_volume/MAX_VOLUME); // Half way through a cycle is full compression
}

//const char *const string_table[] PUTINFLASH = {string_0, string_1, string_2, string_3, string_4, string_5};
const struct settingsEntry_s settings[] PUTINFLASH = {
  {RESPOND_MODE,             (MODE_ALL^MODE_MANUAL), strMode, 0, 0, modeDict, verifyDictWordToInt8, respondInt8ToDict, actionUpdateDisplayIcons, &currentMode},
  {RESPOND_BREATH_RATE,      (MODE_ALL^MODE_MANUAL), strBreathRate, MIN_BREATH_RATE, MAX_BREATH_RATE, NULL, verifyLimitsToInt8, respondInt8, NULL, &visp_eeprom.breath_rate},
  {RESPOND_BREATH_RATIO,     (MODE_ALL^MODE_MANUAL), strBreathRatio, 0, 0, breathRatioDict, verifyDictWordToInt8, respondInt8ToDict, NULL, &visp_eeprom.breath_ratio},
  {RESPOND_BREATH_VOLUME,    MODE_VCCMV | MODE_OFF, strBreathVolume, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, handleNewVolume, &visp_eeprom.breath_volume},
  {RESPOND_BREATH_PRESSURE,  MODE_PCCMV | MODE_OFF, strBreathPressure, MIN_BREATH_PRESSURE, MAX_BREATH_PRESSURE, NULL, verifyLimitsToInt16, respondInt16, NULL, &visp_eeprom.breath_pressure},
  {RESPOND_BREATH_THRESHOLD, MODE_NONE, strBreathThreshold, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, NULL, &visp_eeprom.breath_threshold},
  {RESPOND_BODYTYPE,         MODE_NONE, strBodyType, 0, 0, bodyDict, verifyDictWordToInt8, respondInt8ToDict, NULL, &visp_eeprom.bodyType},
  {RESPOND_CALIB0,           MODE_NONE, strCalib0, -1000, 1000, NULL, noSet, respondFloat, NULL, &calibrationOffsets[0]},
  {RESPOND_CALIB1,           MODE_NONE, strCalib1, -1000, 1000, NULL, noSet, respondFloat, NULL, &calibrationOffsets[1]},
  {RESPOND_CALIB2,           MODE_NONE, strCalib2, -1000, 1000, NULL, noSet, respondFloat, NULL, &calibrationOffsets[2]},
  {RESPOND_CALIB3,           MODE_NONE, strCalib3, -1000, 1000, NULL, noSet, respondFloat, NULL, &calibrationOffsets[3]},
  {RESPOND_SENSOR0,          MODE_NONE, strSensor0, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[0].sensorType},
  {RESPOND_SENSOR1,          MODE_NONE, strSensor1, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[1].sensorType},
  {RESPOND_SENSOR2,          MODE_NONE, strSensor2, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[2].sensorType},
  {RESPOND_SENSOR3,          MODE_NONE, strSensor3, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, &sensors[3].sensorType},
  {RESPOND_DEBUG,            MODE_NONE, strDebug, 0, 0, enableDict, verifyDictWordToInt8, respondInt8ToDict, NULL, &debug},
  {0, MODE_NONE,  NULL, 0, 0, NULL, NULL, NULL, NULL}
};


bool noSet(struct settingsEntry_s * entry, const char *arg)
{
  warning(PSTR("%S is read-only"), entry->theName);
  return true;
}

bool verifyDictWordToInt8(struct settingsEntry_s * entry, const char *arg)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;

  do
  {
    memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
    d++;
    if (dict.theWord && strcasecmp_P(arg, dict.theWord) == 0)
    {
      *(uint8_t *)entry->data = dict.theAssociatedValue;
      return true;
    }
  } while (dict.theWord);
  return false;
}

bool verifyDictWordToInt16(struct settingsEntry_s * entry, const char *arg)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;

  do
  {
    memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
    d++;
    if (dict.theWord && strcasecmp_P(arg, dict.theWord) == 0)
    {
      *(uint16_t *)entry->data = dict.theAssociatedValue;
      return true;
    }
  } while (dict.theWord);
  return false;
}

bool verifyLimitsToFloat(struct settingsEntry_s * entry, const char *arg)
{
  float value = atof(arg);
  // TODO: other checks on the argument...
  if (value >= (float)entry->theMin && value <= (float)entry->theMax)
  {
    *(float *)entry->data = value;
    return true;
  }
  return false;
}

bool verifyLimitsToInt16(struct settingsEntry_s * entry, const char *arg)
{
  int value = strtol(arg, NULL, 0);
  // TODO: other checks on the argument...
  if (value >= entry->theMin && value <= entry->theMax)
  {
    *(uint16_t *)entry->data = value;
    return true;
  }
  return false;
}

bool verifyLimitsToInt8(struct settingsEntry_s * entry, const char *arg)
{
  int value = strtol(arg, NULL, 0);
  // TODO: other checks on the argument...
  if (value >= entry->theMin && value <= entry->theMax)
  {
    *(uint16_t *)entry->data = value;
    return true;
  }
  return false;
}

void respondInt8(struct settingsEntry_s * entry)
{
  respond('S', PSTR("%S,%d"), entry->theName, *(int8_t *)entry->data);
}
void respondInt16(struct settingsEntry_s * entry)
{
  respond('S', PSTR("%S,%d"), entry->theName, *(int16_t *)entry->data);
}
void respondFloat(struct settingsEntry_s * entry)
{
  char buff[32];
  *buff = 0;
  dtostrf(*(float*)entry->data, 7, 2, buff);
  respond('S', PSTR("%S,%s"), entry->theName, buff);
}

void respondInt8ToDict(struct settingsEntry_s * entry)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;

  do
  {
    // Must copy from flash to access it
    memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
    d++;
    if (dict.theWord)
    {
      if (dict.theAssociatedValue == *(int8_t*)entry->data)
      {
        respond('S', PSTR("%S,%S"), entry->theName, dict.theWord);
        return;
      }
    }
  } while (dict.theWord);
}

void respondSettingLimits(struct settingsEntry_s * entry)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;
  if (entry->personalDictionary)
  {
    hwSerial.print('S');
    hwSerial.print(',');
    hwSerial.print(millis());
    hwSerial.print(',');
    printp(entry->theName);
    hwSerial.print(F("_dict"));
    do
    {
      memcpy_P(&dict, &entry->personalDictionary[d], sizeof(dict));
      d++;
      if (dict.theWord)
      {
        hwSerial.print(',');
        printp(dict.theWord);
        hwSerial.print(',');
        printp(dict.theDescription);
      }
    } while (dict.theWord);
    hwSerial.println();
  }
  else
  {
    respond('S', PSTR("%S_min,%d"), entry->theName, entry->theMin);
    respond('S', PSTR("%S_max,%d"), entry->theName, entry->theMax);
  }
}

void respondEnabled(struct settingsEntry_s * entry)
{
  respond('S', PSTR("%S_display,%S"), entry->theName,
          ((entry->validModes & currentMode)==0 ? strDisabled : strEnabled));
}

void actionUpdateDisplayIcons(struct settingsEntry_s *)
{
  uint8_t x = 0;
  struct settingsEntry_s entry = {0};

  do
  {
    // We must copy the struct from flash to access it
    memcpy_P(&entry, & settings[x], sizeof(entry));
    x++;
    if (entry.bitmask)
        respondEnabled(&entry);
  }
  while (entry.bitmask != 0);
}

void respondAppropriately(uint32_t flags)
{
  uint8_t x = 0;
  struct settingsEntry_s entry = {0};

  do
  {
    // We must copy the struct from flash to access it
    memcpy_P(&entry, & settings[x], sizeof(entry));
    x++;

    if (entry.bitmask & flags)
    {
      if (flags & RESPOND_LIMITS)
      {
        respondSettingLimits(&entry);
        respondEnabled(&entry);
      }
      entry.respondIt(&entry);
    }
  }
  while (entry.bitmask != 0);
}

void handleSettingCommand(const char *arg1, const char *arg2)
{
  struct settingsEntry_s entry = {0};
  uint8_t x = 0;

  if (*arg1 == 0)
  {
    // Same as Q without the Limits
    respondAppropriately(RESPOND_ALL ^ RESPOND_LIMITS);
    return;
  }

  do
  {
    // We must copy the struct from flash to access it
    memcpy_P(&entry, & settings[x], sizeof(entry));
    x++;
    if (entry.bitmask)
    {
      if ((strcasecmp_P(arg1, entry.theName) == 0))
      {
        if (*arg2 == 0)
        {
          entry.respondIt(&entry);
          return;
        }
        else if (entry.verifyIt(&entry, arg2))
        {
          saveParametersToVISP();
          entry.respondIt(&entry);
          // Maybe enable a motor or something?
          if (entry.actionIt)
            entry.actionIt(&entry);
          return;
        }
        else
          warning(PSTR("Invalid %S value %s"), entry.theName, arg2);
      }
    }
  } while (entry.bitmask);

  warning(PSTR("Unknown setting '%s'"), arg1);
}

void handleQueryCommand(const char *arg1, const char *arg2)
{
  sendCurrentSystemHealth();
  respondAppropriately(RESPOND_ALL);
  respond('Q', PSTR("Finished"));
}

void handleIdentifyCommand(const char *arg1, const char *arg2)
{
  respond('I', PSTR("VISP Core,%d,%d,%d"), VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
}

void handleCalibrateCommand(const char *arg1, const char *arg2)
{
  calibrateClear();
}

// Core system health (we need a way to clear errors, like once the sensors are attached)
// Also we have to add motor failure detection to this.
void sendCurrentSystemHealth()
{
  respond('H', (sensorsFound ? PSTR("good") : PSTR("bad")));
}

void handleHealthCommand(const char *arg1, const char *arg2)
{
  sendCurrentSystemHealth();
}

void handleEepromCommand(const char *arg1, const char *arg2)
{
  uint16_t eepromAddress;

  if (*arg1)
  {
    eepromAddress = strtol(arg1, NULL, 0);

    if (*arg2)
    {
      updateEEPROMdata(eepromAddress, strtol(arg2, NULL, 0));
      sendEEPROMdata(eepromAddress, 1);
      if (eepromAddress == 127)
        saveParametersToVISP();
    }
    else
      sendEEPROMdata(eepromAddress, 16);
  }
  else
    sendEEPROMdata(0, 128);
}

typedef void (*commandCallback)(const char *arg1, const char *arg2);

struct commandEntry_s {
  const char cmdByte;
  const commandCallback doIt;
};

const struct commandEntry_s commands[] PUTINFLASH = {
  { 'C', handleCalibrateCommand},
  { 'I', handleIdentifyCommand},
  { 'Q', handleQueryCommand},
  { 'S', handleSettingCommand },
  { 'E', handleEepromCommand },
  { 'H', handleHealthCommand },
  { 'R', NULL}, // declare reset function at address 0
  { 0, NULL}
};

void commandParser(int cmdByte)
{
  static parser_state_e currentState = PARSE_COMMAND;
  static uint8_t command = 0;
  static char arg1[16] = "", arg2[16] = "";
  static uint8_t arg1Pos = 0, arg2Pos = 0;
  struct commandEntry_s entry = {0};

  // Any kind of garbage input resets the state machine
  if (!isprint(cmdByte))
    currentState = PARSE_IGNORE_TILL_LF;

  // Ignore DOS CR
  if (cmdByte == '\r')
    return;

  // Need to be able to gracefully handle empty lines
  if (cmdByte == '\n')
  {
    int x = 0;
    // Process the command here
    currentState = PARSE_COMMAND;

    do
    {
      // We must copy the struct from flash to access it
      memcpy_P(&entry, & commands[x], sizeof(entry));
      if (command == entry.cmdByte)
        entry.doIt(arg1, arg2);
      x++;
    } while (entry.cmdByte);
    return;
  }

  switch (currentState) {
    case PARSE_COMMAND:
      if (cmdByte == ',')
        currentState = PARSE_FIRST_ARG;
      else
      {
        // And reset!
        command = cmdByte;
        arg1Pos = 0;
        memset(arg1, 0, sizeof(arg1));
        arg2Pos = 0;
        memset(arg2, 0, sizeof(arg2));
      }
      break;
    case PARSE_FIRST_ARG:
      if (cmdByte == ',')
        currentState = PARSE_SECOND_ARG;
      else
      {
        if (arg1Pos < (sizeof(arg1) - 1))
        {
          arg1[arg1Pos++] = cmdByte;
        }
      }
      break;
    case PARSE_SECOND_ARG:
      if (cmdByte == ',')
        currentState = PARSE_IGNORE_TILL_LF; // Default
      else
      {
        if (arg2Pos < (sizeof(arg2) - 1))
        {
          arg2[arg2Pos++] = cmdByte;
        }
      }
      break;
    case PARSE_IGNORE_TILL_LF:
      break;
  }
}

void dataSend(float *P)
{
  // Take some time to write to the serial port
  hwSerial.print('d');
  hwSerial.print(',');
  hwSerial.print(millis());
  hwSerial.print(',');
  hwSerial.print(pressure, 4);
  hwSerial.print(',');
  hwSerial.print(volume, 4);
  hwSerial.print(',');
  hwSerial.print(tidalVolume, 4);
  if (debug == DEBUG_ENABLED)
  {
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U5], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U6], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U7], 1);
    hwSerial.print(',');
    hwSerial.print(P[SENSOR_U8], 1);
  }
  hwSerial.println();
}
