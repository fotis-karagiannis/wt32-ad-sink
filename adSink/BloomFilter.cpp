#include "BloomFilter.h"
#include <Arduino.h>
#include <FS.h>

// Constructor: allocate bit array and initialize to zero
BloomFilter::BloomFilter(uint32_t bitCount, uint8_t hashCount)
    : bitCount(bitCount), hashCount(hashCount)
{
    bits = new uint8_t[(bitCount + 7) / 8];
    clear();
}

// Destructor: free allocated bit array
BloomFilter::~BloomFilter() {
    delete[] bits;
}

// Clear all bits in the filter
void BloomFilter::clear() {
    memset(bits, 0, (bitCount + 7) / 8);
}

// Set a specific bit in the filter
void BloomFilter::setBit(uint32_t index) {
    bits[index >> 3] |= (1 << (index & 7));
}

// Read a specific bit from the filter
bool BloomFilter::getBit(uint32_t index) const {
    return bits[index >> 3] & (1 << (index & 7));
}

// Hash function: simple rolling hash with seed variation
uint32_t BloomFilter::hash(const String &value, uint32_t seed) const {
    uint32_t h = seed;

    for (size_t i = 0; i < value.length(); i++) {
        h = (h * 131) + value[i];
    }

    return h % bitCount;
}

// Ccompute hash seed for hash index i
uint32_t BloomFilter::computeSeed(uint8_t i) const {
    return 0x9747b28c + (uint32_t)i * 0x12345;
}

// Add a value to the filter using multiple hash functions
void BloomFilter::add(const String &value) {
    for (uint8_t i = 0; i < hashCount; i++) {
        uint32_t h = hash(value, computeSeed(i));
        setBit(h);
    }
}

// Check if a value may be in the filter
bool BloomFilter::possiblyContains(const String &value) const {
    for (uint8_t i = 0; i < hashCount; i++) {
        uint32_t h = hash(value, computeSeed(i));
        if (!getBit(h)) return false;
    }
    return true;
}

// Save filter bit array to filesystem
bool BloomFilter::save(fs::FS &fs, const char *path) const {
    File f = fs.open(path, "w");
    if (!f) return false;

    size_t size = (bitCount + 7) / 8;
    size_t written = f.write(bits, size);
    f.close();

    return (written == size);
}

// Load filter bit array from filesystem
bool BloomFilter::load(fs::FS &fs, const char *path) {
    File f = fs.open(path, "r");
    if (!f) return false;

    size_t size = (bitCount + 7) / 8;

    if (f.size() != (int)size) {
        Serial.printf("[BloomFilter] load(): size mismatch. Expected %u, got %d\n",
                      (unsigned)size, (int)f.size());
        f.close();
        return false;
    }

    size_t readBytes = f.read(bits, size);
    f.close();

    bool ok = (readBytes == size);

    Serial.printf("[BloomFilter] load(): read %u bytes, ok=%s\n",
                  (unsigned)readBytes, ok ? "true" : "false");

    return ok;
}
