// SPDX-FileCopyrightText: (c) 2026 SparkFun Electronics
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiMulticast joins a WiFi network and a UDP multicast group, prints every
// packet received on the group, and sends a numbered heartbeat to the group
// every few seconds. Use it to test the WiFi multicast filter.
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

constexpr uint8_t kGroup[4]{239, 255, 0, 1};
constexpr uint16_t kPort = 5007;

constexpr uint32_t kDHCPTimeout = 30000;            // In milliseconds
constexpr unsigned long kHeartbeatInterval = 5000;  // In milliseconds

// --------------------------------------------------------------------------
//  Program State
// --------------------------------------------------------------------------

namespace {  // Internal linkage section

WiFiUDP udp;
IPAddress group;
uint32_t received = 0;
bool joined = false;

}  // namespace

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("WiFiMulticast\r\n");

  group = IPAddress{kGroup[0], kGroup[1], kGroup[2], kGroup[3]};

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

  // Join the group after the interface has started, so it includes WiFi
  if (!udp.beginMulticast(group, kPort)) {
    printf("beginMulticast() failed: errno=%d\r\n", errno);
    return;
  }
  joined = true;
  printf("Joined %u.%u.%u.%u port %u\r\n",
         group[0], group[1], group[2], group[3], kPort);
}

// Main program loop.
void loop() {
  if (!joined) {
    return;
  }

  // Print received packets
  const int size = udp.parsePacket();
  if (size >= 0) {
    received++;
    const IPAddress from = udp.remoteIP();
    printf("RX #%lu from %u.%u.%u.%u:%u (%d bytes): %.*s\r\n",
           static_cast<unsigned long>(received),
           from[0], from[1], from[2], from[3], udp.remotePort(), size,
           size, reinterpret_cast<const char*>(udp.data()));
  }

  // Send a heartbeat
  static unsigned long heartbeatTimer = millis();
  static uint32_t sent = 0;
  if (millis() - heartbeatTimer >= kHeartbeatInterval) {
    heartbeatTimer = millis();
    sent++;
    char msg[64];
    const IPAddress ip = WiFi.localIP();
    const int len = snprintf(msg, sizeof(msg), "heartbeat %lu from %u.%u.%u.%u",
                             static_cast<unsigned long>(sent),
                             ip[0], ip[1], ip[2], ip[3]);
    if (!udp.send(group, kPort, msg, static_cast<size_t>(len))) {
      printf("TX failed (errno=%d)\r\n", errno);
    } else {
      printf("TX %s\r\n", msg);
    }
  }
}
