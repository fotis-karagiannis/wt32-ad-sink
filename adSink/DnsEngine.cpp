#include "DnsEngine.h"
#include "SerialLogger.h"

extern SerialLogger LogSerial;
extern BloomFilter filter;

// Constructor: initialize defaults for DNS engine
DnsEngine::DnsEngine()
: listenPort(53),
  upstreamIP(IPAddress(1,1,1,1)),
  upstreamPort(53),
  filter(nullptr)
{
}

// Start DNS engine on the specified port
bool DnsEngine::begin(uint16_t port) {
    listenPort = port;

    LogSerial.printf("[DNS] Starting DNS engine on port %u\n", listenPort);

    // Bind UDP socket for incoming DNS queries
    if (!dnsUdp.begin(listenPort)) {
        LogSerial.println("[DNS] Failed to bind UDP port");
        return false;
    }

    // Upstream UDP socket (ephemeral port)
    upstreamUdp.begin(0);

    LogSerial.printf("[DNS] Upstream DNS set to %s:%u\n",
                     upstreamIP.toString().c_str(), upstreamPort);

    return true;
}

// Assign Bloom filter pointer (moved from header)
void DnsEngine::setFilter(BloomFilter* f) {
    filter = f;
}

// Set upstream DNS server (moved from header)
void DnsEngine::setUpstream(IPAddress ip, uint16_t port) {
    upstreamIP = ip;
    upstreamPort = port;
}

// Read a DNS label sequence and append to qname
// Returns false on malformed data
static bool readDnsLabels(const uint8_t* buf, size_t len,
                          size_t& offset, String& qname)
{
    qname = "";

    while (offset < len) {
        uint8_t labellen = buf[offset++];

        // Zero-length label = end of name
        if (labellen == 0) break;

        // Compression not supported here
        if (labellen & 0xC0) return false;

        // Bounds check
        if (offset + labellen > len) return false;

        if (qname.length() > 0) qname += ".";

        for (uint8_t i = 0; i < labellen; i++) {
            qname += (char)buf[offset + i];
        }

        offset += labellen;
    }

    return true;
}

// Parse DNS question section: qname, qtype, qclass
bool DnsEngine::parseQuestion(const uint8_t* buf, size_t len,
                              size_t& offset, String& qname,
                              uint16_t& qtype, uint16_t& qclass)
{
    // Read domain name labels
    if (!readDnsLabels(buf, len, offset, qname)) {
        return false;
    }

    // Ensure enough bytes for QTYPE + QCLASS
    if (offset + 4 > len) return false;

    qtype  = (buf[offset] << 8) | buf[offset + 1];
    qclass = (buf[offset + 2] << 8) | buf[offset + 3];
    offset += 4;

    return true;
}

// Write standard DNS response header flags
static void writeDnsHeaderFlags(uint8_t* outBuf) {
    // QR=1, Opcode=0, AA=0, TC=0, RD=1
    outBuf[2] = 0x81;

    // RA=1, Z=0, RCODE=0
    outBuf[3] = 0x80;
}

// Build a DNS response that returns a blocked IP (0.0.0.0)
void DnsEngine::buildBlockedResponse(const uint8_t* query, size_t queryLen,
                                     uint8_t* outBuf, size_t& outLen,
                                     IPAddress blockedIP)
{
    if (queryLen > 512) queryLen = 512;

    // Copy original query header + question
    memcpy(outBuf, query, queryLen);
    size_t len = queryLen;

    // Apply standard DNS response flags
    writeDnsHeaderFlags(outBuf);

    // QDCOUNT = 1
    outBuf[6] = 0;
    outBuf[7] = 1;

    // ANCOUNT = 1
    outBuf[8]  = 0;
    outBuf[9]  = 0;
    outBuf[10] = 0;
    outBuf[11] = 0;

    // Ensure space for answer section
    if (len + 16 > 512) {
        outLen = len;
        return;
    }

    // Answer name pointer to offset 12
    outBuf[len++] = 0xC0;
    outBuf[len++] = 0x0C;

    // TYPE = A
    outBuf[len++] = 0x00;
    outBuf[len++] = 0x01;

    // CLASS = IN
    outBuf[len++] = 0x00;
    outBuf[len++] = 0x01;

    // TTL = 0
    outBuf[len++] = 0x00;
    outBuf[len++] = 0x00;
    outBuf[len++] = 0x00;
    outBuf[len++] = 0x00;

    // RDLENGTH = 4
    outBuf[len++] = 0x00;
    outBuf[len++] = 0x04;

    // RDATA = blocked IP
    outBuf[len++] = blockedIP[0];
    outBuf[len++] = blockedIP[1];
    outBuf[len++] = blockedIP[2];
    outBuf[len++] = blockedIP[3];

    outLen = len;
}

// Handle a single DNS query from dnsUdp
void DnsEngine::handleQuery() {
    uint8_t query[512];
    int len = dnsUdp.read(query, sizeof(query));
    if (len <= 0 || len < 12) return;

    totalQueries++;

    String qname;
    uint16_t qtype = 0, qclass = 0;
    size_t offset = 12;

    bool parsed = parseQuestion(query, (size_t)len, offset, qname, qtype, qclass);

    bool isAorAAAA = (qtype == 1 || qtype == 28);
    bool blocked = false;

    BloomFilter* f = filter ? filter : &::filter;

    if (parsed && isAorAAAA && f) {
        blocked = f->possiblyContains(qname);
    }

    if (blocked) {
        LogSerial.println("[DNS] BLOCKED: " + qname);

        blockedQueries++;
        lastBlocked = qname;

        uint8_t resp[512];
        size_t respLen = sizeof(resp);

        buildBlockedResponse(query, (size_t)len, resp, respLen, IPAddress(0,0,0,0));

        dnsUdp.beginPacket(dnsUdp.remoteIP(), dnsUdp.remotePort());
        dnsUdp.write(resp, respLen);
        dnsUdp.endPacket();
    } else {
        uint8_t resp[512];
        size_t respLen = sizeof(resp);

        bool ok = forwardToUpstream(query, (size_t)len, resp, respLen);
        if (ok) {
            dnsUdp.beginPacket(dnsUdp.remoteIP(), dnsUdp.remotePort());
            dnsUdp.write(resp, respLen);
            dnsUdp.endPacket();
        } else {
            LogSerial.println("[DNS] Upstream timeout");
        }
    }
}

// Forward DNS query to upstream server and wait for reply
bool DnsEngine::forwardToUpstream(const uint8_t* query, size_t queryLen,
                                  uint8_t* response, size_t& respLen,
                                  uint16_t timeoutMs)
{
    if (queryLen > 512) queryLen = 512;

    if (!upstreamUdp.beginPacket(upstreamIP, upstreamPort)) {
        return false;
    }

    upstreamUdp.write(query, queryLen);
    upstreamUdp.endPacket();

    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        int size = upstreamUdp.parsePacket();
        if (size > 0) {
            if ((size_t)size > respLen) size = respLen;

            int r = upstreamUdp.read(response, size);
            if (r > 0) {
                respLen = (size_t)r;
                return true;
            }
            break;
        }
        delay(1);
    }

    return false;
}
