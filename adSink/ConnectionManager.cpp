#include "ConnectionManager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <time.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <ETH.h>

#include "Webpages.h"
#include "BlocklistManager.h"
#include "BloomFilter.h"
#include "SerialLogger.h"
#include "DnsEngine.h"
#include "JSONBuilder.h"

#define DNS_PORT 53

extern bool filterLoaded;
extern BlocklistManager blocklist;
extern BloomFilter filter;
extern SerialLogger LogSerial;
extern DnsEngine dnsEngine;

// Helper: protection status 
static bool isProtectionActive() {
    bool wifiConnected = (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED);
    bool ethConnected  = ETH.linkUp() && (ETH.localIP() != IPAddress(0,0,0,0));

    if (!filterLoaded) return false;
    if (!wifiConnected && !ethConnected) return false;

    return true;
}

// Constructor
ConnectionManager::ConnectionManager()
: apMode(false), webServer(80) {}

// Redirect helper
void ConnectionManager::redirect(const String& url) {
    webServer.sendHeader("Location", url, true);
    webServer.send(302, "text/plain", "");
}


// Begin: Ethernet first, then Wi-Fi
void ConnectionManager::begin() {
    LogSerial.println("[ETH] Trying Ethernet first...");

    ETH.begin(
        ETH_PHY_LAN8720,
        1,
        23,
        18,
        16,
        ETH_CLOCK_GPIO0_IN
    );

    delay(200);

    bool ethLinked = false;
    unsigned long start = millis();
    while (millis() - start < 1500) {
        if (ETH.linkUp()) {
            ethLinked = true;
            break;
        }
        delay(100);
    }

    if (!ethLinked) {
        LogSerial.println("[ETH] No Ethernet link detected. Disabling PHY and falling back to WiFi.");
        pinMode(16, OUTPUT);
        digitalWrite(16, LOW);
        delay(100);
    } else {
        LogSerial.println("[ETH] Ethernet link detected. Keeping PHY enabled.");
    }

    startSTA();
}


// Start Access Point mode
void ConnectionManager::startAP() {
    apMode = true;

    WiFi.disconnect(true, true);
    delay(200);

    WiFi.mode(WIFI_AP);
    delay(200);

    WiFi.softAPConfig(
        IPAddress(192,168,4,1),
        IPAddress(192,168,4,1),
        IPAddress(255,255,255,0)
    );

    WiFi.softAP("AdSink-Setup");
    delay(300);

    IPAddress ip = WiFi.softAPIP();

    dnsServer.start(DNS_PORT, "*", ip);

    // Captive portal redirects
    webServer.on("/generate_204", HTTP_GET, [this]() {
        redirect("http://192.168.4.1/");
    });
    webServer.on("/fwlink", HTTP_GET, [this]() {
        redirect("http://192.168.4.1/");
    });
    webServer.on("/hotspot-detect.html", HTTP_GET, [this]() {
        redirect("http://192.168.4.1/");
    });

    // WiFi scan
    webServer.on("/scan", HTTP_GET, [this]() {
        int n = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "\"" + WiFi.SSID(i) + "\"";
        }
        json += "]";
        webServer.send(200, "application/json", json);
    });

    webServer.on("/", HTTP_GET, std::bind(&ConnectionManager::handleRoot, this));
    webServer.on("/save", HTTP_POST, std::bind(&ConnectionManager::handleSave, this));

    webServer.begin();
}


// Start Station mode
void ConnectionManager::startSTA() {
    apMode = false;

    Preferences prefs;
    prefs.begin("adsink", false);
    wifiSSID = prefs.getString("wifi_ssid", "");
    wifiPassword = prefs.getString("wifi_pass", "");
    prefs.end();

    if (wifiSSID.length() == 0) {
        LogSerial.println("[WiFi] No stored credentials, starting AP.");
        startAP();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    LogSerial.println("[WiFi] Connecting to STA...");

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        LogSerial.print(".");
    }
    LogSerial.println();

    if (WiFi.status() == WL_CONNECTED) {
        LogSerial.print("[WiFi] Connected. IP: ");
        LogSerial.println(WiFi.localIP().toString());

        if (!MDNS.begin("adsink")) {
            LogSerial.println("mDNS error");
        } else {
            MDNS.addService("http", "tcp", 80);
            LogSerial.println("[mDNS] http://adsink.local");
        }

        // JSONBuilder Dashboard Endpoints 
        webServer.on("/dashboard_data", HTTP_GET, [this]() {
            unsigned long ms  = millis();
            unsigned long sec = ms / 1000;
            unsigned long min = sec / 60;
            unsigned long hr  = min / 60;

            char uptimeStr[64];
            snprintf(uptimeStr, sizeof(uptimeStr), "%lu hr %lu min %lu sec",
                     hr, min % 60, sec % 60);

            size_t heap    = ESP.getFreeHeap();
            size_t minheap = ESP.getMinFreeHeap();

            String wifiSsid = WiFi.SSID();
            int    wifiRssi = WiFi.RSSI();

            bool protection = isProtectionActive();

            bool   ethConnected = ETH.linkUp() && (ETH.localIP() != IPAddress(0,0,0,0));
            String ethIp        = ethConnected ? ETH.localIP().toString() : "";
            int    ethSpeed     = ethConnected ? ETH.linkSpeed() : 0;

            JSONBuilder jb;
            jb.add("uptime", String(uptimeStr));
            jb.add("filter_loaded", filterLoaded);
            jb.add("heap", (unsigned long)heap);
            jb.add("minheap", (unsigned long)minheap);
            jb.add("wifi_ssid", wifiSsid);
            jb.add("wifi_rssi", wifiRssi);
            jb.add("protection_active", protection);
            jb.add("eth_connected", ethConnected);
            jb.add("eth_ip", ethIp);
            jb.add("eth_speed", ethSpeed);

            webServer.send(200, "application/json", jb.str());
        });

        webServer.on("/dnsstats_data", HTTP_GET, [this]() {
            int pctInt = 0;
            if (dnsEngine.totalQueries > 0) {
                float pct = (100.0f * dnsEngine.blockedQueries) / dnsEngine.totalQueries;
                pctInt = (int)(pct + 0.5f); // rounded integer percent
            }

            JSONBuilder jb;
            jb.add("total",   (unsigned long)dnsEngine.totalQueries);
            jb.add("blocked", (unsigned long)dnsEngine.blockedQueries);
            jb.add("percent", pctInt);                 
            jb.add("last_blocked", dnsEngine.lastBlocked);

            webServer.send(200, "application/json", jb.str());
        });

        webServer.on("/blocklist_status", HTTP_GET, [this]() {
            Preferences prefs;
            prefs.begin("adsink", false);

            String last = prefs.getString("last_update", "");
            String url  = prefs.getString("blocklist_url", "");

            prefs.end();

            if (last.length() == 0) last = "Never";

            JSONBuilder jb;
            jb.add("filter_loaded", filterLoaded);
            jb.add("last_update", last);
            jb.add("url", url);
            jb.add("downloading", blocklist.isDownloading());

            webServer.send(200, "application/json", jb.str());
        });

        // HTML routes
        // Root redirect
        webServer.on("/", HTTP_GET, [this]() {
            redirect("/dashboard");
        });

        // Dashboard page
        webServer.on("/dashboard", HTTP_GET, [this]() {
            String page = FPSTR(DASHBOARD_NAV);
            page += FPSTR(DASHBOARD_HTML);
            webServer.send(200, "text/html", page);
        });

        // Logs page
        webServer.on("/logs", HTTP_GET, [this]() {
            String page = FPSTR(DASHBOARD_NAV);
            page += FPSTR(LOGS_HTML);
            webServer.send(200, "text/html", page);
        });

        // DNS stats page
        webServer.on("/dnsstats", HTTP_GET, [this]() {
            String page = FPSTR(DASHBOARD_NAV);
            page += FPSTR(DNSSTATS_HTML);
            webServer.send(200, "text/html", page);
        });

        // Settings page
        webServer.on("/settings", HTTP_GET, [this]() {
            String page = FPSTR(DASHBOARD_NAV);
            page += FPSTR(SETTINGS_HTML);
            webServer.send(200, "text/html", page);
        });

        // Functional endpoints
        // Logs data
        webServer.on("/logs_data", HTTP_GET, [this]() {
            extern String logBuffer;
            webServer.send(200, "text/plain", logBuffer);
        });

        // Save blocklist URL
        webServer.on("/save_blocklist_url", HTTP_POST, [this]() {
            if (webServer.hasArg("url")) {
                String url = webServer.arg("url");

                Preferences prefs;
                prefs.begin("adsink", false);
                prefs.putString("blocklist_url", url);
                prefs.end();

                blocklist.setURL(url);

                LogSerial.println("[Settings] Saved blocklist URL: " + url);

                redirect("/settings");
            } else {
                webServer.send(400, "text/plain", "Missing url");
            }
        });

        // Download blocklist
        webServer.on("/download_blocklist", HTTP_GET, [this]() {
            LogSerial.println("[HTTP] /download_blocklist triggered.");

            bool ok = blocklist.downloadAndBuildFilter(LittleFS, "/filter.bin");
            filterLoaded = ok;

            String msg = blocklist.getLastError();
            if (msg.length() == 0) msg = ok ? "OK" : "FAIL";

            LogSerial.printf("[HTTP] /download_blocklist result: %s\n", msg.c_str());

            Preferences prefs;
            prefs.begin("adsink", false);

            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                char formatted[32];
                snprintf(formatted, sizeof(formatted),
                         "%02d/%02d/%04d - %02d:%02d",
                         timeinfo.tm_mday,
                         timeinfo.tm_mon + 1,
                         timeinfo.tm_year + 1900,
                         timeinfo.tm_hour,
                         timeinfo.tm_min);

                prefs.putString("last_update", formatted);
            } else {
                prefs.putString("last_update", "Unknown");
            }

            prefs.end();

            webServer.send(200, "text/plain", msg);
        });

        // Test domain
        webServer.on("/test_domain", HTTP_GET, [this]() {
            if (!webServer.hasArg("d")) {
                webServer.send(400, "text/plain", "Missing domain");
                return;
            }

            String d = webServer.arg("d");
            d.trim();

            if (d.length() == 0) {
                webServer.send(400, "text/plain", "Invalid domain");
                return;
            }

            bool blocked = filter.possiblyContains(d);
            webServer.send(200, "text/plain", blocked ? "BLOCKED" : "ALLOWED");
        });

        // Debug NVS
        webServer.on("/debug_nvs", HTTP_GET, [this]() {
            LogSerial.println("=== NVS KEYS IN 'adsink' ===");

            nvs_iterator_t it = nullptr;
            esp_err_t res = nvs_entry_find(NVS_DEFAULT_PART_NAME, "adsink", NVS_TYPE_ANY, &it);

            if (res != ESP_OK || it == nullptr) {
                LogSerial.println("No entries found or error.");
            }

            while (res == ESP_OK && it != nullptr) {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info);

                LogSerial.printf("Key: %s, Type: %d\n", info.key, info.type);

                res = nvs_entry_next(&it);
            }

            webServer.send(200, "text/plain", "OK");
        });

        // Reset device
        webServer.on("/reset_device", HTTP_GET, [this]() {
            LogSerial.println("[Settings] Reset device requested: clearing WiFi credentials and restarting.");

            Preferences prefs;
            prefs.begin("adsink", false);
            prefs.remove("wifi_ssid");
            prefs.remove("wifi_pass");
            prefs.end();

            webServer.send(200, "text/plain", "OK");

            delay(500);
            ESP.restart();
        });

        // ------------------------------

        webServer.begin();
        LogSerial.println("[HTTP] WebServer started.");

    } else {
        LogSerial.println("[WiFi] STA failed, starting AP.");
        startAP();
    }
}

// AP Root Page
void ConnectionManager::handleRoot() {
    if (!apMode) {
        webServer.send(200, "text/plain", "Device is connected to WiFi.");
        return;
    }

    String page = FPSTR(INDEX_HTML_TEMPLATE);
    webServer.send(200, "text/html", page);
}


// Save WiFi Credentials
void ConnectionManager::handleSave() {
    if (webServer.hasArg("ssid") && webServer.hasArg("pass")) {
        String ssid = webServer.arg("ssid");
        String pass = webServer.arg("pass");

        Preferences prefs;
        prefs.begin("adsink", false);
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", pass);
        prefs.end();

        webServer.send(200, "text/html", FPSTR(SAVED_HTML));
        delay(1000);
        ESP.restart();
    } else {
        webServer.send(400, "text/plain", "Missing ssid/pass");
    }
}


// Getters
bool ConnectionManager::isAPMode() const {
    return apMode;
}

DNSServer& ConnectionManager::getDNSServer() {
    return dnsServer;
}

WebServer& ConnectionManager::getWebServer() {
    return webServer;
}
