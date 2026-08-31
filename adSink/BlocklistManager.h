#pragma once
#include <Arduino.h>
#include <FS.h>
#include "BloomFilter.h"

class BlocklistManager {
public:
    BlocklistManager(BloomFilter &filter);

    void setURL(const String &url);
    const String &getURL() const;
    bool isDownloading() const;

    bool downloadAndBuildFilter(fs::FS &fs, const char *path);

    String getLastError() const;

private:
    BloomFilter &filter;
    String blocklistURL;

    bool downloading;
    String lastError;

    static void sanitizeDomain(char *hostToken);
    static bool parseLine(char *buf, String &outDomain);
};
