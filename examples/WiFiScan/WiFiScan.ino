// SPDX-FileCopyrightText: (c) 2026 Kirk Benell
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiScan scans for WiFi networks every few seconds and prints them. It
// doesn't join a network, so it's a good first check that the chip works.
//
// This needs a Teensy 4.1 with the CYW4343W WiFi shield, and the
// `ARDUINO_TEENSY41_CYW4343W` macro defined to `1`.
//
// This file is part of the QNEthernet library.

#include <QNEthernet.h>

using namespace qindesign::network;

#if !defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
#error "Define ARDUINO_TEENSY41_CYW4343W=1 to compile in WiFi"
#endif

constexpr unsigned long kScanInterval = 5000;  // 5 seconds

// Returns a name for a scan result's security bits.
static const char* securityName(uint8_t bits) {
  if (bits & driver_cyw4343w::kScanSecurityWPA3) return "WPA3";
  if (bits & driver_cyw4343w::kScanSecurityWPA2) {
    return (bits & driver_cyw4343w::kScanSecurityWPA) ? "WPA/WPA2" : "WPA2";
  }
  if (bits & driver_cyw4343w::kScanSecurityWPA) return "WPA";
  if (bits & driver_cyw4343w::kScanSecurityWEP) return "WEP";
  return "OPEN";
}

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("Starting...\r\n");

  uint8_t mac[6];
  WiFi.macAddress(mac);  // This boots the chip
  printf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Main program loop.
void loop() {
  printf("\r\nScanning...\r\n");
  const int n = WiFi.scanNetworks();
  if (n < 0) {
    printf("Scan failed\r\n");
  } else {
    printf("Found %d networks\r\n", n);
    printf(" #  %-32s  RSSI  Ch  BSSID              Security\r\n", "SSID");
    for (int i = 0; i < n; i++) {
      WiFiClass::ScanResult r;
      if (!WiFi.scanResult(i, r)) {
        continue;
      }
      printf("%2d  %-32s  %4d  %2u  "
             "%02X:%02X:%02X:%02X:%02X:%02X  %s\r\n",
             i + 1, r.ssid, r.rssi, r.channel,
             r.bssid[0], r.bssid[1], r.bssid[2],
             r.bssid[3], r.bssid[4], r.bssid[5],
             securityName(r.security));
    }
  }
  delay(kScanInterval);
}
