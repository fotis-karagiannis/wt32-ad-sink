#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ETH.h>
#include "BloomFilter.h"

class DnsEngine {
public:
    DnsEngine();
    bool begin(uint16_t listenPort = 53);
    void setFilter(BloomFilter* f);
    void setUpstream(IPAddress ip, uint16_t port = 53);

    WiFiUDP dnsUdp;
    void handleQuery();
    uint32_t totalQueries = 0;
    uint32_t blockedQueries = 0;
    String lastBlocked = "";

private:
    WiFiUDP upstreamUdp;
    uint16_t listenPort;
    IPAddress upstreamIP;
    uint16_t upstreamPort;
    BloomFilter* filter;

    bool parseQuestion(const uint8_t* buf, size_t len,
                       size_t& offset, String& qname,
                       uint16_t& qtype, uint16_t& qclass);

    void buildBlockedResponse(const uint8_t* query, size_t queryLen,
                              uint8_t* outBuf, size_t& outLen,
                              IPAddress blockedIP);

    bool forwardToUpstream(const uint8_t* query, size_t queryLen,
                           uint8_t* response, size_t& respLen,
                           uint16_t timeoutMs = 1500);
};
