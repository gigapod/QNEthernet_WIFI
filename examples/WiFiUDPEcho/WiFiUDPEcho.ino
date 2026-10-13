// SPDX-FileCopyrightText: (c) 2026 SparkFun Electronics
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiUDPEcho joins a WiFi network and echoes UDP packets back to the sender.
// It also serves a one-line TCP greeting. Both listen on all started
// interfaces, so they also answer over wired Ethernet if it's started.
//
// Test with, for example:
//   echo -n "hello" | nc -4u -w1 <IP> 8080
//   nc <IP> 8081
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

constexpr uint16_t kUDPPort = 8080;
constexpr uint16_t kTCPPort = 8081;

// --------------------------------------------------------------------------
//  Program State
// --------------------------------------------------------------------------

namespace {  // Internal linkage section

WiFiUDP udp;
WiFiServer server{kTCPPort};

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
  printf("Starting...\r\n");

  printf("Joining \"%s\"...\r\n", WIFI_SSID);
  if (!WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE)) {
    printf("Failed to start WiFi\r\n");
    return;
  }
  if (!WiFi.waitForLocalIP(kDHCPTimeout)) {
    printf("No address yet (status=%u); continuing\r\n", WiFi.status());
  } else {
    const IPAddress ip = WiFi.localIP();
    printf("IP: %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  }

  udp.begin(kUDPPort);
  server.begin();
  printf("Listening on UDP %u and TCP %u\r\n", kUDPPort, kTCPPort);
}

// Main program loop.
void loop() {
  // Echo UDP packets
  const int size = udp.parsePacket();
  if (size >= 0) {
    const IPAddress remote = udp.remoteIP();
    printf("UDP %d bytes from %u.%u.%u.%u:%u\r\n", size,
           remote[0], remote[1], remote[2], remote[3], udp.remotePort());
    udp.send(remote, udp.remotePort(), udp.data(), static_cast<size_t>(size));
  }

  // Greet TCP clients
  WiFiClient client = server.accept();
  if (client) {
    client.writeFully("Hello from Teensy over WiFi\r\n");
    client.close();
  }
}
