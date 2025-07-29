#include <Arduino.h>
#include "driver/adc.h"
#include "soc/gpio_struct.h"

const uint8_t ignitionTable[11][11] = {
//RPM0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10 K
  {  1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },//  0%
  { 10,12,14,16,18,20,20,20,20,20,20 },// 10%
  { 10,13,15,17,19,21,21,21,21,21,21 },// 20%
  { 10,14,16,18,20,22,22,22,22,22,22 },// 30%
  { 10,15,17,19,21,23,23,23,23,23,23 },// 40%
  { 10,16,18,20,22,24,24,24,24,24,24 },// 50%
  { 10,17,19,21,23,25,25,25,25,25,25 },// 60%
  { 10,18,20,22,24,26,26,26,26,26,26 },// 70%
  { 10,19,21,23,25,27,27,27,27,27,27 },// 80%
  { 10,20,22,24,26,28,28,28,28,28,28 },// 90%
  { 10,21,23,25,27,29,29,29,29,29,29 },//100% TPS
};


// === 全域變數區 ===
unsigned long now;
unsigned long PrintTime = 0, PollingTime = 0;

// 時間記錄（單位：微秒）
uint64_t PositiveTime = 0, NegativeTime = 0, BossPassingTime = 0;
int PositiveValue = 0, NegativeValue = 0;

// 觸發狀態
bool PositiveTriggered = false, NegativeTriggered = false;

// 油門計算相關
int ThrottleValue = 0, Throttle = 0;

// 轉速計算相關
float CycleTime = 0.0f, Hz = 0.0f;
int RPM = 0;

// === 點火控制狀態 ===
bool IgnitionReady = false;                     // 準備點火旗標
bool IgnitionDelay = false;                     // 點火延遲中
bool IgnitionDuration = false;                  // 點火持續中
unsigned long IgnitionDelayTime = 0;            // 延遲時間
unsigned long IgnitionDelayStartTime = 0;       // 延遲起始時間
unsigned long IgnitionDurationTime = 1000;      // 點火持續時間 (us)
unsigned long IgnitionDurationStartTime = 0;    // 點火開始時間
int IgnitionAngle = 0;                          // 點火角度 (degrees)


// 感測器相關
int EngineTempValue = 0, BatteryVoltageValue = 0, AutoChokeValue = 0;
float EngineTemp = 0.0f, BatteryVoltage = 0.0f;
bool AutoChoke = false;

void setup() {
  Serial.begin(2000000); // 初始化串口通信速率

  // 設定 ADC 精度與每個通道的電壓範圍
  adc1_config_width(ADC_WIDTH_BIT_9);  // 0~511
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);  // GPIO36
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_0);  // GPIO39
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // GPIO34 (Throttle)
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); // GPIO35 (EngineTemp)
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); // GPIO32 (BatteryVoltage)
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11); // GPIO33 (AutoChoke)
  gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);

}

void loop() {
  // === 邊緣偵測 ===
  PositiveValue = adc1_get_raw(ADC1_CHANNEL_0); // GPIO36
  if (PositiveValue > 100 && !PositiveTriggered) {
    PositiveTime = esp_timer_get_time();
    PositiveTriggered = true;
    if(IgnitionReady==true){
      IgnitionDelayStartTime = PositiveTime; 
      IgnitionReady=false;
      IgnitionDelay=true;
    }
  }

  if(IgnitionDelay == true){
    now = esp_timer_get_time();
    if(now-IgnitionDelayStartTime >= IgnitionDelayTime){
      GPIO.out_w1ts = (1 << 25);
      IgnitionDurationStartTime = now;
      IgnitionDelayStartTime=now;
      IgnitionDuration= true;
      IgnitionDelay = false;

    }
  }

  if(IgnitionDuration == true){
    now = esp_timer_get_time();
    if(now-IgnitionDurationStartTime >= IgnitionDurationTime){
      GPIO.out_w1tc = (1 << 25);
      IgnitionDuration= false;
    }
  }

  NegativeValue = adc1_get_raw(ADC1_CHANNEL_3); // GPIO39
  if (NegativeValue > 100 && !NegativeTriggered && PositiveTriggered) {
    NegativeTime = esp_timer_get_time();
    NegativeTriggered = true;
    if(IgnitionReady==true){
      IgnitionDuration=true;
      GPIO.out_w1ts = (1 << 25);
      IgnitionDurationStartTime = now;
    }
  }




  // === 油門百分比 ===
  ThrottleValue = adc1_get_raw(ADC1_CHANNEL_6);  // GPIO34
  Throttle = (ThrottleValue < 40) ? 100 :
             (ThrottleValue > 505) ? 0 :
             (100 * (505 - ThrottleValue)) / (505 - 40);

  // === RPM 計算 ===
  if (NegativeTriggered && PositiveTriggered) {
    BossPassingTime = NegativeTime - PositiveTime;
    CycleTime = BossPassingTime / 0.1741f; // CycleTime 單位：微秒 (μs)，一圈 360 度
    Hz = 1.0f / (CycleTime / 1000000.0f);
    RPM = Hz * 60;
    NegativeTriggered = false;
    PositiveTriggered = false;

    IgnitionAngle = ignitionTable[
      (Throttle < 0) ? 0 : (Throttle > 100) ? 10 : Throttle / 10]
      [(RPM < 0) ? 0 : (RPM > 10000) ? 10 : RPM / 1000];
    if(IgnitionAngle==0){
      IgnitionReady=false;
      IgnitionDurationTime=0;
    }else{
    IgnitionDelayTime = (CycleTime * IgnitionAngle) / 360;
    IgnitionDurationTime = constrain(map(RPM, 0, 10000, 2500, 1000), 1000, 2500);
    IgnitionReady=true;
    }
  }

  // === 資料輸出與更新 ===
  now = esp_timer_get_time();
  if (now - PrintTime >= 100000 && !IgnitionDelay && !IgnitionDuration) {
    PrintTime = now;

    if (now - PollingTime >= 1000000) {
      PollingTime = now;

      EngineTempValue = adc1_get_raw(ADC1_CHANNEL_7); 
      EngineTemp = (EngineTempValue / 511.0f) * 120.0f;

      BatteryVoltageValue = adc1_get_raw(ADC1_CHANNEL_4);
      BatteryVoltage = (BatteryVoltageValue / 511.0f) * 16.0f;

      AutoChokeValue = adc1_get_raw(ADC1_CHANNEL_5);
      AutoChoke = (AutoChokeValue > 250);

      Serial.printf("R:%d T:%d IA:%d ID:%d ET:%.1f BV:%.2f AC:%d\n",
                    RPM, Throttle,IgnitionAngle,IgnitionDurationTime, EngineTemp, BatteryVoltage, AutoChoke ? 1 : 0);

    } else {
      Serial.printf("R:%d T:%d IA:%d ID:%d\n", RPM, Throttle,IgnitionAngle,IgnitionDurationTime);
    }
  }
}
