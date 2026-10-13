// SPDX-FileCopyrightText: (c) 2026 Kirk Benell
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiConnect joins a WiFi network with DHCP, shows each status change while
// joining, and then prints the connection details and how long each stage took.
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

constexpr uint32_t kTimeout = 30000;  // How long to wait for an address, in ms

// --------------------------------------------------------------------------
//  Utilities
// --------------------------------------------------------------------------

// Returns a name for a WiFi status.
static const char* statusName(const wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:    return "IDLE (joining)";
    case WL_CONNECTED:      return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED (will retry)";
    case WL_DISCONNECTED:   return "DISCONNECTED";
    case WL_NO_SHIELD:      return "NO_SHIELD";
    default:                return "UNKNOWN";
  }
}

// Prints a labelled IP address.
static void printIP(const char* label, const IPAddress& ip) {
  printf("  %-10s %u.%u.%u.%u\r\n", label, ip[0], ip[1], ip[2], ip[3]);
}

// Prints a labelled MAC address.
static void printMAC(const char* label, const uint8_t* mac) {
  printf("  %-10s %02X:%02X:%02X:%02X:%02X:%02X\r\n", label,
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
  printf("WiFiConnect\r\n");

  const uint32_t t0 = millis();
  printf("Joining \"%s\"...\r\n", WIFI_SSID);
  if (!WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE)) {
    printf("begin() failed: errno=%d, status=%s\r\n", errno,
           statusName(WiFi.status()));
    return;
  }
  const uint32_t tBegin = millis();

  // Watch the status and link until there's an address
  wl_status_t lastStatus = WiFi.status();
  printf("[%6lu ms] status: %s\r\n", tBegin - t0, statusName(lastStatus));
  uint32_t tLink = 0;
  while ((WiFi.localIP() == INADDR_NONE) && (millis() - t0 < kTimeout)) {
    const wl_status_t status = WiFi.status();
    if (status != lastStatus) {
      printf("[%6lu ms] status: %s\r\n", millis() - t0, statusName(status));
      lastStatus = status;
    }
    if ((tLink == 0) && WiFi.linkState()) {
      tLink = millis();
      printf("[%6lu ms] link up\r\n", tLink - t0);
    }
    delay(50);  // Calls yield(), which runs the stack
  }
  const uint32_t tIP = millis();

  if (WiFi.localIP() == INADDR_NONE) {
    printf("No address after %lu ms (status=%s)\r\n", kTimeout,
           statusName(WiFi.status()));
    return;
  }

  printf("\r\nConnected:\r\n");
  printf("  %-10s %s\r\n", "Interface", WiFi.interfaceName());
  printf("  %-10s %s\r\n", "SSID", WiFi.SSID());
  uint8_t bssid[6];
  if (WiFi.BSSID(bssid) != nullptr) {
    printMAC("BSSID", bssid);
  }
  printf("  %-10s %ld dBm\r\n", "RSSI", static_cast<long>(WiFi.RSSI()));
  printMAC("MAC", WiFi.macAddress());
  printIP("IP", WiFi.localIP());
  printIP("Subnet", WiFi.subnetMask());
  printIP("Gateway", WiFi.gatewayIP());
  printIP("DNS", WiFi.dnsServerIP());
  printf("  %-10s %s\r\n", "Hostname", WiFi.hostname());

  printf("\r\nTiming:\r\n");
  printf("  begin() returned after %lu ms (includes booting the chip)\r\n",
         tBegin - t0);
  if (tLink != 0) {
    printf("  link up after          %lu ms\r\n", tLink - t0);
  }
  printf("  address after          %lu ms\r\n", tIP - t0);
}

// Main program loop.
void loop() {
  // Report status changes, for example if the access point goes away
  static wl_status_t lastStatus = WiFi.status();
  const wl_status_t status = WiFi.status();
  if (status != lastStatus) {
    printf("[%lu ms] status: %s\r\n", millis(), statusName(status));
    lastStatus = status;
  }
}
