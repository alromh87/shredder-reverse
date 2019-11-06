
/*
  this sketch lets an ac induction motor switch direction
  at the push of a three-state button: forward, reverse and stop. And there's a big switch that switches off everything.
  There's a safety in the wiring that makes sure both directions cannot switch on both at once,
  but the arduino wouldn't do that either.
  This sketch also monitors the current through the motor with a Hall sensor. If the current rises above a set limit,
  the machine will register a jam. It will turn back for a second and try again for a set amount of times. If it continues
  to be jammed it switches off.
  Oh and it also makes use of a display to tell the world how it is doing.
  It is possible to use the serial plotter to monitor current, since that is the only number that is dumped on serial.
*/

#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>            //use the LCD Display (yes it is one of those i2c things)
#include <Wire.h>
#include "config.h"
#include "pinsPP.h"

//#define DEBUG 1

#define SHRED_ST  0             // Shredding state Stopped
#define SHRED_FW  1             // Shredding state Forward
#define SHRED_REV 2             // Shredding state Reversing

#define JAMMED_NO 0             // Not jammed state
#define JAMMED_YE 1             // Jammed detected state: waiting for motor to loose inertia
#define JAMMED_RV 2             // Reversing after jammed state
#define JAMMED_RE 3             // Jamming reverse done state: waiting for motor to loose inertia

int jamState = JAMMED_NO;
unsigned long jamTick;

unsigned long jamTime = 0;     // this int will store time between problematic jams
int current;                   // this int will store the measured current
int jammedCounter = 0;         // this int will store the amount of Jams within the minJamTime
unsigned long startTime = 0;   // this int is needed to count the interval between jams.
int i;

boolean working = true;
boolean configMode = false;
boolean tunning = false;
boolean alarmed = false;
boolean readyToWork = false;
unsigned long lastStart = 0;
unsigned long lastCurrentPrint = 0;
float currentAvg = 0;
int currentCount = 0;
int shredDir = SHRED_ST;

LiquidCrystal_I2C lcd(0x3F, 16, 2);           //set the address and dimensions of the LCD. Here the address is 0x3F, but it depends on the chip. You can use and i2c-scanner to determine the address of your chip.

byte pBar[8] = {0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f};

String inputString = "";         // a String to hold incoming data
char * banner = "Precious Plastic v4";
char lcdBuffer[16];
///So here comes what the machine will do on startup
void setup() {
  analogReference(EXTERNAL);      //Use external reference for ADC
  // initialize the output pins:
  pinMode(motionPin, OUTPUT);
  digitalWrite(motionPin, HIGH);  //STOP shredder at boot //TODO: change from inverted to normal logic: HIGH -> ON
  pinMode(directionPin, OUTPUT);
  // initialize input pins:
  pinMode(shredButton, INPUT);
  pinMode(reverseButton, INPUT);
  pinMode(measurePin, INPUT);

  Serial.begin(115200);
  //initialise the lcd
  lcd.init();
  lcd.createChar(0, pBar);
  lcd.setBacklight(HIGH);
  lcd.setCursor(0, 1);
  lcd.print("Shredder Pro  ");
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(0, 0);
    sprintf(lcdBuffer, "%.16s", &(banner[i]));
    lcd.print(lcdBuffer);
    delay(350);
  }
  readConfig();
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("             ");
  inputString.reserve(50);
}

// here comes what the machine will do while running, which is basically shred, reverse or do nothing
void loop() {
  if(configMode)return; //Do noting while in config mode
  current = analogRead(measurePin);
  checkDirection();
  if(!readyToWork)return;
  if (!alarmed) {
    if (jamState == JAMMED_NO) {
      if (working && millis() >= lastStart + runConf.startSpan && current > runConf.maxCurrent) {
        halt();                                    // the machine stops
        countJams();                               // count how often this happens within an unacceaptable timeframe
        if (jammedCounter >= runConf.maxJams) {
          alarm();                                 // If it has jammed too much in timeframe stop everything
        } else {
          jamTick  = millis();
          jamState = JAMMED_YE;
        }
      }
    }
    switch (jamState) {
      case JAMMED_YE:
        if (current <= runConf.v0A) {      // wait a bit to make the shredder halt
          reverse();                               // then it reverses
          jamTick  = millis();
          jamState = JAMMED_RV;
        }
        break;
      case JAMMED_RV:
        if (millis() > jamTick + runConf.unjamReverseT) {          // Reverse for set amount and stop
          halt();
          jamTick  = millis();
          jamState = JAMMED_RE;
        }
        break;
      case JAMMED_RE:
        if (current <= runConf.v0A) {      // Let the motor come to a halt
          jamState = JAMMED_NO;
          if (shredDir == SHRED_FW)
            shred();
          else
            reverse();      //On reversed jam stop and try again
        }
        break;
      default:
        break;
    }
    printCurrent();           // display this as value in amps.
    if(!tunning) printBar();  //displays current as a progress bar.
    else printValue();        //you can also display the measured value.
  }
#ifndef DEBUG
  Serial.println(AREAD_2_A(current));             // You can read the current over serial
#endif
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      if(configMode){
        int v, r_v0A, r_gain, r_startSpan, r_maxJams, r_minJamTime, r_unjamReverseT, r_maxCurrent;
        int decoded = sscanf(const_cast<char*>(inputString.c_str()), "%d,%d,%d,%d,%d,%d,%d,%d", &v, &r_v0A, &r_gain, &r_startSpan, &r_maxCurrent, &r_maxJams, &r_minJamTime, &r_unjamReverseT);
        if(decoded != 8){
          Serial.print("Invalid config received: ");
          Serial.println(inputString);
        }else{
          if(v!=EEPROM_V){
            Serial.println("Config version received doesn't match firmware");
          }else{
            runConf = {
              r_v0A,
              A_GAIN(r_gain),
              r_startSpan,
              0,
              r_maxJams,
              r_minJamTime,
              r_unjamReverseT
            };
            runConf.maxCurrent = A_2_AREAD(r_maxCurrent); //Max current should be calculated after we have loaded config voltage offset for 0A(v0A)
            saveConfig();
            configMode = false;
            tunning = false;
            Serial.println("config ok");
          }
        }
      }
      if(inputString.equals("config\n")){
        digitalWrite(motionPin, HIGH);                        // Switch it all off
        Serial.println("Entering config mode");
        configMode = true;
        working = false;
        lcd.setCursor(0, 0);
        lcd.print("PP Shredder     ");
        lcd.setCursor(0, 1);
        lcd.print("     config mode");
      }
      if(inputString.equals("tune\n")){                       // Display Raw analog read value for tunning
        tunning = true;
      }
      if(inputString.equals("reset\n")){
        Serial.println("Restoring default config");
        restoreConfig();
      }
#ifdef DEBUG
      Serial.print("Received: ");Serial.println(inputString);
#endif
      inputString = "";
    }
  }
}

void shred() {
  lcd.setCursor(0, 0);
  lcd.print("Shredding  ");
#ifdef DEBUG
  Serial.println("Shredding");
#endif
  digitalWrite(motionPin, LOW);                          // set stuff in motion
  digitalWrite(directionPin, LOW);                        // in the forward direction
}

void reverse() {
  lcd.setCursor(0, 0);
  lcd.print("Reversing  ");
#ifdef DEBUG
  Serial.println("I am going back");
#endif
  digitalWrite(motionPin, LOW);                        // Set stuff in motion
  digitalWrite(directionPin, HIGH);                    // in the reverse direction
}

void halt() {
  lcd.setCursor(0, 0);
  lcd.print("Shredder   ");
#ifdef DEBUG
  Serial.println("STOP");   //tell the world you are coming to a halt
#endif
  digitalWrite(motionPin, HIGH);                        // Switch it all off
}

void countJams() {
#ifdef DEBUG
  Serial.print(startTime);
  Serial.print(startTime);
  Serial.print(" vs ");
  Serial.println(millis());
#endif
  if (startTime == 0) {
    jamTime = runConf.minJamTime;                 //If it is the first time, don't mind elapsed time betwen jams
  } else {
    jamTime = millis() - startTime;       //check how much time is between jams and store this in jamTime
  }
  if (jamTime < (runConf.minJamTime)) {        //if that is inside unacceptable limits
    jammedCounter++;                         //count this as a jam
    startTime = millis();       //reset the start time
#ifdef DEBUG
    Serial.print("I jammed ");
    Serial.print(jammedCounter);
    Serial.println(" times.");
#endif
  } else {                        //If it has been ages since the last jam
    startTime = millis();
    jammedCounter = 1;                 //start counting again
  }
  lcd.setCursor(0, 0);
  lcd.print("I jammed ");
  lcd.print(jammedCounter);
  lcd.print(" times now  ");
}


void alarm() {
#ifdef DEBUG
  Serial.println("I am jammed");
#endif
  alarmed = true;
  lcd.setCursor(0, 0);
  lcd.print("Alarm: ");
  lcd.print(jammedCounter);
  lcd.print(" jams    ");
}

void checkDirection() {
  /*
      Code assumes a 3 states button with two pins (active high)
      1. Shred
      2. Reverse
      If none is pushed we assume it is off
      It's not possible to push both at the same time (in that case we assume shred forward)
  */
  int shredInput = digitalRead(shredButton);            //check which button is pressed
  int reverseInput =  digitalRead(reverseButton);
  if (shredInput == LOW && reverseInput == LOW) {       //if you are set to stop
    if (working) {
      halt();                                           //don't move
      working = false;
      shredDir = SHRED_ST;
      jamState = JAMMED_NO;
      //      jammedCounter = 0;                              //Should we restart jamming count?
      alarmed = false;
    }
    if(!readyToWork)readyToWork=true;
  } else {
    if(!readyToWork){
      lcd.setCursor(i, 1);
      lcd.print("Switch off to start");
    }
    if (!working) {
      working = true;
      lastStart = millis();
    }
    //TODO: check rotation shift and wait before changing
    if (shredInput == HIGH) {                         //if you are set to shred
      if (shredDir != SHRED_FW) {
        shred();                                      //shred
        shredDir = SHRED_FW;
      }
    } else {                                          //if you are set to reverse
      if (shredDir != SHRED_REV) {
        reverse();                                    //turn back
        shredDir = SHRED_REV;
      }
    }
  }
}

void restoreConfig(){
  runConf = {
    D_V0A,
    A_GAIN(D_GAIN),
    D_START_SPAN,
    0,
    D_MAX_JAMS,
    D_MIN_JAM_TIME,
    E_UNJAM_REVERSE_T,
  };
  runConf.maxCurrent = A_2_AREAD(MAX_CURRENT); //Max current should be calculated after we have loaded config voltage offset for 0A(v0A)
  saveConfig();
}

void readConfig() {
  int address = 0;
  char id[4];
  byte ver;
  EEPROM.get(address, id);
  address += sizeof(id);
  ver = EEPROM[address];
  address++;

  if ((strcmp(id,EEPROM_ID)!=0) || (ver != EEPROM_V)) {
#ifdef DEBUG
    Serial.println("No config found!\n  Restoring to default config");
#endif
    restoreConfig();
  } else {
    EEPROM.get(address, runConf);
#ifdef DEBUG
    Serial.println("Config loaded correctly!!\n\n");
    Serial.print("0A analogRead offset: "); Serial.println(runConf.v0A);
    Serial.print("Analog read to A gain: "); Serial.println(runConf.AnalogR2A);
    Serial.print("@ start wait for "); Serial.print(runConf.startSpan); Serial.println("ms before detecting jams");
    Serial.print("Detect jam when current over "); Serial.println(AREAD_2_A(runConf.maxCurrent));
    Serial.print("Alarm in case of "); Serial.print(runConf.maxJams);
    Serial.print(" jams detected within "); Serial.print(runConf.minJamTime); Serial.println("ms");
    Serial.print("Reverse for "); Serial.print(runConf.unjamReverseT); Serial.println("ms to unjam");
#endif
  }
#ifdef DEBUG
  if (ver != EEPROM_V) {
    Serial.println("Invalid config version found");
  }
#endif
}
void saveConfig() {
  // We use EEPROM.put to write to EEPROM to handle structure and as a bonus uses update that only writes if data has changed, improving EEPROM life
  int address = 0;
  char id[4]=EEPROM_ID;
  byte ver = EEPROM_V;
  EEPROM.put(address, id);
  address += sizeof(id);
  EEPROM.put(address, ver);
  address++;
  EEPROM.put(address, runConf);
}

void printBar() {       //displays the drawn current as a bar
  int pBari = map(current, runConf.v0A + 10, runConf.maxCurrent, 0, 17); // turn the current current value into a percentage of the currentcap, considering sensor 0 value
  for (i = 0; i < 17; i++) {
    lcd.setCursor(i, 1);
    if (i > pBari)
      lcd.print(" ");
    else
      lcd.write(byte(0));
  }
}

void printValue() {           //displays the drawn current as a value
  lcd.setCursor(0, 1);
  if (current < 1000)lcd.print(" ");
  lcd.print(current);
  lcd.print(" out of ");
  lcd.print(runConf.maxCurrent);
  lcd.print(" ");
}

void printCurrent() {
  float currentA = AREAD_2_A(current);
  if (currentA < 0)currentA = 0;
  if (millis() < lastCurrentPrint + 250) {
    currentAvg += currentA;
    currentCount++;
    return;
  }
  lastCurrentPrint = millis();
  lcd.setCursor(11, 0);
  if (currentCount == 0)
    lcd.print(currentA);
  else
    lcd.print(currentAvg / currentCount);
  lcd.setCursor(15, 0);
  lcd.print("A");
  currentAvg = 0;
  currentCount = 0;
}
