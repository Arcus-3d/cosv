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

#define MAX_ARG_LENGTH 20


bool LOADING_SETTINGS = false;

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


struct dictionary_s {
  int theAssociatedValue; // -1 is END OF LIST marker
  const char * PUTINFLASH theWord; // LIMIT OF 15 CHAR SIZE (see parser argument static character array definition) Do not use ',' or '_' in the words
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
const char strTrue [] PUTINFLASH = "True";
const char strFalse [] PUTINFLASH = "False";
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
const char strHBridge [] PUTINFLASH = "HBridge";
const char strStepper [] PUTINFLASH = "Stepper";
const char strAutoDetect [] PUTINFLASH = "AutoDetect";
const char strAutoDetectDesc [] PUTINFLASH = "Start Motor Autodetection";

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
  //  {MODE_MANUAL,  strManual,  strManualdesc},  // No manual mode setting from the interface
  { -1, NULL, NULL}
};

const struct dictionary_s enableDict[] PUTINFLASH = {
  {DEBUG_DISABLED, strDisable, strDisabled},
  {DEBUG_ENABLED, strEnable,  strEnabled},
  { -1, NULL, NULL}
};

const struct dictionary_s truefalseDict[] PUTINFLASH = {
  {true, strTrue, strTrue},
  {false, strFalse,  strFalse},
  { -1, NULL, NULL}
};

const struct dictionary_s bodyDict[] PUTINFLASH = {
  {VISP_BODYTYPE_UNKNOWN, strUnknown, strUnknown},
  {VISP_BODYTYPE_PITOT,   strPitot, strPitotDesc},
  {VISP_BODYTYPE_VENTURI, strVenturi, strVenturiDesc},
  { -1, NULL, NULL}
};

const struct dictionary_s motorTypeDict[] PUTINFLASH = {
  {MOTOR_UNKNOWN, strUnknown, strUnknown},
  {MOTOR_AUTODETECT, strAutoDetect,   strAutoDetectDesc},
  {MOTOR_HBRIDGE,    strHBridge,   strHBridge},
  {MOTOR_STEPPER, strStepper, strStepper},
  { -1, NULL, NULL}
};


char * currentModeStr(char *buff, int buffSize)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;

  do
  {
    // Must copy from flash to access it
    memcpy_P(&dict, &modeDict[d], sizeof(dict));
    d++;
    if (dict.theWord)
    {
      if (dict.theAssociatedValue == currentMode)
        strncpy_P(buff, dict.theWord, buffSize);
    }
  } while (dict.theWord);
  return buff;
}


const char strBreathRatio2 [] PUTINFLASH = "1:2";
const char strBreathRatio3 [] PUTINFLASH = "1:3";
const char strBreathRatio4 [] PUTINFLASH = "1:4";
const char strBreathRatio5 [] PUTINFLASH = "1:5";

const char strBreathRatioDesc2 [] PUTINFLASH = "50% duty cycle";
const char strBreathRatioDesc3 [] PUTINFLASH = "33% duty cycle";
const char strBreathRatioDesc4 [] PUTINFLASH = "25% duty cycle";
const char strBreathRatioDesc5 [] PUTINFLASH = "20% duty cycle";

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
  const char * const theUnits; // BPM, mL, cm2H20 etc
  const int theMin;
  const int theMax;
  const struct dictionary_s  *personalDictionary;
  const verifyCallback  verifyIt;
  const respondCallback respondIt;
  const respondCallback actionIt;
  const respondCallback handleGood;
  void * const data;
};

// So not use ',' or '_' in the names
const char strMode [] PUTINFLASH = "mode";
const char strDebug [] PUTINFLASH = "debug";
const char strBodyType [] PUTINFLASH = "bodytype";
const char strBreathVolume [] PUTINFLASH = "volume";
const char strBreathPressure [] PUTINFLASH = "pressure";
const char strBreathRate [] PUTINFLASH = "rate";
const char strBreathRatio [] PUTINFLASH = "ie"; // Inhale/exhale ratio
const char strBreathThreshold [] PUTINFLASH = "breathThreshold";
const char strCalib0 [] PUTINFLASH = "calib0";
const char strCalib1 [] PUTINFLASH = "calib1";
const char strCalib2 [] PUTINFLASH = "calib2";
const char strCalib3 [] PUTINFLASH = "calib3";
const char strMotorType [] PUTINFLASH = "motorType";
const char strMotorMinSpeed[] PUTINFLASH = "motorMinSpeed";
const char strMotorHomingSpeed[] PUTINFLASH = "motorHomingSpeed";
const char strMotorSpeed[] PUTINFLASH = "motorSpeed";
const char strMotorStepsPerRev[] PUTINFLASH = "motorStepsPerRev";
const char strBPM[] PUTINFLASH = "bpm";
const char strML[] PUTINFLASH = "mL";
const char strCMH2O[] PUTINFLASH = "cmH2O";
const char strPascals[] PUTINFLASH = "pascals";
const char strPercentage[] PUTINFLASH = "%";
const char strStatus[] PUTINFLASH = "status";
const char strValue[] PUTINFLASH = "value";
const char strGood[] PUTINFLASH = "good";
const char strBad[] PUTINFLASH = "bad";
const char strBattery[] PUTINFLASH = "Battery";

bool noSet(struct settingsEntry_s * entry, const char *arg);
bool verifyDictWordToInt8(struct settingsEntry_s * entry, const char *arg);
bool verifyDictWordToInt16(struct settingsEntry_s * entry, const char *arg);
bool verifyLimitsToInt8(struct settingsEntry_s * entry, const char *arg);
bool verifyLimitsToInt16(struct settingsEntry_s * entry, const char *arg);
void respondFloat(struct settingsEntry_s * entry);
void respondInt8(struct settingsEntry_s * entry);
void respondInt16(struct settingsEntry_s * entry);
void respondInt8ToDict(struct settingsEntry_s * entry);
void actionModeChanged(struct settingsEntry_s *);

// But not inlining these, we save flash space!  32206->32086 on Uno
void __NOINLINE settingReply(const char *nameStr, const char *typeStr, int value)
{
    respond('S', PSTR("%S,%S,%d"), nameStr, typeStr, value);
}
void __NOINLINE settingReply(const char *nameStr, const char *typeStr, const char *valueStr)
{
    respond('S', PSTR("%S,%S,%S"), nameStr, typeStr, valueStr);
}
void __NOINLINE settingReply(const char *nameStr, const char *typeStr, char *valueStr)
{
    respond('S', PSTR("%S,%S,%s"), nameStr, typeStr, valueStr);
}

void __NOINLINE settingReplyValue(const char *nameStr, int value)
{
  respond('S', PSTR("%S,value,%d"), nameStr, value);
}

void __NOINLINE settingReplyValue(const char *nameStr, const char *value)
{
  respond('S', PSTR("%S,value,%S"), nameStr, value);
}
void __NOINLINE settingReplyValue(const char *nameStr, char *value)
{
  respond('S', PSTR("%S,value,%s"), nameStr, value);
}

void __NOINLINE settingReplyStatus(const char *nameStr, bool isGood)
{
    respond('S', PSTR("%S,status,%S"), nameStr, isGood ? strGood : strBad);  
}

void __NOINLINE handleNewVolume(struct settingsEntry_s * entry)
{
  //volumeTimeout=((motorCycleTime/2)*(visp_eeprom.vreath_volume/MAX_VOLUME); // Half way through a cycle is full compression
}

void actionModeChanged(struct settingsEntry_s *);
void __NOINLINE actionMotorChange(struct settingsEntry_s * entry)
{
  motorSetup();
}

// See motor.cpp
void __NOINLINE updateMotorSpeed()
{
  settingReplyValue(strMotorSpeed, motorSpeed);
  settingReply(strMotorSpeed, PSTR("units"), motorRunState==MOTOR_HOMING ? PSTR("Homing") : strPercentage);
}
void __NOINLINE actionMotorSpeed(struct settingsEntry_s * entry)
{
  motorGo();
}



// Buttons can now be good/bad to reflect what is wrong with the core
void __NOINLINE handleMotorGood(struct settingsEntry_s * entry)
{
  settingReplyStatus(entry->theName, motorFound);
}

// Buttons can now be good/bad to reflect what is wrong with the core
void __NOINLINE handleSensorGood(struct settingsEntry_s * entry)
{
  settingReplyStatus(entry->theName, sensorsFound);
}

// Buttons can now be good/bad to reflect what is wrong with the core
void __NOINLINE handleBatteryGood(struct settingsEntry_s * entry)
{
  settingReplyStatus(entry->theName, batteryLevel>30);
}

void __NOINLINE handleVispSaveSettings(struct settingsEntry_s * entry)
{
  saveParametersToVISP();
}

void handleQueryCommand(const char *arg1, const char *arg2);
void __NOINLINE actionQueryCommand(struct settingsEntry_s * entry)
{
  handleQueryCommand(NULL,NULL);
}

//const char *const string_table[] PUTINFLASH = {string_0, string_1, string_2, string_3, string_4, string_5};
const struct settingsEntry_s settings[] PUTINFLASH = {
  {RESPOND_MODE,                        (MODE_ALL ^ MODE_MANUAL), strMode, NULL, 0, 0, modeDict, verifyDictWordToInt8, respondInt8ToDict, actionModeChanged, NULL, &currentMode},
  {RESPOND_BREATH_RATE      |SAVE_THIS, (MODE_ALL ^ MODE_MANUAL), strBreathRate, strBPM, MIN_BREATH_RATE, MAX_BREATH_RATE, NULL, verifyLimitsToInt8, respondInt8, NULL, NULL, &breathRate},
  {RESPOND_BREATH_RATIO     |SAVE_THIS, (MODE_ALL ^ MODE_MANUAL), strBreathRatio, NULL, 0, 0, breathRatioDict, verifyDictWordToInt8, respondInt8ToDict, NULL, NULL, &breathRatio},
  {RESPOND_BREATH_VOLUME    |SAVE_THIS, (MODE_VCCMV | MODE_OFF), strBreathVolume, strML, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, handleNewVolume, NULL, &breathVolume},
  {RESPOND_BREATH_PRESSURE  |SAVE_THIS, (MODE_PCCMV | MODE_OFF), strBreathPressure, strCMH2O, MIN_BREATH_PRESSURE, MAX_BREATH_PRESSURE, NULL, verifyLimitsToInt16, respondInt16, NULL, NULL, &breathPressure},
  {RESPOND_BREATH_THRESHOLD |SAVE_THIS,  MODE_NONE, strBreathThreshold, NULL, 0, 1000, NULL, verifyLimitsToInt16, respondInt16, NULL, NULL, &breathThreshold},
  {RESPOND_BODYTYPE|EXPERT,              MODE_ALL,  strBodyType, NULL, 0, 0, bodyDict, verifyDictWordToInt8, respondInt8ToDict, NULL, handleVispSaveSettings, &visp_eeprom.bodyType},
  {RESPOND_BATTERY,                      MODE_ALL,  strBattery, NULL, 0, 100, NULL, noSet, respondInt8, NULL, handleBatteryGood, &batteryLevel},
  {RESPOND_CALIB0|EXPERT,                MODE_ALL, strCalib0, strPascals, -1000, 1000, NULL, noSet, respondFloat, NULL, NULL, &calibrationOffsets[0]},
  {RESPOND_CALIB1|EXPERT,                MODE_ALL, strCalib1, strPascals, -1000, 1000, NULL, noSet, respondFloat, NULL, NULL, &calibrationOffsets[1]},
  {RESPOND_CALIB2|EXPERT,                MODE_ALL, strCalib2, strPascals, -1000, 1000, NULL, noSet, respondFloat, NULL, NULL, &calibrationOffsets[2]},
  {RESPOND_CALIB3|EXPERT,                MODE_ALL, strCalib3, strPascals, -1000, 1000, NULL, noSet, respondFloat, NULL, NULL, &calibrationOffsets[3]},
  {RESPOND_SENSOR0|EXPERT,               MODE_ALL, strSensor0, NULL, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, handleSensorGood, &sensors[0].sensorType},
  {RESPOND_SENSOR1|EXPERT,               MODE_ALL, strSensor1, NULL, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, handleSensorGood, &sensors[1].sensorType},
  {RESPOND_SENSOR2|EXPERT,               MODE_ALL, strSensor2, NULL, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, handleSensorGood, &sensors[2].sensorType},
  {RESPOND_SENSOR3|EXPERT,               MODE_ALL, strSensor3, NULL, 0, 0, sensorDict, noSet, respondInt8ToDict, NULL, handleSensorGood, &sensors[3].sensorType},
  {RESPOND_MOTOR_TYPE|EXPERT|SAVE_THIS,          (MODE_ALL ^ MODE_MANUAL), strMotorType, NULL, 0, 0, motorTypeDict, verifyDictWordToInt8, respondInt8ToDict, actionMotorChange, handleMotorGood, &motorType},
  {RESPOND_MOTOR_SPEED|EXPERT,                   (MODE_ALL ^ MODE_MANUAL), strMotorSpeed, strPercentage,        0, 100, NULL, verifyLimitsToInt8, respondInt8, actionMotorSpeed, NULL, &motorSpeed},
  {RESPOND_MOTOR_MIN_SPEED|EXPERT|SAVE_THIS,     (MODE_ALL ^ MODE_MANUAL), strMotorMinSpeed, strPercentage,     0, 100, NULL, verifyLimitsToInt8, respondInt8, actionMotorChange, NULL, &motorMinSpeed},
  {RESPOND_MOTOR_HOMING_SPEED|EXPERT|SAVE_THIS,  (MODE_ALL ^ MODE_MANUAL), strMotorHomingSpeed, strPercentage,  0, 100, NULL, verifyLimitsToInt8, respondInt8, actionMotorChange, NULL, &motorHomingSpeed},
  {RESPOND_MOTOR_STEPS_PER_REV|EXPERT|SAVE_THIS, (MODE_ALL ^ MODE_MANUAL), strMotorStepsPerRev, NULL, 0, 1600,    NULL, verifyLimitsToInt16, respondInt16, actionMotorChange, NULL, &motorStepsPerRev},
  {RESPOND_DEBUG,                                (MODE_ALL ^ MODE_MANUAL), strDebug, NULL, 0, 0, enableDict, verifyDictWordToInt8, respondInt8ToDict, actionQueryCommand, NULL, &debug},
  {0, MODE_NONE,  NULL, NULL, 0, 0, NULL, NULL, NULL, NULL}
};


bool __NOINLINE noSet(struct settingsEntry_s * entry, const char *arg)
{
  warning(PSTR("%S is read-only"), entry->theName);
  return true;
}

bool __NOINLINE verifyDictWordToInt8(struct settingsEntry_s * entry, const char *arg)
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

bool __NOINLINE verifyDictWordToInt16(struct settingsEntry_s * entry, const char *arg)
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

bool __NOINLINE verifyLimitsToFloat(struct settingsEntry_s * entry, const char *arg)
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

bool __NOINLINE verifyLimitsToInt16(struct settingsEntry_s * entry, const char *arg)
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

bool __NOINLINE verifyLimitsToInt8(struct settingsEntry_s * entry, const char *arg)
{
  int value = strtol(arg, NULL, 0);
  // TODO: other checks on the argument...
  if (value >= entry->theMin && value <= entry->theMax)
  {
    *(uint8_t *)entry->data = value;
    return true;
  }
  return false;
}

void __NOINLINE respondInt8(struct settingsEntry_s * entry)
{
  settingReplyValue(entry->theName, *(int8_t *)entry->data);
}
void __NOINLINE respondInt16(struct settingsEntry_s * entry)
{
  settingReplyValue(entry->theName, *(int16_t *)entry->data);
}
void __NOINLINE respondFloat(struct settingsEntry_s * entry)
{
  hwSerial.print('S');
  hwSerial.print(',');
  hwSerial.print(millis());
  hwSerial.print(',');
  printp(entry->theName);
  printp(PSTR(",value,"));
  hwSerial.print(*(float*)entry->data, 4);
  hwSerial.println();
}

void __NOINLINE respondInt8ToDict(struct settingsEntry_s * entry)
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
        settingReplyValue(entry->theName, dict.theWord);
        return;
      }
    }
  } while (dict.theWord);
}

void __NOINLINE respondStatus(struct settingsEntry_s * entry)
{
  if (entry->handleGood)
    entry->handleGood(entry);
}


void __NOINLINE respondSettingLimits(struct settingsEntry_s * entry)
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
    hwSerial.print(F(",dict"));
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
    settingReply(entry->theName, PSTR("min"), entry->theMin);
    settingReply(entry->theName, PSTR("max"), entry->theMax);
  }
  // What group does this belong to?
  settingReply(entry->theName, PSTR("group"), (entry->verifyIt == noSet ? PSTR("status") : PSTR("button")));

  // Any special units we need to display in the button?
  if (entry->theUnits)
    settingReply(entry->theName, PSTR("units"), entry->theUnits);
}

void __NOINLINE respondEnabled(struct settingsEntry_s * entry)
{
  bool showThis = ((entry->validModes & currentMode) ? true : false);
  if ((entry->bitmask & EXPERT) && debug==DEBUG_DISABLED)
      showThis = false;

  settingReply(entry->theName, PSTR("enabled"), (showThis ? strTrue : strFalse));
}

// Called when mode changes (Hijacked by motor change to update motor settings on the display)
void __NOINLINE actionModeChanged(struct settingsEntry_s *)
{
  uint8_t x = 0;
  struct settingsEntry_s entry = {0};

  if (LOADING_SETTINGS)
    return;

  do
  {
    // We must copy the struct from flash to access it
    memcpy_P(&entry, & settings[x], sizeof(entry));
    x++;
    if (entry.bitmask)
    {
      respondEnabled(&entry);
      respondStatus(&entry);
      entry.respondIt(&entry);
    }
  }
  while (entry.bitmask != 0);
}

void __NOINLINE respondAppropriately(uint32_t flags)
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
        respondSettingLimits(&entry); // Limits, Dictionary, and group output
        respondEnabled(&entry);       // Is the icon enabled/disabled?
      }
      respondStatus(&entry); // Is it good or bad?
      entry.respondIt(&entry); // What is it's value?
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
    if (!LOADING_SETTINGS)
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
          if (!LOADING_SETTINGS)
          {
            // Only perform a save if the setting actually needs to be saved.
            if (entry.bitmask & SAVE_THIS)
              coreSaveSettings();

            entry.respondIt(&entry);
          }
          // Maybe enable a motor or something?
          if (entry.actionIt)
            entry.actionIt(&entry);
          if (!LOADING_SETTINGS)
            respondStatus(&entry);
          return;
        }
        else
        {
          warning(PSTR("Invalid %S value %s"), entry.theName, arg2);
          return;
        }
      }
    }
  } while (entry.bitmask);

  warning(PSTR("Unknown setting '%s'"), arg1);
}

void handleQueryCommand(const char *arg1, const char *arg2)
{
  respond('Q', PSTR("Begin"));  // Interface should start creating buttons now
  respondAppropriately(RESPOND_ALL);
  respond('Q', PSTR("End"));    // Interface should stop creating buttons now
}

void handleIdentifyCommand(const char *arg1, const char *arg2)
{
  respond('I', PSTR("Core,%d,%d,%d"), VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
}

void handleCalibrateCommand(const char *arg1, const char *arg2)
{
  calibrateClear();
}

// Core system health (we need a way to clear errors, like once the sensors are attached)
// Also we have to add motor failure detection to this.
void sendCurrentSystemHealth()
{
  respond('H', (sensorsFound && motorFound ? strGood : strBad));
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
  static uint8_t theCommandByte = 0;
  static char arg1[MAX_ARG_LENGTH] = "", arg2[MAX_ARG_LENGTH] = ""; // MAX ARG LENGTH
  static uint8_t arg1Pos = 0, arg2Pos = 0;
  struct commandEntry_s entry = {0};

  if (!isprint(cmdByte))
    currentState = PARSE_IGNORE_TILL_LF;

  if (cmdByte == '\n' || cmdByte == '\r')
  {
    int x = 0;
    if (theCommandByte)
    {
      // Need to be able to gracefully handle empty lines
      do
      {
        // We must copy the struct from flash to access it
        memcpy_P(&entry, & commands[x], sizeof(entry));
        if (entry.cmdByte && theCommandByte == entry.cmdByte)
          entry.doIt(arg1, arg2);
        x++;
      } while (entry.cmdByte);
    }
    // Process the command here
    currentState = PARSE_COMMAND;
    theCommandByte = 0;
    return;
  }

  // Ignore whitespace
  if (isspace(cmdByte))
    return;

  switch (currentState) {
    case PARSE_COMMAND:
      if (cmdByte == ',')
        currentState = PARSE_FIRST_ARG;
      else
      {
        // And reset!
        theCommandByte = cmdByte;
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
          arg1[arg1Pos] = cmdByte;
          arg1Pos++;
          arg1[arg1Pos] = 0;
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
          arg2[arg2Pos] = cmdByte;
          arg2Pos++;
          arg2[arg2Pos] = 0;
        }
      }
      break;
    case PARSE_IGNORE_TILL_LF:
      break;
  }
}

void dataSend()
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
    hwSerial.print(sensors[SENSOR_U5].pressure, 1);
    hwSerial.print(',');
    hwSerial.print(sensors[SENSOR_U6].pressure, 1);
    hwSerial.print(',');
    hwSerial.print(sensors[SENSOR_U7].pressure, 1);
    hwSerial.print(',');
    hwSerial.print(sensors[SENSOR_U8].pressure, 1);
  }
  hwSerial.println();
}


void primeTheFrontEnd()
{
  respondAppropriately(RESPOND_ALL);//  ^ RESPOND_LIMITS);
}

void sanitizeCoreData()
{
  if (breathRatio > MAX_BREATH_RATIO)
    breathRatio = MAX_BREATH_RATIO;
  if (breathRatio < MIN_BREATH_RATIO)
    breathRatio = MIN_BREATH_RATIO;
  if (breathRate > MAX_BREATH_RATE)
    breathRate = MAX_BREATH_RATE;
  if (breathRate < MIN_BREATH_RATE)
    breathRate = MIN_BREATH_RATE;
  if (breathPressure > MAX_BREATH_PRESSURE)
    breathPressure = MAX_BREATH_PRESSURE;
  if (breathPressure < MIN_BREATH_PRESSURE)
    breathPressure = MIN_BREATH_PRESSURE;
}


#include <EEPROM.h>

const unsigned long crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static unsigned long crc;

unsigned long  eeprom_crc(void) {

  crc = ~0L;

  for (int index = 4 ; index < EEPROM.length() ; ++index) {
    crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}


int coreSaveName(int i, const char *name)
{
  char ch;

  EEPROM.write(i++, 'S');
  EEPROM.write(i++, ',');
  while ((ch = pgm_read_byte(name++)) != 0x00)
    EEPROM.write(i++, ch);
  EEPROM.write(i++, ',');
  return i;
}

int coreSaveDict(int i, const struct dictionary_s *dictionary, int value)
{
  struct dictionary_s dict = {0};
  uint8_t d = 0;
  char ch;
  do
  {
    memcpy_P(&dict, &dictionary[d], sizeof(dict));
    d++;
    if (dict.theWord && dict.theAssociatedValue == value)
    {
      while ((ch = pgm_read_byte(dict.theWord++)) != 0x00)
        EEPROM.write(i++, ch);
      break;
    }
  } while (dict.theWord);
  EEPROM.write(i++, '\n');
  return i;
}

int coreSaveValue(int i, int value)
{
  char out[6]; // 65536 is 5 chars long
  char *c = out;
  bool isNeg = value < 0;

  if (value == 0)
    EEPROM.write(i++, '0');
  else
  {
    if (isNeg)
      value = -value;

    while (value > 0)
    {
      *c = value % 10 + '0';
      value /= 10;
      c++;
    }
    if (isNeg)
      EEPROM.write(i++, '-');
    while (c > out)
    {
      c--;
      EEPROM.write(i++, *c);
    }
  }
  //for (int d = 10000; d > 0; d /= 10)
  //{
  //  if (value >= d)
  //    EEPROM.write(i++, '0' + ((value / d) % 10));
  //}
  //  if (value>=1000) EEPROM.write(i++, '0' + ((value/1000) % 10));
  ///  if (value>=100) EEPROM.write(i++, '0' + ((value/100) % 10));
  //  if (value>=10) EEPROM.write(i++, '0' + ((value/10) % 10));
  //  EEPROM.write(i++, '0' + (value % 10));
  EEPROM.write(i++, '\n');
  return i;
}




/* There are 2 different EEPROMS.. One on the VISP PCB, the other in the Core */
/* We treat the core's EEPROM as a command string! */
#define SAVE_SETTINGS 1
uint32_t  CORE_SAVE_SETTINGS_TIMEOUT = 0;

bool coreSaveSettings()
{
  if (LOADING_SETTINGS)
    return false;

  // Every time someone moves a slider, we get a bunch of 'S' events
  // This waits for them to settle down before saving at one shot
  // Reduces wear on the EEPROM
  CORE_SAVE_SETTINGS_TIMEOUT = millis() + 5000; // In 5 seconds, save to the core...
  return true;
}

// Break the writes up into smaller segments so that it is spread out over time
void coreSaveSettingsStateMachine()
{
  static int8_t  CORE_SAVE_SETTINGS_STATE = 0;
  static int i;

  switch (CORE_SAVE_SETTINGS_STATE) {
    case 0:
      if (CORE_SAVE_SETTINGS_TIMEOUT != 0 && CORE_SAVE_SETTINGS_TIMEOUT < millis())
      {
        CORE_SAVE_SETTINGS_STATE++;
        CORE_SAVE_SETTINGS_TIMEOUT = 0;
        info(PSTR("saving"));
      }
      break;
    case 1:
      i = coreSaveName(4, strMotorType);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 2:
      i = coreSaveDict(i, motorTypeDict, motorType);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 3:
      i = coreSaveName(i, strMotorMinSpeed);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 4:
      i = coreSaveValue(i, motorMinSpeed);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 5:
      i = coreSaveName(i, strMotorHomingSpeed);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 6:
      i = coreSaveValue(i, motorHomingSpeed);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 7:
      i = coreSaveName(i, strMotorStepsPerRev);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 8:
      i = coreSaveValue(i, motorStepsPerRev);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 9:
      i = coreSaveName(i, strBreathPressure);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 10:
      i = coreSaveValue(i, breathPressure);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 11:
      i = coreSaveName(i, strBreathVolume);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 12:
      i = coreSaveValue(i, breathVolume);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 13:
      i = coreSaveName(i, strBreathRate);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 14:
      i = coreSaveValue(i, breathRate);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 15:
      i = coreSaveName(i, strBreathRatio);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 16:
      i = coreSaveDict(i, breathRatioDict, breathRatio);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 17:
      i = coreSaveName(i, strBreathThreshold);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 18:
      i = coreSaveValue(i, breathThreshold);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 19:
      if (i < EEPROM.length())
        EEPROM.write(i++, 0);
      else
        CORE_SAVE_SETTINGS_STATE++;
      break;
#ifndef WANT_TRICKLE_EEPROM
    case 20:
      EEPROM.put(0, eeprom_crc());
      info(PSTR("saved"));
      CORE_SAVE_SETTINGS_STATE = 0;
      break;
#else
    case 20:
      crc = ~0L;
      i = 4;
      CORE_SAVE_SETTINGS_STATE++;
    // break; fall through
    case 21:
      if (i < EEPROM.length())
      {
        crc = crc_table[(crc ^ EEPROM[i]) & 0x0f] ^ (crc >> 4);
        crc = crc_table[(crc ^ (EEPROM[i++] >> 4)) & 0x0f] ^ (crc >> 4);
        crc = ~crc;
      }
      else
        CORE_SAVE_SETTINGS_STATE++;
      break;
    case 22:
      EEPROM.put(0, crc);
      CORE_SAVE_SETTINGS_STATE++;
      break;
    case 23:
      info(PSTR("saved"));
      CORE_SAVE_SETTINGS_STATE = 0;
      break;
#endif
  }
}

bool coreLoadSettings()
{
  unsigned long crc;

  LOADING_SETTINGS = true;
  EEPROM.get(0, crc);
  if (eeprom_crc() == crc)
  {
    int ch;
    // No idea what junk may have come in from the hwSerial yet
    commandParser('\n');
    for (int i = 4; (i < EEPROM.length()) && (ch = EEPROM.read(i)); i++)
    {
      // info(PSTR("E[%d]:%c"), i, ch);
      commandParser(ch);
    }
  }
  LOADING_SETTINGS = false;
  sanitizeCoreData();
  return false;
}
