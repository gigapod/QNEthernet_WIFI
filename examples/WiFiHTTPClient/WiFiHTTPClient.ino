// SPDX-FileCopyrightText: (c) 2026 SparkFun Electronics
// SPDX-License-Identifier: AGPL-3.0-or-later

// WiFiHTTPClient joins a WiFi network and repeatedly fetches a web page over
// TCP, printing the response status line, the number of bytes received, and
// the time taken. Use it to test DNS and TCP over WiFi.
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

#ifndef HTTP_HOST
#define HTTP_HOST "example.com"
#endif
#ifndef HTTP_PATH
#define HTTP_PATH "/"
#endif
constexpr uint16_t kHTTPPort = 80;

constexpr uint32_t kDHCPTimeout = 30000;        // In milliseconds
constexpr uint32_t kResponseTimeout = 10000;    // In milliseconds
constexpr unsigned long kFetchInterval = 10000;  // In milliseconds

// --------------------------------------------------------------------------
//  Main Program
// --------------------------------------------------------------------------

// Fetches the page once and prints the results.
static void fetch() {
  static uint32_t fetches = 0;
  static uint32_t failures = 0;
  fetches++;

  WiFiClient client;
  const uint32_t t0 = millis();
  printf("[%lu] Connecting to %s:%u...\r\n",
         static_cast<unsigned long>(fetches), HTTP_HOST, kHTTPPort);
  if (!client.connect(HTTP_HOST, kHTTPPort)) {
    failures++;
    printf("  connect failed (errno=%d); failures=%lu\r\n", errno,
           static_cast<unsigned long>(failures));
    return;
  }
  const uint32_t tConnect = millis();

  client.writeFully("GET " HTTP_PATH " HTTP/1.1\r\n"
                    "Host: " HTTP_HOST "\r\n"
                    "Connection: close\r\n"
                    "\r\n");

  // Read until the server closes the connection
  char statusLine[80]{'\0'};
  size_t statusLen = 0;
  bool inStatusLine = true;
  size_t total = 0;
  uint8_t buf[512];
  uint32_t lastData = millis();
  while (client.connected() || client.available() > 0) {
    const int n = client.read(buf, sizeof(buf));
    if (n > 0) {
      lastData = millis();
      for (int i = 0; inStatusLine && i < n; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') {
          inStatusLine = false;
        } else if (statusLen < sizeof(statusLine) - 1) {
          statusLine[statusLen++] = static_cast<char>(buf[i]);
        }
      }
      total += static_cast<size_t>(n);
    } else if (millis() - lastData > kResponseTimeout) {
      printf("  timed out waiting for data\r\n");
      break;
    } else {
      yield();
    }
  }
  statusLine[statusLen] = '\0';
  client.close();
  const uint32_t tDone = millis();

  if (total == 0) {
    failures++;
  }
  const uint32_t ms = tDone - tConnect;
  printf("  \"%s\"\r\n", statusLine);
  printf("  %u bytes; connect %lu ms, transfer %lu ms (%lu kB/s); "
         "failures=%lu\r\n",
         static_cast<unsigned>(total), tConnect - t0, ms,
         static_cast<unsigned long>(ms ? total / ms : 0),
         static_cast<unsigned long>(failures));
}

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) {
    // Wait for Serial
  }
  printf("WiFiHTTPClient\r\n");

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
}

// Main program loop.
void loop() {
  static unsigned long fetchTimer = millis() - kFetchInterval;  // Start now
  if (!WiFi.linkState() || (WiFi.localIP() == INADDR_NONE) ||
      (millis() - fetchTimer < kFetchInterval)) {
    return;
  }
  fetchTimer = millis();
  fetch();
}
