#include "SerialLogger.h"

// Global circular log buffer
String logBuffer = "";
const size_t LOG_MAX = 8000; // 8 KB buffer

// Initialize hardware Serial at the given baud rate
void SerialLogger::begin(unsigned long baud) {
    Serial.begin(baud);
}

// Append text to the circular log buffer
void SerialLogger::appendToLog(const String &msg) {
    logBuffer += msg;

    // Trim from the front if exceeding max size
    if (logBuffer.length() > LOG_MAX) {
        logBuffer.remove(0, logBuffer.length() - LOG_MAX);
    }
}

// Print formatted text to Serial and log buffer
int SerialLogger::printf(const char *fmt, ...) {
    char buf[256];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial.print(buf);
    appendToLog(String(buf));

    return len;
}

// println() with no arguments
void SerialLogger::println() {
    Serial.println();
    appendToLog("\n");
}
