#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "ConnectionManager.h"
#include "BloomFilter.h"
#include "BlocklistManager.h"
#include "SerialLogger.h"
#include "DnsEngine.h"
#include "LedControl.h"

// Global objects
SerialLogger LogSerial;
ConnectionManager connectionManager;
LedControl statusLed(14);

BloomFilter filter(320000, 6);
BlocklistManager blocklist(filter);

DnsEngine dnsEngine;

bool filterLoaded = false;

// Helper functions 
// Mount LittleFS; format automatically if the filesystem is corrupted or unreadable
void initStorage() {
    if (!LittleFS.begin()) {
        LogSerial.println("[AdSink] LittleFS mount failed, formatting...");
        LittleFS.format();  // Format only if mounting fails
    } else {
        LogSerial.println("[AdSink] LittleFS mounted");
    }
}

// Retrieve saved blocklist URL from NVS (does not load filter data)
void loadBlocklistURL() {
    Preferences prefs;
    prefs.begin("adsink", false);

    // Read saved URL (empty if none saved)
    String blocklistURL = prefs.getString("blocklist_url", "");
    prefs.end();

    if (blocklistURL.length() > 0) {
        LogSerial.println("[AdSink] Saved blocklist URL: " + blocklistURL);
        blocklist.setURL(blocklistURL);  // Apply saved URL
    }
}

// Load Bloom filter from LittleFS if filter.bin exists
void loadFilter() {
    // Check if filter file exists before loading
    if (LittleFS.exists("/filter.bin")) {
        LogSerial.println("[AdSink] Loading existing filter...");

        // Attempt to load filter from file
        if (filter.load(LittleFS, "/filter.bin")) {
            filterLoaded = true;  // Mark filter as loaded
        }
    }
}

// Process DNS queries only when STA or Ethernet is active (AP mode uses captive portal DNS)
void handleDnsSinkhole() {
    bool dnsRunning = (WiFi.getMode() == WIFI_STA) || ETH.linkUp();

    // Skip sinkhole DNS when in AP mode
    if (!dnsRunning) return;

    // Check for incoming DNS packets
    int packetSize = dnsEngine.dnsUdp.parsePacket();
    if (packetSize > 0) {
        dnsEngine.handleQuery();  // Process DNS query
    }
}

// LED reflects basic connectivity (Wi-Fi or Ethernet), not protection state
void updateStatusLed() {
    // LED ON when Wi-Fi connected or Ethernet has valid IP
    if ((ETH.linkUp() && (ETH.localIP() != IPAddress(0,0,0,0))) ||
        (WiFi.status() == WL_CONNECTED)) {
        statusLed.on();
    } 
    else {
        statusLed.off();  // LED OFF when no connectivity
    }
}

// Initialize logging, storage, network stack, DNS engine, and load saved data
void setup() {
    LogSerial.begin(115200);
    delay(200);

    statusLed.init();
    statusLed.off();  // Ensure LED starts OFF

    LogSerial.println();
    LogSerial.println("[AdSink] Booting...");

    initStorage();        // Mount filesystem
    loadBlocklistURL();   // Load saved blocklist URL
    loadFilter();         // Load Bloom filter if present

    connectionManager.begin();  // Initialize Wi-Fi/Ethernet/AP

    dnsEngine.setFilter(&filter);  // Attach filter to DNS engine
    dnsEngine.begin(53);           // Start DNS server

    LogSerial.println("[AdSink] DNS engine initialized.");
}

// Handle AP DNS, web server requests, sinkhole DNS, and LED status updates
void loop() {
    // Captive portal DNS in AP mode
    if (connectionManager.isAPMode()) {
        connectionManager.getDNSServer().processNextRequest();
    }

    // Handle HTTP requests
    connectionManager.getWebServer().handleClient();

    // DNS sinkhole (STA/Ethernet only)
    handleDnsSinkhole();

    // Update LED based on connectivity
    updateStatusLed();
}
