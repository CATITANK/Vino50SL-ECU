#include <Arduino.h>
#include "driver/adc.h"
#include "soc/gpio_struct.h"

const uint8_t ignitionTable[11][11] = {
  {  1, 1, 5,10,15,20,20,20,20,20,20 },
  { 10,12,14,16,18,20,20,20,20,20,20 },
  { 10,13,15,17,19,21,21,21,21,21,21 },
  { 10,14,16,18,20,22,22,22,22,22,22 },
  { 10,15,17,19,21,23,23,23,23,23,23 },
  { 10,16,18,20,22,24,24,24,24,24,24 },
  { 10,17,19,21,23,25,25,25,25,25,25 },
  { 10,18,20,22,24,26,26,26,26,24,22 },
  { 10,19,21,23,25,27,27,27,26,24,22 },
  { 10,20,22,24,26,28,28,28,26,24,22 },
  { 10,21,23,25,27,29,29,29,26,24,22 },
};

const int DwellTimeTable[11] = {
  1090, 1180, 1270, 1360, 1450, 1540, 1630, 1720, 1810, 1900, 2000
};

// 腳位定義
const int IgnitionAnglePin = 32;
const int RPMPin = 33;
const int DwellTimePin = 25;
const int TPSPin = 26;
const int ThrottlePin = 36;
const int IgnitionPin = 4;

// 全域變數
int RPMNow = 2000;
int RPMGoal = 2000;
int RPMValue = 0;
int ThrottleValue = 0;
int Throttle = 0;
int TPSValue = 0;
int IgnitionAngle = 0;
int IgnitionValue = 0;
int DwellTimeValue = 0;

int64_t now = 0;
int64_t StepTrackingTime = 0;
int64_t LastExecutionTime = 0;
int64_t DwellStart = 0;

void setup() {
  pinMode(IgnitionAnglePin, OUTPUT);
  pinMode(RPMPin, OUTPUT);
  pinMode(DwellTimePin, OUTPUT);
  pinMode(TPSPin, OUTPUT);
  pinMode(ThrottlePin, INPUT);
  gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT); 
  Serial.begin(1000000); // 偵錯用
}

void loop() {
  now = esp_timer_get_time(); // 微秒

  // 讀油門 & TPS 輸出
  ThrottleValue = analogRead(ThrottlePin);
  Throttle = map(ThrottleValue, 0, 4095, 0, 100);
  TPSValue = map(Throttle, 0, 100, 0, 250);
  analogWrite(TPSPin, TPSValue);

  // RPM 目標值
  RPMGoal = map(Throttle, 0, 100, 2000, 10000);
  if (RPMNow != RPMGoal && (now - StepTrackingTime >= 500)) {
    StepTrackingTime = now;
    RPMNow += (RPMGoal > RPMNow) ? 1 : -1;
  }

  // PWM 輸出 RPM
  RPMValue = map(RPMNow, 2000, 10000, 46, 250);
  analogWrite(RPMPin, RPMValue);

  // 查表
  int ThrottleIndex = constrain(Throttle / 10, 0, 10);
  int RPMIndex = constrain(RPMNow / 1000, 0, 10);
  IgnitionAngle = ignitionTable[ThrottleIndex][RPMIndex];
  IgnitionValue = map(IgnitionAngle, 0, 30, 0, 250);
  analogWrite(IgnitionAnglePin, IgnitionValue);

  int DwellTime = DwellTimeTable[RPMIndex];
  DwellTimeValue = map(DwellTime, 0, 2500, 0, 250);
  analogWrite(DwellTimePin, DwellTimeValue);


  double TimePerRevolution = (60.0 / RPMNow) * 1000000.0;

  if (now - LastExecutionTime >= TimePerRevolution) {
    LastExecutionTime = now;
    DwellStart = now;
    GPIO.out_w1ts = (1 << 4); 
    Serial.printf("S\n");
  }

  if (now - DwellStart >= DwellTime) {
    GPIO.out_w1tc = (1 << 4); 
    // Serial.printf("c\n");
  }

  // Serial.printf("Throttle=%d  RPM=%d  Ign=%d  Dwell=%d\n", Throttle, RPMNow, IgnitionAngle, DwellTime);
}
