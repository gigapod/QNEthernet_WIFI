// SPDX-FileCopyrightText: (c) 2026 Kirk Benell
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiTCPThroughput joins a WiFi network and runs a TCP server that receives
// data as fast as the sender can send it, printing the throughput for each
// connection. Use it to load-test the WiFi receive path.
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

constexpr uint16_t kPort = 5001;

constexpr uint32_t kDHCPTimeout = 30000;          // In milliseconds
constexpr unsigned long kProgressInterval = 1000;  // In milliseconds

// --------------------------------------------------------------------------
//  Program State
// --------------------------------------------------------------------------

namespace {  // Internal linkage section

WiFiServer server{kPort};
uint8_t buf[2048];

}  // namespace

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

// Prints a byte count and rate.
static void printRate(const char* label, const size_t bytes,
                      const uint32_t ms) {
  // kbit/s = bytes * 8 / ms
  const unsigned long kbps =
      ms ? static_cast<unsigned long>((static_cast<uint64_t>(bytes) * 8) / ms)
         : 0;
  printf("%s %lu bytes in %lu ms = %lu.%03lu Mbit/s\r\n", label,
         static_cast<unsigned long>(bytes), static_cast<unsigned long>(ms),
         kbps / 1000, kbps % 1000);
}

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("WiFiTCPThroughput\r\n");

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
  server.begin();
  printf("Listening on %u.%u.%u.%u:%u\r\n", ip[0], ip[1], ip[2], ip[3], kPort);
}

// Main program loop.
void loop() {
  WiFiClient client = server.accept();
  if (!client) {
    return;
  }

  const IPAddress from = client.remoteIP();
  printf("Connection from %u.%u.%u.%u\r\n", from[0], from[1], from[2], from[3]);

  size_t total = 0;
  size_t sinceProgress = 0;
  const uint32_t start = millis();
  uint32_t progressTimer = start;
  while (client.connected() || client.available() > 0) {
    const int n = client.read(buf, sizeof(buf));
    if (n > 0) {
      total += static_cast<size_t>(n);
      sinceProgress += static_cast<size_t>(n);
    } else {
      yield();
    }
    if (millis() - progressTimer >= kProgressInterval) {
      printRate("  interval:", sinceProgress, millis() - progressTimer);
      sinceProgress = 0;
      progressTimer = millis();
    }
  }
  client.close();
  printRate("Total:", total, millis() - start);
}
