// SPDX-FileCopyrightText: (c) 2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// QNWiFi.h defines the WiFi interface. It runs alongside Ethernet and shares
// the same network stack.
// This file is part of the QNEthernet library.

#pragma once

#include "QNEthernet.h"

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)

namespace qindesign {
namespace network {

// The WiFi interface. This has the EthernetClass API, minus the wired-only
// parts. Note that some settings, such as DNS servers, are shared by
// all interfaces.
class WiFiClass final : private EthernetClass {
 public:
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

 private:
  WiFiClass() : EthernetClass(enet::wifi_interface()) {}
  ~WiFiClass() noexcept = default;

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
