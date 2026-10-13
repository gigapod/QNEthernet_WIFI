// SPDX-FileCopyrightText: (c) 2026 Kirk Benell
// SPDX-License-Identifier: AGPL-3.0-or-later

// EthernetAndWiFi brings up the wired Ethernet and WiFi interfaces at the same
// time and pings each one's gateway.
//
// This needs a Teensy 4.1 with the CYW4343W WiFi shield, and the
// `ARDUINO_TEENSY41_CYW4343W` macro defined to `1`. Set your network's SSID
// and passphrase below or with build flags.
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

constexpr uint32_t kDHCPTimeout = 15000;  // 15 seconds

constexpr unsigned long kPingInterval = 2000;  // 2 seconds

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

// Prints an interface's address settings.
static void printAddresses(const char* name, const char* ifName,
                           const IPAddress& ip, const IPAddress& gw) {
  printf("[%s] %s: IP=%u.%u.%u.%u gateway=%u.%u.%u.%u\r\n", name, ifName,
         ip[0], ip[1], ip[2], ip[3], gw[0], gw[1], gw[2], gw[3]);
}

// Prints the result of pinging a gateway, given the ping time.
static void printPing(const char* name, const IPAddress& gw, long t) {
  if (t >= 0) {
    printf("[%s] Ping %u.%u.%u.%u: %ld ms\r\n", name,
           gw[0], gw[1], gw[2], gw[3], t);
  } else {
    printf("[%s] Ping %u.%u.%u.%u: failed\r\n", name,
           gw[0], gw[1], gw[2], gw[3]);
  }
}

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("Starting...\r\n");

  Ethernet.onLinkState([](bool state) {
    printf("[Ethernet] Link %s\r\n", state ? "ON" : "OFF");
  });
  Ethernet.onAddressChanged([]() {
    if (Ethernet.localIP() != INADDR_NONE) {
      printAddresses("Ethernet", Ethernet.interfaceName(),
                     Ethernet.localIP(), Ethernet.gatewayIP());
    }
  });
  WiFi.onLinkState([](bool state) {
    printf("[WiFi] Link %s\r\n", state ? "ON" : "OFF");
  });
  WiFi.onAddressChanged([]() {
    if (WiFi.localIP() != INADDR_NONE) {
      printAddresses("WiFi", WiFi.interfaceName(),
                     WiFi.localIP(), WiFi.gatewayIP());
    }
  });

  printf("Starting Ethernet with DHCP...\r\n");
  if (!Ethernet.begin()) {
    printf("Failed to start Ethernet\r\n");
  }
  printf("Starting WiFi with DHCP, joining \"%s\"...\r\n", WIFI_SSID);
  if (!WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE)) {
    printf("Failed to start WiFi\r\n");
  }

  // Ethernet.loop() (called from yield()) moves both interfaces along
  if (!Ethernet.waitForLocalIP(kDHCPTimeout)) {
    printf("No Ethernet address yet\r\n");
  }
  if (!WiFi.waitForLocalIP(kDHCPTimeout)) {
    printf("No WiFi address yet (status=%u)\r\n", WiFi.status());
  } else {
    uint8_t bssid[6];
    if (WiFi.BSSID(bssid) != nullptr) {
      printf("[WiFi] SSID=%s BSSID=%02X:%02X:%02X:%02X:%02X:%02X RSSI=%ld dBm\r\n",
             WiFi.SSID(), bssid[0], bssid[1], bssid[2], bssid[3], bssid[4],
             bssid[5], static_cast<long>(WiFi.RSSI()));
    }
  }
}

// Main program loop.
void loop() {
  static unsigned long pingTimer = millis();
  if ((millis() - pingTimer) >= kPingInterval) {
    // Each ping is routed out the interface whose subnet holds the address
    const IPAddress ethGW = Ethernet.gatewayIP();
    if (ethGW != INADDR_NONE) {
      printPing("Ethernet", ethGW, Ethernet.ping(ethGW));
    }
    const IPAddress wifiGW = WiFi.gatewayIP();
    if (wifiGW != INADDR_NONE) {
      printPing("WiFi", wifiGW, WiFi.ping(wifiGW));
    }
    pingTimer = millis();
  }
}
