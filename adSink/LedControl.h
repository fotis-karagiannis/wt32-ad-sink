#pragma once
#include <Arduino.h>

class LedControl {
public:
    LedControl(uint8_t pin);
    void init();
    void on();
    void off();

private:
    uint8_t ledPin;
    bool ledState = false;
};
