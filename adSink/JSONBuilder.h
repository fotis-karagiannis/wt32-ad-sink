#pragma once
#include <Arduino.h>

class JSONBuilder {
public:
    JSONBuilder();
    void add(const String& key, const String& value);
    void add(const String& key, const char* value);
    void add(const String& key, bool value);
    void add(const String& key, int value);
    void add(const String& key, unsigned long value);

    String str() const;

private:
    String json;
    bool first;

    void appendKey(const String& key);
    String escape(const String& s) const;
};
