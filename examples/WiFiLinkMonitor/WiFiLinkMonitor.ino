// SPDX-FileCopyrightText: (c) 2026 Kirk Benell
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiLinkMonitor joins a WiFi network and then reports link, address, and
// status changes with timestamps, plus a periodic RSSI line. Use it to test
// rejoining: power-cycle or move away from the access point and watch the
// interface come back.
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

constexpr unsigned long kReportInterval = 5000;  // In milliseconds

// --------------------------------------------------------------------------
//  Program State
// --------------------------------------------------------------------------

namespace {  // Internal linkage section

uint32_t linkUps = 0;
uint32_t linkDowns = 0;
uint32_t addressChanges = 0;
unsigned long lastLinkChange = 0;

}  // namespace

// --------------------------------------------------------------------------
//  Utilities
// --------------------------------------------------------------------------

// Returns a name for a WiFi status.
static const char* statusName(const wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:    return "IDLE";
    case WL_CONNECTED:      return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_DISCONNECTED:   return "DISCONNECTED";
    case WL_NO_SHIELD:      return "NO_SHIELD";
    default:                return "UNKNOWN";
  }
}

// Prints a timestamp prefix, in seconds.
static void stamp() {
  const unsigned long t = millis();
  printf("[%5lu.%03lu] ", t / 1000, t % 1000);
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
  printf("WiFiLinkMonitor\r\n");

  WiFi.onLinkState([](bool state) {
    const unsigned long now = millis();
    stamp();
    if (state) {
      linkUps++;
      printf("Link UP (was down %lu ms)\r\n", now - lastLinkChange);
    } else {
      linkDowns++;
      printf("Link DOWN (was up %lu ms)\r\n", now - lastLinkChange);
    }
    lastLinkChange = now;
  });

  WiFi.onAddressChanged([]() {
    addressChanges++;
    const IPAddress ip = WiFi.localIP();
    stamp();
    printf("Address: %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  });

  WiFi.onInterfaceStatus([](bool status) {
    stamp();
    printf("Interface %s\r\n", status ? "UP" : "DOWN");
  });

  stamp();
  printf("Joining \"%s\"...\r\n", WIFI_SSID);
  if (!WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE)) {
    printf("begin() failed: errno=%d\r\n", errno);
  }
  lastLinkChange = millis();
}

// Main program loop.
void loop() {
  // Status changes
  static wl_status_t lastStatus = WL_DISCONNECTED;
  const wl_status_t status = WiFi.status();
  if (status != lastStatus) {
    stamp();
    printf("Status: %s -> %s\r\n", statusName(lastStatus), statusName(status));
    lastStatus = status;
  }

  // Periodic report
  static unsigned long reportTimer = millis();
  if (millis() - reportTimer >= kReportInterval) {
    reportTimer = millis();
    const IPAddress ip = WiFi.localIP();
    stamp();
    printf("%s, RSSI %ld dBm, IP %u.%u.%u.%u, ups=%lu downs=%lu addr=%lu\r\n",
           statusName(status), static_cast<long>(WiFi.RSSI()),
           ip[0], ip[1], ip[2], ip[3], static_cast<unsigned long>(linkUps),
           static_cast<unsigned long>(linkDowns),
           static_cast<unsigned long>(addressChanges));
  }
}
