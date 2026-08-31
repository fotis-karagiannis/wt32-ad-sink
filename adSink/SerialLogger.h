#pragma once
#include <Arduino.h>

extern String logBuffer;
extern const size_t LOG_MAX;

class SerialLogger {
public:
    void begin(unsigned long baud);
    
    template<typename T>
    void print(const T &msg) {
        Serial.print(msg);
        appendToLog(String(msg));
    }

    template<typename T>
    void println(const T &msg) {
        Serial.println(msg);
        appendToLog(String(msg) + "\n");
    }

    void println();

    int printf(const char *fmt, ...);

private:
    void appendToLog(const String &msg);
};
