#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ETH.h>

class ConnectionManager {
public:
    ConnectionManager();
    void begin();
    bool isAPMode() const;

    DNSServer& getDNSServer();
    WebServer& getWebServer();
    void redirect(const String& url);

private:
    void startAP();
    void startSTA();
    void handleRoot();
    void handleSave();

    bool apMode = false;
    DNSServer dnsServer;
    WebServer webServer;
    String wifiSSID;
    String wifiPassword;
};
