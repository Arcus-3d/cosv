
// COSV - Cam Open Source Ventilator - by Steven.Carr@hammontonmakers.org
// Use at your own risk, No warranty expressed or implied.
//
// Designed for use with the following 5V Arduino boards
//
// Arduino Uno
// https://components101.com/sites/default/files/component_pin/Arduino-Uno-Pin-Diagram.png
//
// Arduino Nano
// http://www.circuitstoday.com/wp-content/uploads/2018/02/Arduino-Nano-Pin-Description-952x1024.jpg
//
// ADC Pins
//
int ADC_RATE     = A0; //  Knob Input - Rate (breaths per minute)
int ADC_VOLUME   = A1; // Knob Input - Volume (How much of a rotation before reversal)
int ADC_DURATION = A2; // Knob Input - Duration
// A3 - Sensor - Pressure Sensor
// A4 - OLED SDA
// A5 - OLED SCL

//
// PD0 - RX - To GUI Processor
// PD1 - TX - To GUI Processor
int VAPE_IN  = 2; // PD2 - IRQ Digital Input - Cheap Vape Sensor (for now)
int BLDC_FG  = 3; // PD3 - IRQ Digital Input - Motor FG
int BLDC_DIR = 4; // PD4 -            - BLDC Motor Direction
int BLDC_PWM = 5; // PD5 - PWM Output - BLDC Motor speed
int WATCHDOG = 6; // PD6 - Watchdog output pulse
int HOME_SENSOR = 7; // PD7 - Digital Input Home Sensor (Pulled up internally, connect other sensor lead to GND)

int STEP_P0 = 8;
int STEP_P1 = 9;
int STEP_P2 = 10;
int STEP_P3 = 11;


//
// PB2 - SS   -
// PB3 - MISO -
// PB4 - MOSI -
// PB5 - SCK  - Also built in uno led...
int LED = 13;

// Hardware
// BLDC motor is a JGB37-3525
//
// [Red line] is connected to the positive pole of the power supply, and the positive and negative poles cannot be connected incorrectly;
// [Black line] is connected to the negative pole of the power supply, and the positive and negative poles cannot be connected incorrectly;
// [White line] positive and negative control line. When disconnected, the motor turns, and the negative line is connected to the negative pole, and the motor is reversed.
// [Blue line] PWM speed control line, connect 0~5V pulse width governor
// [Yellow line] FG signal line
//
// FG pulses 9 times per revolution of the motor
// Which becomes 270 pulses for 1 shaft rotation
//
// We use analogWrite() to set the PWM for the pin.   (0: Maxmum speed; 255: Stop)

// Pressure Sensor
// MPXV7002DP Transducer APM2.5 APM2.52 Differential Pressure Sensor
// ADC input 0 to VCC
//
// The MPXV7002 is designed to measure positive and negative pressure.
// In addition, with an offset specifically at 2.5V instead of the conventional 0V,
// this new series allows to measure pressure up to 7kPa through each port for
// pressure sensing but also for vacuum sensing
// (refer to the transfer function in the data sheet for more detailed information).
// Features: - -2 to 2 kPa (-0.3 to 0.3 psi). -0.5 to 4.5 V

//https://www.inspiredrc.com/images/ocd2007/O2FundamentalsGraph3.JPG
//  "inhale duration, inhale volume, time between inhale cycles"
// 3 dials.
//  One affects how fast we squeeze the bag, one affects how much we squeeze, and the third is how often
// 

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <splash.h>

#include <avr/interrupt.h>

//#define SCREEN_ADDRESS 0x3D // Address 0x3D for 128x64  Address 0x3C for 128x32
#define SCREEN_ADDRESS 0x3C // Address 0x3D for 128x64  Address 0x3C for 128x32

#define OLED_RESET -1    // No reset pin...
Adafruit_SSD1306 display64(128, 64, &Wire, OLED_RESET);
Adafruit_SSD1306 display32(128, 32, &Wire, OLED_RESET);
Adafruit_SSD1306 *mainDisplay = NULL;
Adafruit_SSD1306 *pitotDisplay = NULL;

// Future, we can deduce whatever ratio is installed and adjust accordingly
int bldcPWMon = 0;
int bldcPWMoff = 255;

// Future, 1/4 rotation for children or smaller lung capacity.
// Should this be adjustable by the user via a POT input?
int bldcFGMax = (270 / 2); // Half rotation is 1 breath

bool bldcMotorIsRunning = false; // Failsafe
bool bldcMotorWasRunning = false; // screen update when motor stops

// 0->1024 (0V to 5V) Pot values to control our core device
int adcRate = 0;
int adcVolume = 0;
int adcDuration = 0;

int adcOldRate = -1;
int adcOldVolume = -1;
int adcOldDuration = -1;

bool displayChanged = false;

bool patientWantsToBreath = false;

int withinRangeOfHomeSensor = 0;

int currentState = 0;
#define STATE_INITIALIZING         0
#define STATE_HOMING_SEEKING_EDGE1 1
#define STATE_HOMING_SEEKING_EDGE2 2
#define STATE_HOMING_CENTERING     3
#define STATE_WAITING              4
#define STATE_BREATHING            5

int bldcFGCount = 0; // See checkStalledMotor()
int homeFgWidth = 0;
int homeFgHalf = 0;

/*** GUI Output Code ***/
// All text, so we can debug, finish with a '\n'
// First character is the message type.
// L = Log
// D = Debug

void guiOutput(const char classType, const __FlashStringHelper *data)
{
  Serial.write(classType);
  Serial.println(data);
}

void guiLog(const __FlashStringHelper *data)
{
  guiOutput('L', data);
}

void guiDebug(const __FlashStringHelper *data)
{
  guiOutput('D', data);
}

void guiEvent(const __FlashStringHelper *data)
{
  guiOutput('E', data);
}

void guiAlarm(const __FlashStringHelper *data)
{
  guiOutput('A', data);
}

void guiValueChanged(const char which, int value)
{
  Serial.write(which);
  Serial.println(value, HEX);
}


/*** End of GUI Output Code ***/






/*** Display code ***/
void refreshDisplay(void) {
  if (mainDisplay)
  {
  mainDisplay->clearDisplay();
  mainDisplay->setCursor(0, 0);     // Start at top-left corner

  mainDisplay->print(F("COSV:"));

  switch (currentState) {
    case STATE_INITIALIZING:
      mainDisplay->println(F("Initializing"));
      break;
    case STATE_HOMING_SEEKING_EDGE1:
      mainDisplay->println(F("Seeking EDGE1"));
      break;
    case STATE_HOMING_SEEKING_EDGE2:
      mainDisplay->println(F("Seeking EDGE2"));
      break;
    case STATE_HOMING_CENTERING:
      mainDisplay->println(F("Centering"));
      break;
    case STATE_WAITING:
      mainDisplay->println(F("Waiting"));
      mainDisplay->print(F("Rate="));
      mainDisplay->println(adcRate);
      mainDisplay->print(F("Volume="));
      mainDisplay->println(adcVolume);
      mainDisplay->print(F("Duration="));
      mainDisplay->println(adcDuration);
      break;
    case STATE_BREATHING:
      mainDisplay->println(F("Breathing"));
      break;
  }

  if (digitalRead(HOME_SENSOR) == HIGH)
  {
    mainDisplay->print(F("NotHome!  "));
  }
  else
  {
    mainDisplay->print(F("HOMED!    "));
  }
  if (bldcMotorIsRunning)
  {
    mainDisplay->println(F("-MOVING-"));
  }
  else
  {
    mainDisplay->println(F("--Idle--"));
  }

  mainDisplay->print(F("FG:"));
  mainDisplay->println(bldcFGCount);
  mainDisplay->display();
  }
}

void scan_i2c()
{
  byte error, address; //variable for error and I2C address
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");

  delay(5000); // wait 5 seconds for the next I2C scan
}

void setupDisplays()
{
  int error;
  Wire.begin(); // Wire communication begin

  // scan_i2c();
  
  if (!mainDisplay)
  {
    // Look for the bigger OLED first
    Wire.beginTransmission(0x3C);  
    if (0 == Wire.endTransmission())
    {
      // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
      display64.begin(SSD1306_SWITCHCAPVCC, 0x3C);
      mainDisplay = &display64;
      guiLog(F("128x64 display detected"));
      mainDisplay->display();
      delay(2000);
      mainDisplay->setTextSize(1);      // Normal 1:1 pixel scale
      mainDisplay->setTextColor(SSD1306_WHITE); // Draw white text
      mainDisplay->cp437(true);         // Use full 256 char 'Code Page 437' font
    }
    else
    {
      Wire.beginTransmission(0x3D);  
      if (0 == Wire.endTransmission())
      {
        // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
        display32.begin(SSD1306_SWITCHCAPVCC, 0x3D);
        mainDisplay = &display32;
        guiLog(F("128x32 display detected"));
        mainDisplay->display();
        delay(2000);
        mainDisplay->setTextSize(1);      // Normal 1:1 pixel scale
        mainDisplay->setTextColor(SSD1306_WHITE); // Draw white text
        mainDisplay->cp437(true);         // Use full 256 char 'Code Page 437' font
      }
  }
}

/* 
 if (!pitotDisplay)
 {
    // Look for the smaller OLED second
    Wire.beginTransmission(0x3C);
    if (0 == Wire.endTransmission())
    {
      // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
      display32.begin(SSD1306_SWITCHCAPVCC, 0x3C);
      pitotDisplay = &display32;
      guiLog(F("128x32 display detected"));
      pitotDisplay->display();
      delay(2000);
      pitotDisplay->setTextSize(1);      // Normal 1:1 pixel scale
      pitotDisplay->setTextColor(SSD1306_WHITE); // Draw white text
      pitotDisplay->setCursor(0, 0);     // Start at top-left corner
      pitotDisplay->cp437(true);         // Use full 256 char 'Code Page 437' font
      pitotDisplay->clearDisplay();
      pitotDisplay->print("VISP Version 0.1 detected");
      pitotDisplay->display();
    }
 }
 */
  if (mainDisplay == NULL)
    guiLog(F("No mainDisplay detected"));

  if (pitotDisplay == NULL)
    guiLog(F("No pitotDisplay detected"));

  currentState = STATE_INITIALIZING;
  refreshDisplay();
}

/* End of OLED display */





/*** Interrupt code ***/
// This is an interrupt routine
void breathTrigger()
{
  patientWantsToBreath = true;
  guiDebug(F("breathTrigger"));
}


// Let the motor run until we get the correct number of pulses
void bldcFGTrigger() // IRQ function
{
  bldcFGCount++;
  if (bldcMotorIsRunning)
  {
    if (bldcFGCount >= bldcFGMax)
    {
      analogWrite(BLDC_PWM, bldcPWMoff);
      bldcFGCount = 0;
      bldcMotorIsRunning = false;
      digitalWrite(LED, LOW);
      guiAlarm(F("OutOfPulses"));
    }
  }
}

/*** End of Interrupt code */

void bldcWaitTillMotorStops()
{
  int oldFGCount=0;
  do
  {
    oldFGCount=bldcFGCount;
    delay(100); // Let's see if any acrue!
  } while (oldFGCount!=bldcFGCount);
}

// This will end up being a 2 stage, with a return to home stage
void bldcStartMotor()
{
  // Don't do this if we are already in a cycle
  if (!bldcMotorIsRunning)
  {
    guiEvent(F("Breathe"));
    bldcMotorIsRunning = true;
    digitalWrite(LED, HIGH);
    analogWrite(BLDC_PWM, bldcPWMon);
    currentState = STATE_BREATHING;
    refreshDisplay();
  }
}

void bldcStopMotor()
{
  if (bldcMotorIsRunning)
  {
    bldcMotorIsRunning = false;
    digitalWrite(LED, LOW);
    analogWrite(BLDC_PWM, bldcPWMoff);
    currentState = STATE_WAITING;
    refreshDisplay();
    guiAlarm(F("Stalled"));
  }
}

/***  HOMING CODE ***/
void bldcFGCounter() // IRQ function
{
  homeFgWidth++;
}

void bldcFGHomeMe() // IRQ function
{
  if (homeFgHalf == 0)
  {
    analogWrite(BLDC_PWM, bldcPWMoff);
    bldcMotorIsRunning = false;
    digitalWrite(LED, LOW);
    currentState = STATE_WAITING;
    guiAlarm(F("OutOfHomedPulses"));

  }
  else
    homeFgHalf--;

}

void homeThisPuppy()
{
  // ok, the Hall Effect is a "close"
  // So, swing till it turns on, and continue till it stops, and check the bldcFgCount.
  // Then walk halfway back and we should be "homed"
  digitalWrite(LED, HIGH);

  guiLog(F("Homing"));


  // Within the home zone...
  // Rewind until out of the zone.
  if (digitalRead(HOME_SENSOR) == HIGH)
  {
    guiLog(F("Find Edge2"));

    currentState = STATE_HOMING_SEEKING_EDGE2;
    refreshDisplay();

    // Reverse the motor
    digitalWrite(BLDC_DIR, HIGH);

    analogWrite(BLDC_PWM, bldcPWMon);
    while (digitalRead(HOME_SENSOR) == HIGH)
    {
      delay(10);
      refreshDisplay();
    }
    analogWrite(BLDC_PWM, bldcPWMoff);
    bldcWaitTillMotorStops();
  }

  guiLog(F("Find Edge1"));
  currentState = STATE_HOMING_SEEKING_EDGE1;
  refreshDisplay();

  // Choose a default direction
  digitalWrite(BLDC_DIR, LOW);

  // Sweep looking for hall sensor
  analogWrite(BLDC_PWM, bldcPWMon);
  while (digitalRead(HOME_SENSOR) == LOW)
  {
    delay(10);
    refreshDisplay();
  }

  // Keep the motor running, but start counting
  bldcFGCount=0;
  // We might miss a few counts with the 1ms dleay tactic
  attachInterrupt(digitalPinToInterrupt(BLDC_FG), bldcFGCounter, RISING);

  guiLog(F("Find Edge2"));
  currentState = STATE_HOMING_SEEKING_EDGE2;
  refreshDisplay();

  while (digitalRead(HOME_SENSOR) == HIGH)
  {
    delay(10);
    refreshDisplay();
  }

  analogWrite(BLDC_PWM, bldcPWMoff);
  detachInterrupt(digitalPinToInterrupt(BLDC_FG));
  bldcWaitTillMotorStops();
  homeFgHalf = homeFgWidth / 2;
  // Motor spun down, and has some steps left to record.
  // Since this is the spin down count, subtract it from the homing
  // but only if it makes sense...
  if (bldcFGCount < homeFgHalf)
      homeFgHalf -= bldcFGCount;
  
  // Reverse the motor
  digitalWrite(BLDC_DIR, HIGH);

  guiLog(F("Centering"));
  currentState = STATE_HOMING_CENTERING;
  refreshDisplay();

  attachInterrupt(digitalPinToInterrupt(BLDC_FG), bldcFGHomeMe, RISING);
  analogWrite(BLDC_PWM, bldcPWMon);
  while (bldcMotorIsRunning)
    delay(1);

  // Reset the motor direction
  digitalWrite(BLDC_DIR, LOW);
  detachInterrupt(digitalPinToInterrupt(BLDC_FG));

  // TADA Motor is homed!
  guiLog(F("Finished"));
  currentState = STATE_WAITING;
  refreshDisplay();
  delay(1000);
}

/*** END HOMING CODE ***/




void setup() {
  Serial.begin(115200);
  guiLog(F("Powerup"));

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  // Watchdog output
  pinMode(WATCHDOG, OUTPUT);
  digitalWrite(WATCHDOG, LOW);

  pinMode(VAPE_IN, INPUT);

  // Setup the motor output
  pinMode(HOME_SENSOR, INPUT_PULLUP); // Short to ground to trigger
  pinMode(BLDC_FG, INPUT);
  pinMode(BLDC_PWM, OUTPUT);
  pinMode(BLDC_DIR, OUTPUT);
  digitalWrite(BLDC_DIR, LOW);

  pinMode(STEP_P0, OUTPUT);
  pinMode(STEP_P1, OUTPUT);
  pinMode(STEP_P2, OUTPUT);
  pinMode(STEP_P3, OUTPUT);

  setupDisplays();

  homeThisPuppy();

  if (homeFgWidth == 0)
    guiLog(F("BLDC Motor not detected"));

  // Setup the BLDC_FG Sensor Input (After homing, as homing uses it's own FG IRQ handlers)
  attachInterrupt(digitalPinToInterrupt(BLDC_FG), bldcFGTrigger, RISING);

  // Setup the Vape Sensor Input
  attachInterrupt(digitalPinToInterrupt(VAPE_IN), breathTrigger, RISING);
}


/*** Timer callback subsystem ***/

typedef void (*tCBK)() ;
typedef struct t  {
  unsigned long tStart;
  unsigned long tTimeout;
  tCBK cbk;
};


bool tCheck (struct t *t ) {
  if (millis() > t->tStart + t->tTimeout)
  {
    t->cbk();
    t->tStart = millis();
  }
}


/*** End of timer callback subsystem ***/


void watchdogPulse()
{
  digitalWrite(WATCHDOG, HIGH);
  digitalWrite(WATCHDOG, LOW);
}

void guiHeartbeat()
{
  guiOutput('H', F("Beat"));
}

void adcCheck()
{
  // A lot of flickering on the readings, so let's get it a bit coarser.
  adcRate = analogRead(ADC_RATE) / 16;
  adcVolume = analogRead(ADC_VOLUME) / 16;
  adcDuration = analogRead(ADC_DURATION) / 16;

  if (adcRate != adcOldRate)
  {
    guiValueChanged('r', adcRate);
    adcOldRate = adcRate;
    displayChanged = true;
  }
  if (adcVolume != adcOldVolume)
  {
    guiValueChanged('v', adcVolume);
    adcOldVolume = adcVolume;
    displayChanged = true;
  }
  if (adcDuration != adcOldDuration)
  {
    guiValueChanged('d', adcDuration);
    displayChanged = true;
    adcOldDuration = adcDuration;
  }
}

void checkStalledMotor()
{
  static int lastMotorCheck = 0;
  static int errorCount = 0;

  if (bldcMotorIsRunning)
  {
    if (lastMotorCheck == bldcFGCount)
    {
      errorCount++;
      if (errorCount >= 3)
        bldcStopMotor();
    }

    lastMotorCheck = bldcFGCount;
  }
  else
  {
    lastMotorCheck = 0;
    errorCount = 0;
  }
}

// Timer Driven Tasks and their Schedules.
// These are checked and executed in order.
// If somethign takes priority over another task, put it at the top of the list
t tasks[] = {
  {0, 100, checkStalledMotor},
  {0, 100, watchdogPulse},
  {0, 250, adcCheck},
  {0, 1000, guiHeartbeat},
  { -1, 0, NULL} // End of list
};


void loop() {

  if (patientWantsToBreath)
  {
    guiDebug(F("patientWantsToBreath"));
    patientWantsToBreath = false;
    bldcStartMotor();
  }

  // Update the display, outside of the interrupt function...
  if (bldcMotorWasRunning && !bldcMotorIsRunning)
  {
    guiEvent(F("Motor Stopped"));
    displayChanged = true;
  }
  bldcMotorWasRunning = bldcMotorIsRunning;

  if (withinRangeOfHomeSensor != digitalRead(HOME_SENSOR))
  {
    withinRangeOfHomeSensor = digitalRead(HOME_SENSOR);
    displayChanged = true;
  }

  for (int x = 0; tasks[x].cbk; x++)
    tCheck(&tasks[x]);

  if (displayChanged)
    refreshDisplay();
}
