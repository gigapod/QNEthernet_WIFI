// SPDX-FileCopyrightText: (c) 2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// QNWiFi.h defines the WiFi interface. It runs alongside Ethernet and shares
// the same network stack.
// This file is part of the QNEthernet library.

#pragma once

#include "QNEthernet.h"

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)

// C++ includes
#include <cstdint>

#include "qnethernet/drivers/driver_CYW4343W_wifi.h"

namespace qindesign {
namespace network {

// WiFi connection status. The names and values match the Arduino WiFi API.
enum wl_status_t : uint8_t {
  WL_IDLE_STATUS     = 0,    // Trying to connect
  WL_CONNECTED       = 3,    // Joined the network
  WL_CONNECT_FAILED  = 4,    // The last attempt failed; it will be retried
  WL_DISCONNECTED    = 6,    // Not trying to connect
  WL_NO_SHIELD       = 255,  // No WiFi hardware
};

// The WiFi interface. This has the EthernetClass API, minus the wired-only
// parts. Note that some settings, such as DNS servers, are shared by
// all interfaces.
class WiFiClass final : private EthernetClass {
 public:
  using Security   = driver_cyw4343w::Security;
  using ScanResult = driver_cyw4343w::ScanResult;

  static constexpr uint32_t kDefaultScanTimeout = 10000;  // 10 seconds

  using EthernetClass::kMACAddrSize;

  using EthernetClass::libraryVersion;
  using EthernetClass::maxMulticastGroups;
  using EthernetClass::mtu;
  using EthernetClass::isPromiscuousMode;

  using EthernetClass::driverCapabilities;
  using EthernetClass::interfaceName;
  using EthernetClass::macAddress;

  using EthernetClass::loop;

  using EthernetClass::begin;
  using EthernetClass::end;
  using EthernetClass::setDHCPEnabled;
  using EthernetClass::isDHCPEnabled;
  using EthernetClass::isDHCPActive;
  using EthernetClass::renewDHCP;
  using EthernetClass::waitForLocalIP;
  using EthernetClass::waitForLink;

  using EthernetClass::linkStatus;
  using EthernetClass::linkState;
  using EthernetClass::onLinkState;
  using EthernetClass::onAddressChanged;
  using EthernetClass::onInterfaceStatus;
  using EthernetClass::interfaceStatus;

  using EthernetClass::localIP;
  using EthernetClass::subnetMask;
  using EthernetClass::gatewayIP;
  using EthernetClass::dnsServerIP;
  using EthernetClass::broadcastIP;
  using EthernetClass::setLocalIP;
  using EthernetClass::setSubnetMask;
  using EthernetClass::setGatewayIP;
  using EthernetClass::setDNSServerIP;

  using EthernetClass::joinGroup;
  using EthernetClass::leaveGroup;
  using EthernetClass::setHostname;
  using EthernetClass::hostname;
  using EthernetClass::operator bool;
  using EthernetClass::setAsDefaultInterface;

  using EthernetClass::hostByName;
  using EthernetClass::ping;

  // --------------------------------------------------------------------------
  //  Network Selection
  // --------------------------------------------------------------------------

  // Sets the network to join when the interface starts (see the begin()
  // functions). If the interface is already started, call begin() again to
  // join the new network. An open network ignores the passphrase.
  //
  // This returns false and sets errno to EINVAL if the SSID is empty or longer
  // than 32 characters, or if the passphrase isn't 8-63 characters for
  // a secured network.
  bool setCredentials(const char* ssid, const char* passphrase,
                      Security security = Security::kWPA2);

  // Sets the network and starts the interface with DHCP. If the passphrase is
  // NULL or empty then the network is assumed to be open; otherwise WPA2 is
  // used. Joining happens in the background; use waitForLink() or
  // waitForLocalIP() to wait for it, or status() to check it.
  //
  // To use a static address, call setCredentials() and then one of the
  // begin(ip, ...) functions.
  //
  // This returns false if the credentials are invalid (errno will be EINVAL)
  // or if the interface could not be started.
  bool begin(const char* ssid, const char* passphrase = nullptr);

  // Sets the network, with an explicit security type, and starts the
  // interface with DHCP.
  bool begin(const char* ssid, const char* passphrase, Security security);

  // Leaves the network and stops the interface. This is the same as end().
  void disconnect() {
    end();
  }

  // --------------------------------------------------------------------------
  //  Connection Information
  // --------------------------------------------------------------------------

  // Returns the connection status.
  wl_status_t status() const;

  // Returns the SSID of the configured network, or an empty string.
  const char* SSID() const;

  // Returns the signal strength of the joined network, in dBm, or zero if not
  // joined.
  int32_t RSSI() const;

  // Fills in the BSSID (the access point's MAC address) of the joined network
  // and returns 'bssid', or returns NULL if not joined.
  uint8_t* BSSID(uint8_t bssid[6]) const;

  // --------------------------------------------------------------------------
  //  Scanning
  // --------------------------------------------------------------------------

  // Scans for networks, waiting up to 'timeout' milliseconds, and returns the
  // number found, or -1 on error. This blocks, and while scanning, the WiFi
  // interface drops received frames.
  int scanNetworks(uint32_t timeout = kDefaultScanTimeout);

  // Gets one result from the last scan. This returns false if the index is out
  // of range.
  bool scanResult(int index, ScanResult& result) const;

  // Accessors for the last scan's results, by index. These return an empty
  // string, zero, or NULL if the index is out of range.
  const char* SSID(int index);
  int32_t RSSI(int index) const;
  uint8_t channel(int index) const;
  uint8_t* BSSID(int index, uint8_t bssid[6]) const;
  uint8_t encryptionType(int index) const;  // driver_cyw4343w::ScanSecurityBits

 private:
  WiFiClass() : EthernetClass(enet::wifi_interface()) {}
  ~WiFiClass() noexcept = default;

  char scanSSID_[sizeof(ScanResult::ssid)]{'\0'};  // For SSID(index)

  // WiFiClass is neither copyable nor movable
  WiFiClass(const WiFiClass&) = delete;
  WiFiClass(WiFiClass&&) = delete;
  WiFiClass& operator=(const WiFiClass&) = delete;
  WiFiClass& operator=(WiFiClass&&) = delete;

  STATIC_INIT_FRIEND(WiFiClass, WiFi);
};

// Instance for interacting with WiFi.
STATIC_INIT_DECL(WiFiClass, WiFi);

}  // namespace network
}  // namespace qindesign

#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS
