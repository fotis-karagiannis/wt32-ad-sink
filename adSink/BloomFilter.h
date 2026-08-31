#pragma once
#include <Arduino.h>
#include <FS.h>

class BloomFilter {
public:
    BloomFilter(uint32_t bitCount, uint8_t hashCount);
    ~BloomFilter();
    void clear();
    void add(const String &value);
    bool possiblyContains(const String &value) const;

    bool save(fs::FS &fs, const char *path) const;
    bool load(fs::FS &fs, const char *path);

private:
    uint32_t bitCount;
    uint8_t hashCount;
    uint8_t *bits;

    void setBit(uint32_t index);
    bool getBit(uint32_t index) const;
    uint32_t hash(const String &value, uint32_t seed) const;
    uint32_t computeSeed(uint8_t i) const;
};
