//#define ENC_DECODER (1 << 2)
#include <ClickEncoder.h>

#include <SPI.h>
#include <Wire.h>
#include <U8glib.h>
#include <TimerOne.h>
#include <MsTimer2.h>
#include <EEPROM.h>

#define OLED_CS    7
#define OLED_DC    8
#define OLED_RESET 9

U8GLIB_SSD1306_128X64 u8g(OLED_CS, OLED_DC, OLED_RESET);

ClickEncoder *encoder;
int16_t last, value;

boolean secPassed;

boolean inMenu = true;
char curMenuItem = 0;
boolean inPause = false;

char timePassed[13];

enum menuId {
  DRAWING = 0,
  WRITING = 1
};

char* drawingText = "Drawing";
char* writingText = "Writing";

void timer2Sec() {
  secPassed = true;
}

void timerIsr() {
  encoder->service();
}

char wDDallTime, wHHallTime, wMMallTime, wSSallTime;
char dDDallTime, dHHallTime, dMMallTime, dSSallTime;
char curDD, curHH, curMM, curSS;

void u8g_prepare_B12(void) {
  u8g.setFont(u8g_font_ncenB12);
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();
}

void u8g_prepare_B24(void) {
  u8g.setFont(u8g_font_timB24);
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();
}

void saveWritingTimings() {
  wSSallTime = wSSallTime + curSS;
  wMMallTime = wMMallTime + curMM;
  wHHallTime = wHHallTime + curHH;
  wDDallTime = wDDallTime + curDD;
  
  if(wSSallTime >= 60) {
    wSSallTime -= 60;
    wMMallTime++;
  }
  if(wMMallTime >= 60) {
    wMMallTime -= 60;
    wHHallTime++;
  }
  if(wHHallTime >= 24) {
    wHHallTime -= 24;
    wDDallTime++;
  }
  noInterrupts();
  EEPROM.write(0, wDDallTime);
  EEPROM.write(1, wHHallTime);
  EEPROM.write(2, wMMallTime);
  EEPROM.write(3, wSSallTime);
  interrupts();
}
void saveDrawingTimings() {
  noInterrupts();
  dSSallTime = dSSallTime + curSS;
  dMMallTime = dMMallTime + curMM;
  dHHallTime = dHHallTime + curHH;
  dDDallTime = dDDallTime + curDD;
  if(dSSallTime >= 60) {
    dSSallTime -= 60;
    dMMallTime++;
  }
  if(dMMallTime >= 60) {
    dMMallTime -= 60;
    dHHallTime++;
  }
    if(dHHallTime >= 24) {
    dHHallTime -= 24;
    dDDallTime++;
  }
  EEPROM.write(4, dDDallTime);
  EEPROM.write(5, dHHallTime);
  EEPROM.write(6, dMMallTime);
  EEPROM.write(7, dSSallTime);
  interrupts();
}

boolean resetTime = false;

void setTimingsToZero() {
  noInterrupts();
  EEPROM.write(0, 0);
  EEPROM.write(1, 0);
  EEPROM.write(2, 0);
  EEPROM.write(3, 0);
  EEPROM.write(4, 0);
  EEPROM.write(5, 0);
  EEPROM.write(6, 0);
  EEPROM.write(7, 0);
  interrupts();
}

void saveTimings() {
  if(curMenuItem == menuId::WRITING)
    saveWritingTimings();
  if(curMenuItem == menuId::DRAWING)
    saveDrawingTimings();
}

void readAllTimings() {
  readWritingTimings();
  readDrawingTimings();
}

void readWritingTimings() {
  noInterrupts();
  wDDallTime = EEPROM.read(0);
  wHHallTime = EEPROM.read(1);
  wMMallTime = EEPROM.read(2);
  wSSallTime = EEPROM.read(3);
  interrupts();
}

void readDrawingTimings() {
  noInterrupts();
  dDDallTime = EEPROM.read(4);
  dHHallTime = EEPROM.read(5);
  dMMallTime = EEPROM.read(6);
  dSSallTime = EEPROM.read(7);
  interrupts();
}

void setup() {
  readAllTimings();
  encoder = new ClickEncoder(A1, A0, A2, 4);
  
  Timer1.initialize(250);
  Timer1.attachInterrupt(timerIsr); 

  MsTimer2::set(1000, timer2Sec); // 1s period
  MsTimer2::start();
  
  last = 0;
  value = 0;
}

void draw() {
  u8g_prepare_B12();
  if(inMenu) {
    if (curMenuItem == menuId::DRAWING) {
      u8g.drawStr(30, 5, drawingText);
      sprintf(timePassed, "%d:%02d:%02d", dDDallTime, dHHallTime, dMMallTime);
      u8g.drawStr(30, 35, timePassed);
    } else if (curMenuItem == menuId::WRITING) {
      u8g.drawStr(30, 5, writingText);
      sprintf(timePassed, "%d:%02d:%02d", wDDallTime, wHHallTime, wMMallTime);
      u8g.drawStr(30, 35, timePassed);
    }
  } else {
      if (curMenuItem == menuId::DRAWING) {
      u8g.drawStr(30, 5, drawingText);
      sprintf(timePassed, "%02d:%02d:%02d", curHH, curMM, curSS);
      u8g_prepare_B24();
      u8g.drawStr(5, 30, timePassed);
    } else if (curMenuItem == menuId::WRITING) {
      u8g.drawStr(30, 5, writingText);
      sprintf(timePassed, "%02d:%02d:%02d", curHH, curMM, curSS);
      u8g_prepare_B24();
      u8g.drawStr(5, 30, timePassed);
    }
  }
}

void loop() {
start:
  u8g.firstPage();
  do {
    draw();
  } while( u8g.nextPage() );
  

  ClickEncoder::Button b;
  if(inMenu) {
    value += encoder->getValue();
    if (value != last) {
      resetTime = false;
      last = value;
      if (curMenuItem == menuId::DRAWING) {
        curMenuItem = menuId::WRITING;
       } else if (curMenuItem == menuId::WRITING) {
          curMenuItem = menuId::DRAWING;
      }
    }
    
    b = encoder->getButton();
    if (b != ClickEncoder::Open) {
      switch(b){
        case ClickEncoder::Clicked:
        inMenu = false;
        inPause = false;
        resetTime = false;
        curDD = 0; curHH = 0; curMM = 0; curSS = 0;
        break;
        case ClickEncoder::DoubleClicked:
        if (resetTime) {
          setTimingsToZero();
        }
        resetTime = true;
      }
    }
  } else { //inMenu = false
     b = encoder->getButton();
    if (b != ClickEncoder::Open) {
      switch(b){
        case ClickEncoder::Clicked:
          if (inPause) {
            inPause = false;
          } else {
            inPause = true;
            saveTimings();
          }
          break;
        case ClickEncoder::DoubleClicked:
          inMenu = true;
          saveTimings();
          readAllTimings();
          break;
      }
    }
    if(secPassed) {
      if (inPause)
        goto start;
      secPassed = false;
      curSS++;
      if(curSS >= 60) {
        curSS = 0;
        curMM++;
      }
      if(curMM >= 60) {
        curMM = 0;
        curHH++;
      }
      if(curHH >= 24) {
        curHH = 0;
        curDD++;        
      }
    }
  }
}
