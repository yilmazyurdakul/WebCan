#pragma once
#include <Arduino.h>

class LedStatus {
public:
  explicit LedStatus(uint8_t pin = LED_BUILTIN)
      : ledPin(pin), blinkInterval(0), state(false), lastToggle(0) {}

  void begin() {
    pinMode(ledPin, OUTPUT);
    off();
  }

  void on() {
    blinkInterval = 0;
    digitalWrite(ledPin, HIGH);
    state = true;
  }

  void off() {
    blinkInterval = 0;
    digitalWrite(ledPin, LOW);
    state = false;
  }

  void blink(uint32_t intervalMs) {
    blinkInterval = intervalMs;
  }

  void update() {
    if (blinkInterval == 0) return; // solid state, nothing to do
    unsigned long now = millis();
    if (now - lastToggle >= blinkInterval) {
      state = !state;
      digitalWrite(ledPin, state);
      lastToggle = now;
    }
  }

private:
  uint8_t ledPin;
  uint32_t blinkInterval;
  bool state;
  unsigned long lastToggle;
};
