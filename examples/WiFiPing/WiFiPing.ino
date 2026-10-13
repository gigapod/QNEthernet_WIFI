// SPDX-FileCopyrightText: (c) 2026 SparkFun Electronics
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiPing joins a WiFi network and repeatedly pings the gateway and a host by
// name, keeping statistics. Use it to test the WiFi data path and DNS.
//
// This needs a Teensy 4.1 with a CYW4343W or CYW43439 WiFi board, and
// `ARDUINO_TEENSY41_CYW4343W` defined to `1`.
//
// This file is part of the QNEthernet library.

#include <QNEthernet.h>

using namespace qindesign::network;

#if !defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
#error "Define ARDUINO_TEENSY41_CYW4343W=1 to compile in WiFi"
#endif

// --------------------------------------------------------------------------
//  Configuration
// --------------------------------------------------------------------------

// Set these here or with build flags, for example: -DWIFI_SSID=\"mySSID\"
#ifndef WIFI_SSID
#define WIFI_SSID "yourSSID"
#endif
#ifndef WIFI_PASSPHRASE
#define WIFI_PASSPHRASE "yourPassphrase"  // Empty for an open network
#endif

#ifndef PING_HOST
#define PING_HOST "pjrc.com"
#endif

constexpr uint32_t kDHCPTimeout = 30000;       // In milliseconds
constexpr unsigned long kPingInterval = 1000;  // In milliseconds
constexpr uint32_t kReportEvery = 20;          // Print a summary every N rounds

// --------------------------------------------------------------------------
//  Program State
// --------------------------------------------------------------------------

namespace {  // Internal linkage section

// Ping statistics for one target.
struct PingStats {
  const char* name;
  uint32_t sent = 0;
  uint32_t received = 0;
  long minMs = -1;
  long maxMs = -1;
  long totalMs = 0;
};

PingStats gatewayStats{"gateway"};
PingStats hostStats{PING_HOST};
IPAddress hostIP;

}  // namespace

// --------------------------------------------------------------------------
//  Utilities
// --------------------------------------------------------------------------

// Records one ping result.
static void record(PingStats& s, const long t) {
  s.sent++;
  if (t < 0) {
    printf("  %-10s timeout\r\n", s.name);
    return;
  }
  s.received++;
  s.totalMs += t;
  if (s.minMs < 0 || t < s.minMs) s.minMs = t;
  if (t > s.maxMs) s.maxMs = t;
  printf("  %-10s %ld ms\r\n", s.name, t);
}

// Prints the statistics for one target.
static void summarize(const PingStats& s) {
  const uint32_t lost = s.sent - s.received;
  printf("  %-10s sent=%lu received=%lu lost=%lu (%lu%%)", s.name,
         static_cast<unsigned long>(s.sent),
         static_cast<unsigned long>(s.received),
         static_cast<unsigned long>(lost),
         static_cast<unsigned long>(s.sent ? (lost * 100) / s.sent : 0));
  if (s.received > 0) {
    printf(" min/avg/max=%ld/%ld/%ld ms", s.minMs,
           s.totalMs / static_cast<long>(s.received), s.maxMs);
  }
  printf("\r\n");
}

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("WiFiPing\r\n");

  printf("Joining \"%s\"...\r\n", WIFI_SSID);
  if (!WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE)) {
    printf("begin() failed: errno=%d\r\n", errno);
    return;
  }
  if (!WiFi.waitForLocalIP(kDHCPTimeout)) {
    printf("No address after %lu ms\r\n", kDHCPTimeout);
    return;
  }
  const IPAddress ip = WiFi.localIP();
  printf("IP %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);

  printf("Looking up %s...\r\n", PING_HOST);
  if (WiFi.hostByName(PING_HOST, hostIP)) {
    printf("%s is %u.%u.%u.%u\r\n", PING_HOST,
           hostIP[0], hostIP[1], hostIP[2], hostIP[3]);
  } else {
    printf("DNS lookup failed; pinging the gateway only\r\n");
  }
}

// Main program loop.
void loop() {
  static unsigned long pingTimer = millis();
  static uint32_t rounds = 0;
  if (!WiFi.linkState() || (WiFi.localIP() == INADDR_NONE) ||
      (millis() - pingTimer < kPingInterval)) {
    return;
  }
  pingTimer = millis();
  rounds++;

  printf("Round %lu:\r\n", static_cast<unsigned long>(rounds));
  record(gatewayStats, WiFi.ping(WiFi.gatewayIP()));
  if (hostIP != INADDR_NONE) {
    record(hostStats, WiFi.ping(hostIP));
  }

  if (rounds % kReportEvery == 0) {
    printf("Summary after %lu rounds, RSSI %ld dBm:\r\n",
           static_cast<unsigned long>(rounds), static_cast<long>(WiFi.RSSI()));
    summarize(gatewayStats);
    if (hostIP != INADDR_NONE) {
      summarize(hostStats);
    }
  }
}
