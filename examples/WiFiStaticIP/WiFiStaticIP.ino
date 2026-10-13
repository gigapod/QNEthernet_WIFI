// SPDX-FileCopyrightText: (c) 2026 Kirk Benell
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiStaticIP joins a WiFi network with a static address instead of DHCP,
// then pings the gateway to confirm the address works.
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

// The static address settings. Pick an unused address on your network.
constexpr uint8_t kIP[4]{192, 168, 1, 200};
constexpr uint8_t kSubnet[4]{255, 255, 255, 0};
constexpr uint8_t kGateway[4]{192, 168, 1, 1};
constexpr uint8_t kDNS[4]{192, 168, 1, 1};

constexpr uint32_t kLinkTimeout = 30000;  // In milliseconds
constexpr unsigned long kPingInterval = 2000;  // In milliseconds

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

// Converts a 4-byte array to an IPAddress.
static IPAddress toIP(const uint8_t a[4]) {
  return IPAddress{a[0], a[1], a[2], a[3]};
}

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("WiFiStaticIP\r\n");

  // Choose the network first, then start with a static address
  if (!WiFi.setCredentials(WIFI_SSID, WIFI_PASSPHRASE,
                           (WIFI_PASSPHRASE[0] == '\0')
                               ? WiFiClass::Security::kOpen
                               : WiFiClass::Security::kWPA2)) {
    printf("Invalid SSID or passphrase\r\n");
    return;
  }
  printf("Joining \"%s\" with a static address...\r\n", WIFI_SSID);
  if (!WiFi.begin(toIP(kIP), toIP(kSubnet), toIP(kGateway), toIP(kDNS))) {
    printf("begin() failed: errno=%d\r\n", errno);
    return;
  }

  // With a static address, the address is set right away; wait for the link
  if (!WiFi.waitForLink(kLinkTimeout)) {
    printf("No link after %lu ms (status=%u)\r\n", kLinkTimeout,
           WiFi.status());
    return;
  }

  const IPAddress ip = WiFi.localIP();
  const IPAddress gw = WiFi.gatewayIP();
  const IPAddress dns = WiFi.dnsServerIP();
  printf("Link up. DHCP active: %s\r\n", WiFi.isDHCPActive() ? "yes" : "no");
  printf("  IP      %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  printf("  Gateway %u.%u.%u.%u\r\n", gw[0], gw[1], gw[2], gw[3]);
  printf("  DNS     %u.%u.%u.%u\r\n", dns[0], dns[1], dns[2], dns[3]);
}

// Main program loop.
void loop() {
  static unsigned long pingTimer = millis();
  if (!WiFi.linkState() || (millis() - pingTimer < kPingInterval)) {
    return;
  }
  pingTimer = millis();

  const IPAddress gw = WiFi.gatewayIP();
  const long t = WiFi.ping(gw);
  if (t >= 0) {
    printf("Ping gateway: %ld ms\r\n", t);
  } else {
    printf("Ping gateway: failed (errno=%d)\r\n", errno);
  }
}
