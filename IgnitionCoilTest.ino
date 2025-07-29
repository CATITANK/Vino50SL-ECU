  #include <Arduino.h>

const int bjtout = 16;

void setup() {
  pinMode(bjtout, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(bjtout, HIGH);
  digitalWrite(LED_BUILTIN, LOW);
  delayMicroseconds(1000);
  digitalWrite(bjtout, LOW);
  digitalWrite(LED_BUILTIN, HIGH);
  delayMicroseconds(10000);
}
