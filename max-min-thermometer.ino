
/*
*/

#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//Initialise the LCD
LiquidCrystal_I2C      lcd(0x27, 20, 4);
Adafruit_BME280 bme; // I2C

unsigned long delayTime;
float preciseTemp;
int currentTemp;
int currentHumidity;
int currentPressure;
int currentMax;
int currentMin;
int hourMax[24];
int hourMin[24];
int dayMax[7];
int dayMin[7];
long barometer[36];  // 6 hours @ 10 minute intervals
char trend03;
char trend36;

unsigned long currentHourOffset;
int currentHour;
unsigned long current10MinOffset;
unsigned long millisInHour = 3600000l; //1000 * 60 * 60;
unsigned long millisIn10Min = 600000l;

//two chars (with arrow head)
//left:
uint8_t r1[8] = {0x0,0x0,0x0,0x1,0x2,0x4,0x8};
uint8_t s1[8] = {0x0,0x0,0x0,0xf,0x0,0x0,0x0};
uint8_t f1[8] = {0x8,0x4,0x2,0x1,0x0,0x0,0x0};
//right:
uint8_t r2[8] = {0xe,0x6,0xa,0x10,0x0,0x0,0x0};
uint8_t s2[8] = {0x0,0x4,0x2,0x1f,0x2,0x4,0x0};
uint8_t f2[8] = {0x0,0x0,0x0,0x10,0xa,0x6,0xe};
//storm (>4mb fall in 3hr)
uint8_t storm[8] = {0x10,0x18,0x8,0xd,0x5,0x3,0xf};

char row1[20];
char row2[20];
char row3[20];
char row4[20];

void setup() {
    Serial.begin(9600);
    Serial.println(F("BME280 test"));

    bool status;
    
    // default settings
    status = bme.begin(0x76);  
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }
    
    delayTime = 10000; // 10 seconds

    lcd.init();                      // initialize the lcd 
    lcd.createChar(1, f1);
    lcd.createChar(2, s1);
    lcd.createChar(3, r1);
    lcd.createChar(4, f2);
    lcd.createChar(5, s2);
    lcd.createChar(6, r2);
    lcd.createChar(7, storm);
    //lcd.backlight();
    currentHourOffset = millis();
    currentHour = 0;

    // initialise arrays
    readSensor();  // gets the currentTemp
    currentMax = currentTemp;
    currentMin = currentTemp;
    int ii;
    for(ii=0; ii<24; ii++) {
      hourMax[ii] = currentTemp;
      hourMin[ii] = currentTemp; 
    }
    for(ii=0; ii<7; ii++) {
      dayMax[ii] = currentTemp;
      dayMin[ii] = currentTemp; 
    }
    long precisePressure = (long)bme.readPressure();
    for(ii=0; ii<36; ii++) {
      barometer[ii] = precisePressure;
    }

    trend03 = 5;
    trend36 = 2;
 }

void loop() { 
   readSensor();

    // row 1: display current temperature, pressure, humidity
    char tempBuf[6];
    dtostrf(preciseTemp, 5, 1, tempBuf);
    sprintf(row1, "%5s%cC  %4u%c%c %3u%%", tempBuf, 223, currentPressure, trend36, trend03, currentHumidity);
    lcd.setCursor(0,0);
    lcd.print(row1);

    // row 2: display headers
    sprintf(row2, "      24h  3d   7d  ");
    lcd.setCursor(0,1);
    lcd.print(row2);

    // row 3: display min temperatures
    int min24hr = arrayMin(hourMin, 24);
    int min3day = arrayMin(dayMin, 3);
    int min7day = arrayMin(dayMin, 7);
    sprintf(row3, "Min: %3d  %3d  %3d  ", min24hr, min3day, min7day);
    lcd.setCursor(0, 2);
    lcd.print(row3);
    
    // row 4: display max temperatures
    int max24hr = arrayMax(hourMax, 24);
    int max3day = arrayMax(dayMax, 3);
    int max7day = arrayMax(dayMax, 7);
    sprintf(row4, "Max: %3d  %3d  %3d  ", max24hr, max3day, max7day);
    lcd.setCursor(0, 3);
    lcd.print(row4);
    
    updateMaxMin();
    checkInterval();
    
    //debug();

    delay(delayTime);
}

void readSensor() {
    preciseTemp = bme.readTemperature();
    currentTemp = (int) (preciseTemp + 0.5); // TODO -0.5 if < 0       
    currentHumidity = (int)(bme.readHumidity());
    currentPressure = (int)(bme.readPressure() / 100.0F);
}

void updateMaxMin() {
  if(currentTemp > currentMax) {
    currentMax = currentTemp;
    if(currentMax > hourMax[0]) {
      hourMax[0] = currentMax;
      if(currentMax > dayMax[0]) {
        dayMax[0] = currentMax;
      }
    }
  }
  if(currentTemp < currentMin) {
    currentMin = currentTemp;
    if(currentMin < hourMin[0]) {
      hourMin[0] = currentMin;
      if(currentMin < dayMin[0]) {
        dayMin[0] = currentMin;
      }
    }
  }
}

void checkInterval() {
  unsigned long now = millis();
  if((now - currentHourOffset) > millisInHour) {
    currentHourOffset = now;
    incrementHour();
    currentMax = currentTemp;
    currentMin = currentTemp;
  }
  if((now - current10MinOffset) > millisIn10Min) {
    current10MinOffset = now;
    incrementBarometer();
  }
}

void incrementHour() {
  currentHour++;
  if(currentHour >= 23) {
    incrementDay();
    currentHour = 0;
  }
  // shift the hour arrays along one place each
  int ii;
  for(ii=23; ii>=1; ii--) {
    hourMax[ii] = hourMax[ii-1];
    hourMin[ii] = hourMin[ii-1];
  }
  hourMax[0] = currentMax;
  hourMin[0] = currentMin;
}

void incrementDay() {
  // shift the day arrays along one place each
  int ii;
  for(ii=7; ii>=1; ii--) {
    dayMax[ii] = dayMax[ii-1];
    dayMin[ii] = dayMin[ii-1];
  }
  dayMax[0] = arrayMax(hourMax, 24);
  dayMin[0] = arrayMin(hourMin, 24);
}

void incrementBarometer() {
  int ii;
  for(ii=36; ii>=1; ii--) {
    barometer[ii] = barometer[ii-1];
  }
  barometer[0] = (int)bme.readPressure();
  // update 0-3 hr trend
  trend03 = 5 + getTrend(barometer[0] - barometer[18]);
  // update 3-6 hr trend
  trend36 = 2 + getTrend(barometer[18] - barometer[36]);
}

int getTrend(int diff) {
  if(diff > 200) { // rising pressure
    return 1;
  } else if(diff < 200) { // falling pressure
    return -1;
  } else {
    return 0;
  }
}

int arrayMax(int *arr, int len) {
  int m = -100;
  for(int ii = 0; ii < len; ii++) {
    if(arr[ii] > m) {
      m = arr[ii];
    }
  }
  return m;
}

int arrayMin(int *arr, int len) {
  int m = 100;
  for(int ii = 0; ii < len; ii++) {
    if(arr[ii] < m) {
      m = arr[ii];
    }
  }
  return m;
}

void debug() {
    Serial.print("Current = ");
    Serial.println(currentTemp);
    Serial.print("max = ");
    Serial.println(currentMax);
    Serial.print("min = ");
    Serial.println(currentMin);
    Serial.print("max24 = ");
    Serial.println(arrayMax(hourMax, 24));
    Serial.print("min24 = ");
    Serial.println(arrayMin(hourMin, 24));
    
    int ii;
    for(ii=0; ii<24; ii++) {
      Serial.print(hourMax[ii]);
      Serial.print(", ");
    }
    Serial.println("");
    for(ii=0; ii<36; ii++) {
      Serial.print(barometer[ii]);
      Serial.print(", ");
    }
    Serial.println("");
}
