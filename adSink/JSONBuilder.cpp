#include "JSONBuilder.h"

// Constructor: initialize empty JSON object
JSONBuilder::JSONBuilder() {
    json = "{";
    first = true;
}

// Append key prefix, handling comma placement
void JSONBuilder::appendKey(const String& key) {
    if (!first) {
        json += ",";
    }
    first = false;

    json += "\"";
    json += key;
    json += "\":";
}

// Escape special characters inside JSON strings
String JSONBuilder::escape(const String& s) const {
    String out;
    out.reserve(s.length());

    for (char c : s) {
        if (c == '\"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else {
            out += c;
        }
    }

    return out;
}

// Add string value
void JSONBuilder::add(const String& key, const String& value) {
    appendKey(key);
    json += "\"";
    json += escape(value);
    json += "\"";
}

// Add C-string value
void JSONBuilder::add(const String& key, const char* value) {
    appendKey(key);
    json += "\"";
    json += escape(String(value));
    json += "\"";
}

// Add boolean value
void JSONBuilder::add(const String& key, bool value) {
    appendKey(key);
    json += (value ? "true" : "false");
}

// Add integer value
void JSONBuilder::add(const String& key, int value) {
    appendKey(key);
    json += String(value);
}

// Add unsigned long value
void JSONBuilder::add(const String& key, unsigned long value) {
    appendKey(key);
    json += String(value);
}

// Finalize JSON object and return as String
String JSONBuilder::str() const {
    return json + "}";
}
