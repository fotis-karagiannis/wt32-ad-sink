#include "BlocklistManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "SerialLogger.h"

extern SerialLogger LogSerial;

// Limits and thresholds for blocklist handling
#define MAX_DOMAINS       50000   // Hard cap on domains added to the filter
#define WARN_AT           40000   // Threshold to warn about very large lists
#define SUCCESS_THRESHOLD 5000    // If stall happens but >= this many domains, treat as success

// Constructor
BlocklistManager::BlocklistManager(BloomFilter &filter)
: filter(filter), downloading(false), lastError("") {}

// Getter: return current blocklist URL
const String &BlocklistManager::getURL() const {
    return blocklistURL;
}

// Getter: return whether a download is currently in progress
bool BlocklistManager::isDownloading() const {
    return downloading;
}

// Set the blocklist URL (stored, not validated here)
void BlocklistManager::setURL(const String &url) {
    blocklistURL = url;
}

// Get last error/warning message
String BlocklistManager::getLastError() const {
    return lastError;
}

// Helper: sanitize a domain token in-place
//
// - Strips inline comments starting with '#'
// - Trims trailing spaces, tabs, and carriage returns
// - Leaves the cleaned domain string in hostToken
void BlocklistManager::sanitizeDomain(char *hostToken) {
    if (!hostToken) return;

    // Remove inline comments
    char *hash = strchr(hostToken, '#');
    if (hash) {
        *hash = 0;
    }

    // Trim trailing whitespace
    int len = strlen(hostToken);
    while (len > 0 && (hostToken[len - 1] == ' ' ||
                       hostToken[len - 1] == '\t' ||
                       hostToken[len - 1] == '\r')) {
        hostToken[--len] = 0;
    }
}

// Helper: parse a single hosts-style line into a domain
//
// Expected formats:
//   "0.0.0.0 example.com"
//   "127.0.0.1 ads.example.org # comment"
//   "0.0.0.0    tracker.example.net"
//
// Behavior:
//   - Skips empty lines and comment-only lines
//   - Skips lines without a valid host token
//   - Sanitizes the host token (inline comments, trailing whitespace)
//   - Returns true and sets outDomain if a valid domain is found
bool BlocklistManager::parseLine(char *buf, String &outDomain) {
    if (!buf) return false;

    // Skip leading whitespace
    char *line = buf;
    while (*line == ' ' || *line == '\t' || *line == '\r') {
        line++;
    }

    // Ignore empty or comment-only lines
    if (*line == 0 || *line == '#') {
        return false;
    }

    // Tokenize: first token is IP, second is host
    char *saveptr = nullptr;
    char *ipToken = strtok_r(line, " \t", &saveptr);
    if (!ipToken) {
        return false;
    }

    char *hostToken = strtok_r(nullptr, " \t", &saveptr);
    if (!hostToken) {
        return false;
    }

    // If host starts with '#', it's effectively a comment
    if (hostToken[0] == '#') {
        return false;
    }

    // Clean up the host token
    sanitizeDomain(hostToken);

    // Ensure we still have something meaningful
    int len = strlen(hostToken);
    if (len <= 0) {
        return false;
    }

    outDomain = String(hostToken);
    return true;
}

// Download a blocklist from the configured URL and build the Bloom filter.
//
// - Enforces global and stall timeouts
// - Parses hosts-style lines
// - Applies domain count limits and warnings
// - Saves the resulting filter to the given filesystem path
//
// Returns:
//   true  -> filter saved successfully
//   false -> failure (network, parsing, save error, etc.)
bool BlocklistManager::downloadAndBuildFilter(fs::FS &fs, const char *path) {
    lastError = "";

    // Preconditions
    if (blocklistURL.length() == 0) {
        lastError = "No URL set";
        return false;
    }
    if (downloading) {
        lastError = "Already downloading";
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        lastError = "WiFi not connected";
        return false;
    }

    downloading = true;
    LogSerial.println("[Blocklist] Downloading: " + blocklistURL);

    HTTPClient http;

    // prevent uint16 overflow
    http.setTimeout(60000); // 60 seconds

    if (!http.begin(blocklistURL)) {
        lastError = "HTTP begin failed";
        downloading = false;
        return false;
    }

    int code = http.GET();
    if (code != 200) {
        lastError = "HTTP GET failed (" + String(code) + ")";
        http.end();
        downloading = false;
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();

    filter.clear();
    LogSerial.println("[Blocklist] Cleared filter.");

    char buf[512];
    int idx = 0;

    unsigned long added = 0;
    unsigned long lines = 0;

    const unsigned long GLOBAL_TIMEOUT = 90000;
    const unsigned long STALL_TIMEOUT  = 10000;

    unsigned long startTime    = millis();
    unsigned long lastProgress = millis();

    bool stalled = false;

    // Main streaming loop
    while (true) {
        unsigned long now = millis();

        if (now - startTime > GLOBAL_TIMEOUT) {
            lastError = "Download exceeded 90s limit";
            LogSerial.println("[Blocklist] Global timeout.");
            break;
        }

        int available = stream->available();

        if (available > 0) {
            char c = stream->read();

            if (c == '\n') {
                buf[idx] = 0;
                idx = 0;
                lines++;

                String domain;
                if (parseLine(buf, domain)) {
                    added++;

                    if (added == WARN_AT) {
                        lastError = "Large blocklist (>40k)";
                    }

                    if (added > MAX_DOMAINS) {
                        lastError = "Trimmed to 50k";
                        LogSerial.println("[Blocklist] Trim limit reached.");
                        break;
                    }

                    filter.add(domain);
                }

                lastProgress = now;

            } else {
                if (idx < (int)sizeof(buf) - 1) {
                    buf[idx++] = c;
                } else {
                    idx = 0;
                }

                lastProgress = now;
            }

        } else {
            if (now - lastProgress > STALL_TIMEOUT) {
                LogSerial.println("[Blocklist] Stall timeout (no new data).");
                stalled = true;
                break;
            }

            if (!http.connected() && stream->available() == 0) {
                LogSerial.println("[Blocklist] Stream ended normally.");
                break;
            }

            delay(1);
        }
    }

    http.end();

    LogSerial.printf("[Blocklist] Added %lu domains\n", added);

    // Final status evaluation
    if (stalled && added >= SUCCESS_THRESHOLD) {
        LogSerial.println("[Blocklist] Stall occurred AFTER full file. Treating as success.");
        lastError = "OK";
    }
    else if (lastError == "") {
        if (added > MAX_DOMAINS) {
            lastError = "Trimmed to 50k";
        } else if (added >= WARN_AT) {
            lastError = "Large blocklist (>40k)";
        } else if (added == 0) {
            lastError = "Empty or invalid blocklist";
        } else {
            lastError = "OK";
        }
    }

    // prevent snprintf truncation
    bool saved = filter.save(fs, path);

    downloading = false;
    return saved;
}
