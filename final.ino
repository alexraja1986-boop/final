/*****************************************************************
   PROJECT : Analog Clock + Alarm + Hourly Voice
   BOARD   : ESP32 (38 Pin)
   DISPLAY : ILI9341 240x320 SPI TFT
   RTC     : DS3231
   AUDIO   : DFPlayer Mini
   AUTHOR  : Clean Version
******************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <RTClib.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include <DFRobotDFPlayerMini.h>

#include <HardwareSerial.h>



//==============================================================
// TFT PIN CONNECTION
//==============================================================

#define TFT_CS      5
#define TFT_DC      2
#define TFT_RST     4

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);



//==============================================================
// RTC
//==============================================================

RTC_DS3231 rtc;



//==============================================================
// DFPLAYER
//==============================================================

HardwareSerial mp3Serial(2);

DFRobotDFPlayerMini player;



//==============================================================
// DFPLAYER PINS
//==============================================================

#define DF_RX 16
#define DF_TX 17



//==============================================================
// BUTTONS
//==============================================================

#define BTN_SET 32
#define BTN_UP  33



//==============================================================
// EEPROM
//==============================================================

#define EEPROM_SIZE 128



//==============================================================
// CLOCK POSITION
//==============================================================

const int CENTER_X = 120;
const int CENTER_Y = 120;
const int CLOCK_RADIUS = 100;



//==============================================================
// COLORS
//==============================================================

#define BLACK ILI9341_BLACK
#define WHITE ILI9341_WHITE
#define RED   ILI9341_RED
#define GREEN ILI9341_GREEN
#define BLUE  ILI9341_BLUE
#define CYAN  ILI9341_CYAN
#define YELLOW ILI9341_YELLOW
#define MAGENTA ILI9341_MAGENTA



//==============================================================
// TIME VARIABLES
//==============================================================

DateTime now;

int hour24;
int hour12;
int minuteNow;
int secondNow;

int dayNow;
int monthNow;
int yearNow;

String dayName;



//==============================================================
// TEMPERATURE
//==============================================================

float rtcTemp;



//==============================================================
// HOURLY VOICE
//==============================================================

bool hourlyPlayed = false;
int lastVoiceHour = -1;



//==============================================================
// ALARM
//==============================================================

#define TOTAL_ALARMS 10

struct AlarmData
{
  byte hour;
  byte minute;
  bool enable;
};

AlarmData alarm[TOTAL_ALARMS];

bool alarmPlaying = false;
int currentAlarm = -1;



//==============================================================
// DISPLAY UPDATE FLAGS
//==============================================================

bool redrawClock = true;

int oldSec = -1;
int oldMin = -1;
int oldHour = -1;



//==============================================================
// MENU
//==============================================================

byte menuPage = 0;
byte editItem = 0;
bool settingMode = false;



//==============================================================
// MILLIS TIMERS
//==============================================================

unsigned long rtcTimer = 0;
unsigned long displayTimer = 0;
unsigned long buttonTimer = 0;
unsigned long alarmTimer = 0;



//==============================================================
// FUNCTION PROTOTYPES
//==============================================================

void drawClockFace();
void drawHands();

void updateRTC();

void updateDisplay();

void checkButtons();

void menuSystem();

void loadEEPROM();

void saveEEPROM();

void hourlyVoice();

void checkAlarm();

void startAlarm();

void stopAlarm();

void drawDate();

void drawTemperature();



//==============================================================
// SETUP
//==============================================================

void setup()
{

  Serial.begin(115200);

  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);

  EEPROM.begin(EEPROM_SIZE);



  //---------------- RTC ----------------

  Wire.begin();

  if (!rtc.begin())
  {
    Serial.println("RTC ERROR");
    while (1);
  }



  //---------------- TFT ----------------

  tft.begin();

  tft.setRotation(1);

  tft.fillScreen(BLACK);



  //---------------- DFPLAYER ----------------

  mp3Serial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);

  if (player.begin(mp3Serial))
  {
    Serial.println("DFPlayer OK");

    player.volume(25);
  }
  else
  {
    Serial.println("DFPlayer ERROR");
  }



  //---------------- EEPROM ----------------

  loadEEPROM();



  //---------------- SCREEN ----------------

  drawClockFace();

}



//==============================================================
// LOOP
//==============================================================

void loop()
{

  updateRTC();

  checkButtons();

  menuSystem();

  hourlyVoice();

  checkAlarm();

  updateDisplay();

}
/*****************************************************************
    PART 2 : ANALOG CLOCK
*****************************************************************/

#include <math.h>

#define DEG2RAD 0.0174532925

//---------------------------------------------------------------
// Draw Clock Face
//---------------------------------------------------------------
void drawClockFace()
{
  tft.fillScreen(BLACK);

  // Outer circle
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS, WHITE);
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS - 1, WHITE);

  // Hour marks
  for (int i = 0; i < 60; i++)
  {
    float angle = (i * 6 - 90) * DEG2RAD;

    int x1 = CENTER_X + cos(angle) * (CLOCK_RADIUS - 4);
    int y1 = CENTER_Y + sin(angle) * (CLOCK_RADIUS - 4);

    int len = (i % 5 == 0) ? 12 : 5;

    int x2 = CENTER_X + cos(angle) * (CLOCK_RADIUS - len);
    int y2 = CENTER_Y + sin(angle) * (CLOCK_RADIUS - len);

    if (i % 5 == 0)
      tft.drawLine(x1, y1, x2, y2, YELLOW);
    else
      tft.drawPixel(x2, y2, WHITE);
  }

  //---------------- Numbers ----------------

  tft.setTextColor(CYAN, BLACK);
  tft.setTextSize(2);

  const char *num[12] =
  {
    "12","1","2","3","4","5",
    "6","7","8","9","10","11"
  };

  for (int i = 0; i < 12; i++)
  {
    float angle = (i * 30 - 90) * DEG2RAD;

    int x = CENTER_X + cos(angle) * (CLOCK_RADIUS - 25);
    int y = CENTER_Y + sin(angle) * (CLOCK_RADIUS - 25);

    tft.setCursor(x - 8, y - 8);
    tft.print(num[i]);
  }

  //---------------- Center ----------------

  tft.fillCircle(CENTER_X, CENTER_Y, 4, RED);

  redrawClock = false;
}



//---------------------------------------------------------------
// Draw Single Hand
//---------------------------------------------------------------
void drawHand(
  float angle,
  int length,
  uint16_t color,
  int thick)
{
  float a = (angle - 90) * DEG2RAD;

  int x = CENTER_X + cos(a) * length;
  int y = CENTER_Y + sin(a) * length;

  for (int i = -thick; i <= thick; i++)
  {
    tft.drawLine(
      CENTER_X + i,
      CENTER_Y,
      x + i,
      y,
      color);
  }
}



//---------------------------------------------------------------
// Draw Hands
//---------------------------------------------------------------
void drawHands()
{
  //---------------- Erase old hands ----------------

  if (oldSec >= 0)
  {
    float secAngle = oldSec * 6;

    drawHand(secAngle, 85, BLACK, 0);

    float minAngle = oldMin * 6 + oldSec * 0.1;

    drawHand(minAngle, 70, BLACK, 1);

    float hrAngle =
      (oldHour % 12) * 30 +
      oldMin * 0.5;

    drawHand(hrAngle, 50, BLACK, 2);

    // Redraw center
    tft.fillCircle(CENTER_X, CENTER_Y, 4, RED);
  }

  //---------------- Hour ----------------

  float hourAngle =
      (hour24 % 12) * 30 +
      minuteNow * 0.5;

  drawHand(hourAngle, 50, GREEN, 2);

  //---------------- Minute ----------------

  float minuteAngle =
      minuteNow * 6 +
      secondNow * 0.1;

  drawHand(minuteAngle, 70, WHITE, 1);

  //---------------- Second ----------------

  float secondAngle =
      secondNow * 6;

  drawHand(secondAngle, 85, RED, 0);

  //---------------- Center ----------------

  tft.fillCircle(CENTER_X, CENTER_Y, 4, RED);

  oldHour = hour24;
  oldMin = minuteNow;
  oldSec = secondNow;
}



//---------------------------------------------------------------
// Update Display
//---------------------------------------------------------------
void updateDisplay()
{
  if (redrawClock)
      drawClockFace();

  if (millis() - displayTimer >= 100)
  {
    displayTimer = millis();

    if (oldSec != secondNow)
    {
      drawHands();

      drawDate();

      drawTemperature();
    }
  }
}
/*****************************************************************
    PART 3 : RTC + DATE + TEMPERATURE
*****************************************************************/

//---------------------------------------------------------------
// Day Names
//---------------------------------------------------------------
const char *weekDays[] =
{
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};


//---------------------------------------------------------------
// Read RTC
//---------------------------------------------------------------
void updateRTC()
{
  // Update every 200ms
  if (millis() - rtcTimer < 200)
    return;

  rtcTimer = millis();

  now = rtc.now();

  hour24 = now.hour();
  minuteNow = now.minute();
  secondNow = now.second();

  dayNow = now.day();
  monthNow = now.month();
  yearNow = now.year();

  rtcTemp = rtc.getTemperature();

  dayName = weekDays[now.dayOfTheWeek()];
}


//---------------------------------------------------------------
// Draw Date
//---------------------------------------------------------------
void drawDate()
{
  static int oldDay = -1;
  static int oldMonth = -1;
  static int oldYear = -1;

  if (oldDay == dayNow &&
      oldMonth == monthNow &&
      oldYear == yearNow)
    return;

  oldDay = dayNow;
  oldMonth = monthNow;
  oldYear = yearNow;

  tft.fillRect(0, 245, 320, 22, BLACK);

  tft.setTextColor(YELLOW, BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 248);

  if(dayNow < 10) tft.print("0");
  tft.print(dayNow);

  tft.print("/");

  if(monthNow < 10) tft.print("0");
  tft.print(monthNow);

  tft.print("/");

  tft.print(yearNow);
}


//---------------------------------------------------------------
// Draw Day
//---------------------------------------------------------------
void drawDay()
{
  static String oldDayName = "";

  if(oldDayName == dayName)
      return;

  oldDayName = dayName;

  tft.fillRect(0, 270, 320, 22, BLACK);

  tft.setTextColor(CYAN, BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 273);

  tft.print(dayName);
}


//---------------------------------------------------------------
// Draw Temperature
//---------------------------------------------------------------
void drawTemperature()
{
  static int oldTemp = -100;

  int temp = (int)rtcTemp;

  if(temp == oldTemp)
      return;

  oldTemp = temp;

  tft.fillRect(210, 245, 110, 22, BLACK);

  tft.setCursor(215,248);

  tft.setTextColor(GREEN, BLACK);
  tft.setTextSize(2);

  tft.print(temp);
  tft.print((char)247);   // Degree symbol
  tft.print("C");
}


//---------------------------------------------------------------
// Draw Digital Time (Optional)
//---------------------------------------------------------------
void drawDigitalTime()
{
  static int oldSecond = -1;

  if(oldSecond == secondNow)
      return;

  oldSecond = secondNow;

  tft.fillRect(40, 205, 170, 28, BLACK);

  tft.setCursor(45,210);

  tft.setTextSize(3);

  tft.setTextColor(WHITE, BLACK);

  if(hour24 < 10) tft.print("0");
  tft.print(hour24);

  tft.print(":");

  if(minuteNow < 10) tft.print("0");
  tft.print(minuteNow);

  tft.print(":");

  if(secondNow < 10) tft.print("0");
  tft.print(secondNow);
}
/*****************************************************************
    PART 4 : DFPLAYER HOURLY VOICE
*****************************************************************/

//------------------------------------------------------------
// Voice Files
//
// 06:00 -> 0001.mp3
// 07:00 -> 0002.mp3
// ...
// 21:00 -> 0016.mp3
//------------------------------------------------------------

const uint8_t hourlyVoiceFile[] =
{
  1,   //06
  2,   //07
  3,   //08
  4,   //09
  5,   //10
  6,   //11
  7,   //12
  8,   //13
  9,   //14
  10,  //15
  11,  //16
  12,  //17
  13,  //18
  14,  //19
  15,  //20
  16   //21
};

bool hourlyBusy = false;


//------------------------------------------------------------
// Play Hourly Voice
//------------------------------------------------------------
void playHourlyVoice(uint8_t fileNumber)
{
    Serial.print("Hourly Voice : ");
    Serial.println(fileNumber);

    player.stop();
    delay(150);

    player.play(fileNumber);

    hourlyBusy = true;
}


//------------------------------------------------------------
// Hourly Voice Manager
//------------------------------------------------------------
void hourlyVoice()
{
    // Alarm is playing -> do not speak hourly voice
    if (alarmPlaying)
        return;

    // Valid hours : 6AM to 9PM
    if (hour24 < 6 || hour24 > 21)
        return;

    // Only during first 5 seconds
    if (minuteNow != 0)
        return;

    if (secondNow > 5)
        return;

    // Already spoken this hour
    if (lastVoiceHour == hour24)
        return;

    uint8_t index = hour24 - 6;

    playHourlyVoice(hourlyVoiceFile[index]);

    lastVoiceHour = hour24;
}


//------------------------------------------------------------
// Reset for next hour
//------------------------------------------------------------
void resetHourlyVoice()
{
    if (minuteNow == 1)
    {
        hourlyBusy = false;
    }
}
/*****************************************************************
    PART 5 : 10 ALARM + EEPROM
*****************************************************************/

#define ALARM_FILE 20

//----------------------------------------------------
// Load EEPROM
//----------------------------------------------------
void loadEEPROM()
{
    for (int i = 0; i < TOTAL_ALARMS; i++)
    {
        int addr = i * 3;

        alarm[i].hour   = EEPROM.read(addr);
        alarm[i].minute = EEPROM.read(addr + 1);
        alarm[i].enable = EEPROM.read(addr + 2);

        if (alarm[i].hour > 23) alarm[i].hour = 6;
        if (alarm[i].minute > 59) alarm[i].minute = 0;

        if (alarm[i].enable > 1)
            alarm[i].enable = false;
    }
}

//----------------------------------------------------
// Save EEPROM
//----------------------------------------------------
void saveEEPROM()
{
    for (int i = 0; i < TOTAL_ALARMS; i++)
    {
        int addr = i * 3;

        EEPROM.write(addr, alarm[i].hour);
        EEPROM.write(addr + 1, alarm[i].minute);
        EEPROM.write(addr + 2, alarm[i].enable);
    }

    EEPROM.commit();
}

//----------------------------------------------------
// Check Alarm
//----------------------------------------------------
void checkAlarm()
{
    if (alarmPlaying)
        return;

    for (int i = 0; i < TOTAL_ALARMS; i++)
    {
        if (!alarm[i].enable)
            continue;

        if (hour24 == alarm[i].hour &&
            minuteNow == alarm[i].minute &&
            secondNow == 0)
        {
            currentAlarm = i;

            startAlarm();

            break;
        }
    }
}

//----------------------------------------------------
// Start Alarm
//----------------------------------------------------
void startAlarm()
{
    Serial.print("Alarm ");
    Serial.print(currentAlarm + 1);
    Serial.println(" Triggered");

    player.stop();
    delay(100);

    player.loop(ALARM_FILE);      // Repeat 0020.mp3

    alarmPlaying = true;
}

//----------------------------------------------------
// Stop Alarm
//----------------------------------------------------
void stopAlarm()
{
    player.stop();

    alarmPlaying = false;

    currentAlarm = -1;

    Serial.println("Alarm Stopped");
}

//----------------------------------------------------
// Stop Alarm by Button
//----------------------------------------------------
void checkAlarmButtons()
{
    if (!alarmPlaying)
        return;

    if (digitalRead(BTN_SET) == LOW)
    {
        delay(150);

        stopAlarm();
    }

    if (digitalRead(BTN_UP) == LOW)
    {
        delay(150);

        stopAlarm();
    }
}