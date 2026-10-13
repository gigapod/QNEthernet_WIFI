// SPDX-FileCopyrightText: (c) 2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// QNWiFi.cpp contains the WiFi interface implementation.
// This file is part of the QNEthernet library.

#include "QNWiFi.h"

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)

// C++ includes
#include <cerrno>
#include <cstring>

namespace qindesign {
namespace network {

// A reference to the singleton.
STATIC_INIT_DEFN(WiFiClass, WiFi);

namespace wifi = driver_cyw4343w;

bool WiFiClass::setCredentials(const char* const ssid,
                               const char* const passphrase,
                               const Security security) {
  if (!wifi::set_credentials(ssid, passphrase, security)) {
    errno = EINVAL;
    return false;
  }
  return true;
}

bool WiFiClass::begin(const char* const ssid, const char* const passphrase) {
  const bool isOpen = (passphrase == nullptr) || (passphrase[0] == '\0');
  return begin(ssid, passphrase, isOpen ? Security::kOpen : Security::kWPA2);
}

bool WiFiClass::begin(const char* const ssid, const char* const passphrase,
                      const Security security) {
  if (!setCredentials(ssid, passphrase, security)) {
    return false;
  }
  return EthernetClass::begin();
}

wl_status_t WiFiClass::status() const {
  const DriverOps& ops = wifi::kDriverOps;
  if (!ops.is_unknown() && !ops.has_hardware()) {
    return WL_NO_SHIELD;
  }
  switch (wifi::join_status()) {
    case wifi::JoinStatus::kJoining:
      return WL_IDLE_STATUS;
    case wifi::JoinStatus::kJoined:
      return WL_CONNECTED;
    case wifi::JoinStatus::kFailed:
      return WL_CONNECT_FAILED;
    case wifi::JoinStatus::kStopped:
    default:
      return WL_DISCONNECTED;
  }
}

const char* WiFiClass::SSID() const {
  return wifi::ssid();
}

int32_t WiFiClass::RSSI() const {
  int32_t value = 0;
  if (!wifi::rssi(&value)) {
    return 0;
  }
  return value;
}

uint8_t* WiFiClass::BSSID(uint8_t bssid[6]) const {
  if ((bssid == nullptr) || !wifi::bssid(bssid)) {
    return nullptr;
  }
  return bssid;
}

int WiFiClass::scanNetworks(const uint32_t timeout) {
  return wifi::scan(timeout);
}

bool WiFiClass::scanResult(const int index, ScanResult& result) const {
  return wifi::scan_result(index, &result);
}

const char* WiFiClass::SSID(const int index) {
  ScanResult r;
  if (!scanResult(index, r)) {
    return "";
  }
  std::memcpy(scanSSID_, r.ssid, sizeof(scanSSID_));
  return scanSSID_;
}

int32_t WiFiClass::RSSI(const int index) const {
  ScanResult r;
  return scanResult(index, r) ? r.rssi : 0;
}

uint8_t WiFiClass::channel(const int index) const {
  ScanResult r;
  return scanResult(index, r) ? r.channel : 0;
}

uint8_t* WiFiClass::BSSID(const int index, uint8_t bssid[6]) const {
  ScanResult r;
  if ((bssid == nullptr) || !scanResult(index, r)) {
    return nullptr;
  }
  std::memcpy(bssid, r.bssid, sizeof(r.bssid));
  return bssid;
}

uint8_t WiFiClass::encryptionType(const int index) const {
  ScanResult r;
  return scanResult(index, r) ? r.security : 0;
}

}  // namespace network
}  // namespace qindesign

#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS
