/*
Clock built with 4 or 6 RGBDigits (www.rgbdigit.com)
and DS3231 RTC module

(c) Erik Homburg 2017

Color schemes happily borrowed from www.rgbdigit.com

This software is free software

The clock is controlled with IR remote controller

use of remote:
up and down arrow    : change brightness
left and right arrow : change between hh:mm and ss display
star or A            : enter time set mode
  left and right arrow : change digit to modify 
  up and down arrow    : change digit value
  hashtag or cancel    : cancel edit mode, no change
  ok or select         : change time and leave edit mode
*/

// #include <Wire.h> // already included in DS3231.h
#include <DS3231.h>
#include <RGBDigitV2.h>
#include <IRremote.h>
//#include <Adafruit_NeoPixel> // already called in RGBDigitV2.h

const byte digitsDataPin = 12;
const byte numDigits = 6;  // select 4 or 6
RGBDigit myDisplay(numDigits, digitsDataPin);

// for IR remote
const int RECV_PIN = 10;
IRrecv irReceiver(RECV_PIN);
decode_results results;

// define Clock class
DS3231 Clock;

// some color definitions in packed 32 bit format
#define veryVeryDarkGray  0x010101
#define white             0x808080
#define rgb_Sienna        0xA0522D
#define rgb_CadetBlue     0x5F9EA0

#define dotColor            white
#define editBackGroundColor rgb_CadetBlue
#define editForeGroundColor rgb_Sienna

// comment and uncomment the next two blocks
// depending on the remote control unit used

// TinyTronics IRREMOTEMODBLK
#define STAR        0x32C6FDF7       
#define HASHTAG     0x3EC3FC1B     
#define ONE         0xC101E57B     
#define TWO         0x97483BFB    
#define THREE       0xF0C41643     
#define FOUR        0x9716BE3F
#define FIVE        0x3D9AE3F7
#define SIX         0x6182021B
#define SEVEN       0x8C22657B
#define EIGHT       0x488F3CBB 
#define NINE        0x0449E79F
#define ZERO        0x1BC0157B
#define UP          0x00511DBB
#define DOWN        0xA3C8EDDB
#define LEFT        0x52A3D41F
#define RIGHT       0x20FE4DBB
#define OK          0xD7E84B1B

#define EDIT        STAR
#define CANCEL      HASHTAG
#define SELECT      OK
// end of TinyTronics IRREMOTEMODBLK

/*
COM-11759 SPARKFUN, FLORIS.CC
#define POWER       0x10EFD827 
#define AKEY        0x10EFF807 
#define BKEY        0x10EF7887
#define CKEY        0x10EF58A7
#define UP          0x10EFA05F
#define DOWN        0x10EF00FF
#define LEFT        0x10EF10EF
#define RIGHT       0x10EF807F
#define SELECT      0x10EF20DF

#define EDIT        AKEY
#define CANCEL      POWER
//end of COM-11759 SPARKFUN, FLORIS.CC
*/

enum buttonType{none, up, down, left, right, select, edit, cancel};
buttonType button = none;

// position of 7 segment display digits
byte hoursHighDigit = 0;
byte hoursLowDigit = hoursHighDigit + 1;
byte minutesHighDigit = 2;
byte minutesLowDigit = minutesHighDigit +1;
byte secondsHighDigit = 4;
byte secondsLowDigit = secondsHighDigit + 1;

// color schemes are used to make a vivid digit display
enum colorSchemeType{wild, earth, water, fire, air};
const colorSchemeType hoursPaintScheme = fire;
const colorSchemeType minutesPaintScheme = water;
const colorSchemeType secondsPaintScheme = air;

// measured average current draw
// water water brightness 13 200mA
// fire fire   brightness 13 160mA
// air air     brightness 13 190mA
// earth earth brightness 13 140mA
// wild wild   brightness 13 210mA

// every <refreshDivider> seconds the displayed digits are repainted
// with new colors according the selected color scheme
// an integer number of <refreshDivider>'s should fit exactly into 60 seconds
// possibilities enough!
int refreshDivider = 3;

// logarithmic distribution
const byte brightnessTable[] = {
  5, 6, 8, 10, 13, 16, 20, 26, 32, 40, 51, 64, 81, 102, 128
};
/*
const byte brightnessTable[] = {
  5, 6, 8, 10, 13, 16, 20, 26, 32, 40, 51, 64, 81, 102, 128, 161, 203, 255
};
*/
// initial brightness
byte brightnessIndex = 4;

byte oldSec;
byte newSec;

void setup() {
	// Start the I2C interface
	Wire.begin();

	myDisplay.begin(); // start the displays and set brightness
	myDisplay.clearAll();
	
	myDisplay.setBrightness(brightnessTable[brightnessIndex]);
	
  newSec = Clock.getSecond();
  oldSec = newSec;

  // get the time from the Real Time Clock (DS3231) and display
  showTime();

  irReceiver.enableIRIn(); // Start infrared receiver   

}

// length of blinking seconds indicator
const unsigned long secondsPulseInterval = 500;

// previous states for state mechanism
unsigned long previousMillis = 0;

void loop() {
  
  unsigned long currentMillis = millis();

   // dim seconds indicater as time has elapsed
  if(currentMillis - previousMillis >= secondsPulseInterval){
    previousMillis = currentMillis;
    myDisplay.clearDot(1);
    myDisplay.clearDot(3);
  }

  // find seconds boundary
  if (newSec == oldSec){
    newSec = Clock.getSecond();
  }
  else{
    oldSec = newSec;
    
    displaySeconds(newSec);

    // after <refreshDivider> seconds, display total time
    if ((newSec % refreshDivider) == 0){
      showTime();
    }

    // start display of seconds pulse
    previousMillis = currentMillis;
    myDisplay.setDot(1, dotColor);
    myDisplay.setDot(3, dotColor);
    
  };
  
  // handle brightness setting
  // handle seconds display
  button = getCommand();        
  switch(button){
    case up:
      if(brightnessIndex < (sizeof(brightnessTable) - 1)){
        brightnessIndex++;
        myDisplay.setBrightness(brightnessTable[brightnessIndex]);
      }
    break;
    case down:
      if(brightnessIndex > 0){
        brightnessIndex--;
        myDisplay.setBrightness(brightnessTable[brightnessIndex]);
      }
    break;
    case left:
      if( numDigits == 4){
        setSecondsOnlyDisplay();
        myDisplay.clearAll();
        showTime();
      }
    break;
    case right:
      if(numDigits == 4){
        setHoursAndMinutesDisplay();
        showTime();
      }
    break;
    case edit:
      setClock();
    break;
  }

}; // end of loop()

// draw a new color value depending on color scheme s
uint32_t getColorFromScheme(colorSchemeType s){
  byte z;
  switch (s){
    case earth:
      z = random(100);
      return myDisplay.Color(25 + (z * 63)/100, 13 + (z * 12)/100, 1 + (z * 3 )/100);
    break;
    case fire:
      return myDisplay.Color(100, random(10,100), 0);
   break;
    case water:
      return myDisplay.Color(0, 130, random(10,200)); 
    break;
    case air:
      z = random(32 ,96);
      return myDisplay.Color(z, z, 96);
    break;
    case wild:
    default:
      return myDisplay.Color(random(1, 200), random(1, 200), random(1, 200));
    break;    
  }
}

// decode remote commands
buttonType getCommand(){ 
  buttonType x;
  if (irReceiver.decode(&results)){
    switch(results.value){
      case EDIT: 
        x = edit; 
      break;
      case CANCEL: 
        x = cancel; 
      break;
      case UP:
        x = up;
      break;
      case DOWN:
        x = down;
      break;  
      case LEFT:
        x = left;
      break;
      case RIGHT:
        x =right;
      break;
      case SELECT:
        x = select;
      break;
      default:
        x = none;
    };
    irReceiver.resume();
    return x;
  }
  else{
    return none;
  }
}

// setClock enables the user to set a desired time to the Real Time Clock
void setClock(){
  
  bool dontCare;  // arguments of getHour  are not important
  byte h = Clock.getHour(dontCare, dontCare);
  byte m = Clock.getMinute();
  byte s = Clock.getSecond();
  
  // fill the edit line, seconds are set to zero
  char timeEditLine[7];
  timeEditLine[0] = h / 10;
  timeEditLine[1] = h % 10;
  timeEditLine[2] = m / 10;
  timeEditLine[3] = m % 10;
  timeEditLine[4] = 0;
  timeEditLine[5] = 0;

  int p = 0;// index to digit positions in the edit line
  // max values of posible digit values
  // note that hours can be up to 29, however at exiting
  // the function values too high are truncated to 23.
  byte timeEditLineMax[6] = {2, 9, 5, 9, 5 ,9};

  myDisplay.setText(timeEditLine, 0, numDigits, editBackGroundColor);
  myDisplay.setDigit(timeEditLine[p] ,p, editForeGroundColor);
  
  buttonType button = getCommand();
  
  // begin of edit loop
  while((button != select) && (button != cancel)){
    button = getCommand();
    int x;
    switch(button){
      case left:
        myDisplay.setDigit(timeEditLine[p] ,p ,editBackGroundColor);
        p--;
        p = max(p ,0);
        myDisplay.setDigit(timeEditLine[p] ,p ,editForeGroundColor);
      break;
      case right:
        myDisplay.setDigit(timeEditLine[p] ,p ,editBackGroundColor);
        p++;
        p = min(p, numDigits - 1);
        myDisplay.setDigit(timeEditLine[p] ,p ,editForeGroundColor);
      break;
      case up:
        x = timeEditLine[p];
        x++;
        timeEditLine[p] = min(x, timeEditLineMax[p]);
        myDisplay.setDigit(timeEditLine[p] ,p, editForeGroundColor);
      break;
      case down:
        x = timeEditLine[p];
        x--;
        timeEditLine[p] = max(x, 0);
        myDisplay.setDigit(timeEditLine[p] ,p ,editForeGroundColor);
      break;
    }
  };
   
  if (button == select){
    // set seconds first, see datasheet DS3231
    // write other data within 1 sec to prevent undesired rollover
    Clock.setSecond(timeEditLine[4]*10 + timeEditLine[5]); 
    Clock.setMinute(timeEditLine[2]*10 + timeEditLine[3]);
    Clock.setHour(min(23, timeEditLine[0]*10 + timeEditLine[1]));
  }
  
  showTime();
}

void showTime(){
  displaySeconds(Clock.getSecond());
  displayMinutes(Clock.getMinute());
  bool dontCare;
  displayHours(Clock.getHour(dontCare, dontCare));
}

void setSecondsOnlyDisplay(){
  hoursHighDigit = 4;
  hoursLowDigit = hoursHighDigit + 1;
  minutesHighDigit = 6;
  minutesLowDigit = minutesHighDigit +1;
  secondsHighDigit = 2;
  secondsLowDigit = secondsHighDigit + 1;
}

void setHoursAndMinutesDisplay(){
  hoursHighDigit = 0;
  hoursLowDigit = hoursHighDigit + 1;
  minutesHighDigit = 2;
  minutesLowDigit = minutesHighDigit +1;
  secondsHighDigit = 4;
  secondsLowDigit = secondsHighDigit + 1;
}

// recolor the active segments in a digit according a color scheme
void paintDigit(int digit, colorSchemeType scheme){
  if (digit < numDigits){
    for(int segment = 0; segment <7; segment ++){
      myDisplay.reColorSegment(digit, segment, getColorFromScheme(scheme)) ;
    }
  }
}
  
// display seconds, minutes, hours
// first in a fixed color
// the repaint the active segments according the set
// color scheme  
void displaySeconds(byte s){
  if (secondsLowDigit < numDigits){
     myDisplay.setDigit(s % 10, secondsLowDigit, veryVeryDarkGray);
  }
  if (secondsHighDigit < numDigits){
    myDisplay.setDigit(s / 10, secondsHighDigit, veryVeryDarkGray);
  }
  paintDigit(secondsHighDigit, secondsPaintScheme);
  paintDigit(secondsLowDigit, secondsPaintScheme);

}

void displayMinutes(byte m){
  if (minutesLowDigit < numDigits){
    myDisplay.setDigit(m % 10, minutesLowDigit, veryVeryDarkGray);
  }
  if (minutesHighDigit < numDigits){
    myDisplay.setDigit(m / 10, minutesHighDigit, veryVeryDarkGray);
  }
  paintDigit(minutesHighDigit, minutesPaintScheme);
  paintDigit(minutesLowDigit, minutesPaintScheme);

}

void displayHours(byte h){
  if (hoursLowDigit < numDigits){
    myDisplay.setDigit(h % 10, hoursLowDigit, veryVeryDarkGray);
  }
  if (hoursHighDigit < numDigits){
    myDisplay.setDigit(h / 10, hoursHighDigit, veryVeryDarkGray);
  }
  paintDigit(hoursHighDigit, hoursPaintScheme);
  paintDigit(hoursLowDigit, hoursPaintScheme);
}

