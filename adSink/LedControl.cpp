#include "LedControl.h"

// Constructor: store LED pin number
LedControl::LedControl(uint8_t pin)
: ledPin(pin)
{
}

// Initialize LED pin and ensure OFF state
void LedControl::init() {
    pinMode(ledPin, OUTPUT);
    off();
}

// Turn LED on continuously
void LedControl::on() {
    ledState = true;
    digitalWrite(ledPin, HIGH);
}

// Turn LED off continuously
void LedControl::off() {
    ledState = false;
    digitalWrite(ledPin, LOW);
}
