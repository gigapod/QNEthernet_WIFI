// SPDX-FileCopyrightText: (c) 2026 SparkFun Electronics
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiBeginEndCycle repeatedly starts WiFi, waits for an address, pings the
// gateway, and stops WiFi again, then prints a summary. Use it to test that
// starting and stopping the interface is reliable.
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

constexpr uint32_t kCycles = 20;
constexpr uint32_t kDHCPTimeout = 30000;  // In milliseconds
constexpr uint32_t kOffTime = 2000;       // Time between cycles, in milliseconds

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("WiFiBeginEndCycle: %lu cycles\r\n",
         static_cast<unsigned long>(kCycles));

  uint32_t ok = 0;
  uint32_t beginFailures = 0;
  uint32_t addressFailures = 0;
  uint32_t pingFailures = 0;
  uint32_t minMs = UINT32_MAX;
  uint32_t maxMs = 0;
  uint32_t totalMs = 0;

  for (uint32_t i = 1; i <= kCycles; i++) {
    printf("Cycle %lu: ", static_cast<unsigned long>(i));
    const uint32_t t0 = millis();
    if (!WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE)) {
      beginFailures++;
      printf("begin() failed (errno=%d)\r\n", errno);
      WiFi.disconnect();
      delay(kOffTime);
      continue;
    }
    if (!WiFi.waitForLocalIP(kDHCPTimeout)) {
      addressFailures++;
      printf("no address (status=%u)\r\n", WiFi.status());
      WiFi.disconnect();
      delay(kOffTime);
      continue;
    }
    const uint32_t ms = millis() - t0;
    const long t = WiFi.ping(WiFi.gatewayIP());
    if (t < 0) {
      pingFailures++;
    } else {
      ok++;
    }
    minMs = (ms < minMs) ? ms : minMs;
    maxMs = (ms > maxMs) ? ms : maxMs;
    totalMs += ms;
    const IPAddress ip = WiFi.localIP();
    printf("address %u.%u.%u.%u after %lu ms, ping %s\r\n",
           ip[0], ip[1], ip[2], ip[3], static_cast<unsigned long>(ms),
           (t >= 0) ? "ok" : "FAILED");

    WiFi.disconnect();
    delay(kOffTime);
  }

  const uint32_t withAddress = kCycles - beginFailures - addressFailures;
  printf("\r\nSummary:\r\n");
  printf("  OK (address and ping): %lu/%lu\r\n",
         static_cast<unsigned long>(ok), static_cast<unsigned long>(kCycles));
  printf("  begin() failures:      %lu\r\n",
         static_cast<unsigned long>(beginFailures));
  printf("  No address:            %lu\r\n",
         static_cast<unsigned long>(addressFailures));
  printf("  Ping failures:         %lu\r\n",
         static_cast<unsigned long>(pingFailures));
  if (withAddress > 0) {
    printf("  Time to address min/avg/max: %lu/%lu/%lu ms\r\n",
           static_cast<unsigned long>(minMs),
           static_cast<unsigned long>(totalMs / withAddress),
           static_cast<unsigned long>(maxMs));
  }
}

// Main program loop.
void loop() {
}
