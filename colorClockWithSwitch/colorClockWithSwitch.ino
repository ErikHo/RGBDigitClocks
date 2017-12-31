/*
Clock built with 4 or 6 RGBDigits (www.rgbdigit.com)
and DS3231 RTC module

(c) Erik Homburg 2017

Color schemes happily borrowed from www.rgbdigit.com

This software is free software

The clock is controlled with a rotary encoder with push button function

normal mode: 
rotate left/right: adjust display intensity
short press on the button: enter time set mode
time set mode:
rotate left/right with button not pressed: change digit position to change
rotate left/right with button pressed: change digit value
short press of button: set new time and leave edit mode
after 2 minutes without user action edit mode is left without change of time

*/

// #include <Wire.h> // already included in DS3231.h
#include <DS3231.h>
#include <RGBDigitV2.h>
#include <Encoder.h>
#include <Bounce2.h>
//#include <Adafruit_NeoPixel> // already called in RGBDigitV2.h

const byte digitsDataPin = 12;
const byte numDigits = 6;  // select 4 or 6
RGBDigit myDisplay(numDigits, digitsDataPin);

// Quadrature signals, use Encoder class
const byte S00 = 2;
const byte S90 = 3;
Encoder Knob(S00, S90);
int knobOldPosition;

// push button on encoder, use Bounce class
Bounce knobButton = Bounce();
const byte encoderButtonPin = 4;

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

// color schemes are used to make a vivid digit display
enum colorSchemeType{wild, earth, water, fire, air};
const colorSchemeType hoursPaintScheme = fire;
const colorSchemeType minutesPaintScheme = water;
const colorSchemeType secondsPaintScheme = air;

// every <refreshDivider> seconds the displayed digits are repainted
// with new colors according the selected color scheme
// an integer number of <refreshDivider>'s should fit exactly into 60 seconds
// possibilities enough!
int refreshDivider = 3;

byte brightnessIndex = 4;
// logarithmic distribution
const byte brightnessTable[] = {
  5, 6, 8, 10, 13, 16, 20, 26, 32, 40, 51, 64, 81, 102, 128
};
/*
// Full brightness table is limited to 15 entries in order to limit
// current consumption
const byte brightnessTable[] = {
  5, 6, 8, 10, 13, 16, 20, 26, 32, 40, 51, 64, 81, 102, 128, 161, 203, 255
};
measured average current draw with 4 digits connected
water water brightness 14 190mA
fire fire   brightness 14 170mA
air air     brightness 14 240mA
earth earth brightness 14 130mA
wild wild   brightness 14 240mA
fire water  brightness 14 200mA
*/

void setup() {
  
  Wire.begin();	// Start the I2C interface
	
	myDisplay.begin(); // start the displays and set brightness
  myDisplay.setBrightness(brightnessTable[brightnessIndex]);

  // the rotary encoder is initially used for controlling display 
  // brightness. Since this partcular encoder generates 4 counts
  // per mechanical click we divide encoder readings by 4.
  Knob.write( brightnessIndex * 4);
  knobOldPosition = Knob.read() / 4;

  // set debounce parameters for encoder push button
  pinMode(encoderButtonPin, INPUT_PULLUP);  
  knobButton.attach(encoderButtonPin);
  knobButton.interval(5); // debounce interval in ms

  // get the time from the Real Time Clock (DS3231) and display
  getAndShowTime(); 

}

// length of blinking seconds indicator
const unsigned long secondsPulseInterval = 500;
// previous states for state mechanism
unsigned long previousMillis = 0;
byte oldSec = 0;

void loop() {

  // dim seconds indicater as time has elapsed
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= secondsPulseInterval){
    previousMillis = currentMillis;
    myDisplay.clearDot(1);
    myDisplay.clearDot(3);
  }

  // find seconds boundary
  byte newSec = Clock.getSecond();
  if (newSec != oldSec){
    // new second detected
    oldSec = newSec;
    // only seconds display if 6 digits present
    if(numDigits == 6){
      displaySeconds(newSec);
    }
    
    // after <refreshDivider> seconds, display total time
    if ((newSec % refreshDivider) == 0){
      getAndShowTime();
    }
    // start display of seconds pulse
    previousMillis = currentMillis;
    myDisplay.setDot(1, dotColor);
    myDisplay.setDot(3, dotColor);
  };

  // handle brightness setting
  int knobPosition = Knob.read() / 4;
  if (knobPosition != knobOldPosition){
    // limit value to fit in array
    brightnessIndex = constrain(knobPosition, 0, sizeof(brightnessTable) - 1);
    myDisplay.setBrightness(brightnessTable[brightnessIndex]);
    // refresh display
    getAndShowTime();
  }
  knobOldPosition = knobPosition;

  // enter time set edit mode at press of rotary button
  knobButton.update();
  if(knobButton.fell()){
    setClock();
  }

}; // end of  main loop()


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

// setClock enables the user to set a desired time to the Real Time Clock
void setClock(){
  
  bool dontCare;  // arguments of getHour  are not important
  byte h = Clock.getHour(dontCare, dontCare);
  byte m = Clock.getMinute();
  
  // fill the edit line, seconds are set to zero
  char timeEditLine[7];
  timeEditLine[0] = h / 10;
  timeEditLine[1] = h % 10;
  timeEditLine[2] = m / 10;
  timeEditLine[3] = m % 10;
  timeEditLine[4] = 0;
  timeEditLine[5] = 0;

  int p = 0; // index to digit positions in the edit line
  // max values of posible digit values
  // note that hours can be up to 29, however at exiting
  // the function values too high are truncated to 23.
  byte timeEditLineMax[6] = {2, 9, 5, 9, 5 ,9};
  myDisplay.setText(timeEditLine, 0, numDigits, editBackGroundColor);
  myDisplay.setDot(1, editBackGroundColor);
  myDisplay.setDot(3, editBackGroundColor);
  
  // the time set edit mode was entered by pushing the button
  // wait for the button to be released
  knobButton.update();
  while(knobButton.read() == LOW){
    knobButton.update();
  };
  // if released display the active digit, initially p = 0
  myDisplay.setDigit(timeEditLine[p] ,p ,editForeGroundColor);


  // A key press shorter than 333 ms exits setClock
  const unsigned long exitWindowTimeLength = 333;
  // an idle time of 120000 ms (2 minutes) is seen as a cancel:
  // exit without update
  const unsigned long timeOutLength = 120000;

  // initialize edit loop variables
  // use the mechanisme as used in Arduino example "Blink without Delay"
  knobOldPosition = Knob.read() / 4;
  long currentTime = millis();
  unsigned long exitTimeWindowStart = currentTime;
  unsigned long timeOutStart = currentTime;
  bool exitTimeWindowActive = false;
  bool done = false;
  bool cancel = false;
  
  // begin of edit loop
  while(done == false && cancel == false){

    // read new states    
    currentTime = millis();
    knobButton.update();
    int knobPosition = Knob.read() / 4;
    
    // detect if knob is rotated
    if(knobPosition != knobOldPosition){
      int delta = knobPosition - knobOldPosition;
      // if knob is not pressed
      // move edit position
      if(knobButton.read() == HIGH){
        myDisplay.setDigit(timeEditLine[p] ,p ,editBackGroundColor);
        p = constrain(p + delta , 0, numDigits - 1);
        myDisplay.setDigit(timeEditLine[p] ,p ,editForeGroundColor);
      }
      else{
        // if knob is pressed change digit value
        int x = timeEditLine[p];
        timeEditLine[p] = constrain(x + delta, 0, timeEditLineMax[p]);
        myDisplay.setDigit(timeEditLine[p] ,p, editForeGroundColor);
      }
    }
    // update knob state
    knobOldPosition = knobPosition;
    
    // something has changed, so reset timeout
    if(knobButton.fell() == true || (knobButton.rose()) == true ||
        knobPosition != knobOldPosition){
      timeOutStart = currentTime;
    };

    // function is exited by a short push of the button
    // mark start time of exit window
    if(knobButton.fell()){
      exitTimeWindowStart = currentTime;
      exitTimeWindowActive = true;
    }

    // determine end of exit window, if end is reached exiting is 
    // not possible until the next button press
    if(currentTime - exitTimeWindowStart >= exitWindowTimeLength){
      exitTimeWindowStart = currentTime;
      exitTimeWindowActive = false;
    }
    
    // if knob is released within exit time window do exit
    if(knobButton.rose() == true && exitTimeWindowActive == true){
      done = true;
    }

    // if timeout is reached, exit without updating the real time clock
    if(currentTime - timeOutStart > timeOutLength){
      cancel = true;
    }
  }
  
  if (done == true){
    // set seconds first, see datasheet DS3231
    // write other data within 1 sec to prevent undesired rollover
    Clock.setSecond(timeEditLine[4]*10 + timeEditLine[5]); 
    Clock.setMinute(timeEditLine[2]*10 + timeEditLine[3]);
    // limit hours to 23
    Clock.setHour(min(23, timeEditLine[0]*10 + timeEditLine[1]));
  }
  getAndShowTime();
} // end of setClock

// get time from DS3231 RTC and display the time
void getAndShowTime(){
  // only show seconds if display digits are present
  if(numDigits == 6){
    displaySeconds(Clock.getSecond());
  };
  displayMinutes(Clock.getMinute());
  bool dontCare;
  displayHours(Clock.getHour(dontCare, dontCare));
}

// recolor the active segments in a digit according a color scheme
void paintDigit(int digit, colorSchemeType scheme){
  for(int segment = 0; segment <7; segment ++){
    myDisplay.reColorSegment(digit, segment, getColorFromScheme(scheme)) ;
  }
}

// display seconds, minutes, hours
// first in a fixed color
// the repaint the active segments according the set
// color scheme
void displaySeconds(byte s){
  myDisplay.setDigit(s / 10, 4, veryVeryDarkGray);
  myDisplay.setDigit(s % 10, 5, veryVeryDarkGray);
  paintDigit(4, secondsPaintScheme);
  paintDigit(5, secondsPaintScheme);
}

void displayMinutes(byte m){
  myDisplay.setDigit(m / 10, 2, veryVeryDarkGray);
  myDisplay.setDigit(m % 10, 3, veryVeryDarkGray);
  paintDigit(2, minutesPaintScheme);
  paintDigit(3, minutesPaintScheme);
}

void displayHours(byte h){
  myDisplay.setDigit(h / 10, 0, veryVeryDarkGray);
  myDisplay.setDigit(h % 10, 1, veryVeryDarkGray);
  paintDigit(0, hoursPaintScheme);
  paintDigit(1, hoursPaintScheme);
}
