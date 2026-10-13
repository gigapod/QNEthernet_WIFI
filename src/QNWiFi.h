// SPDX-FileCopyrightText: (c) 2026 Kirk Benell
// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file QNWiFi.h
 * @brief Defines the WiFi interface, which runs alongside Ethernet and shares
 *        the same network stack.
 *
 * This is only available when a WiFi driver is compiled in
 * (`ARDUINO_TEENSY41_CYW4343W` defined to `1`). It is included by
 * QNEthernet.h.
 *
 * This file is part of the QNEthernet library.
 */

#pragma once

#include "QNEthernet.h"

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)

// C++ includes
#include <cstdint>

#include "qnethernet/drivers/driver_CYW4343W_wifi.h"

namespace qindesign {
namespace network {

/**
 * @brief WiFi connection status.
 *
 * The names and values match the Arduino WiFi API.
 *
 * @see WiFiClass::status()
 */
enum wl_status_t : uint8_t {
  WL_IDLE_STATUS     = 0,    ///< Trying to connect.
  WL_CONNECTED       = 3,    ///< Joined the network.
  WL_CONNECT_FAILED  = 4,    ///< The last attempt failed; it will be retried.
  WL_DISCONNECTED    = 6,    ///< Not trying to connect.
  WL_NO_SHIELD       = 255,  ///< No WiFi hardware.
};

/**
 * @brief The WiFi network interface.
 *
 * This has the EthernetClass API, minus the wired-only parts (link settings,
 * the SPI chip select, MAC address changes, and raw frames), plus functions
 * for choosing a network, getting connection information, and scanning.
 *
 * Use the `WiFi` instance; this class can't be constructed directly.
 *
 * The network stack is shared with `Ethernet`, so:
 * - DNS servers and hostByName() are shared by all interfaces.
 * - loop() (on either instance) moves all interfaces along. It is called from
 *   `yield()` by default.
 * - Sockets aren't tied to an interface; see WiFiClient, WiFiServer,
 *   and WiFiUDP.
 *
 * Example:
 * @code
 * WiFi.begin("mySSID", "myPassphrase");
 * if (WiFi.waitForLocalIP(15000)) {
 *   Serial.println(WiFi.localIP());
 * }
 * @endcode
 */
class WiFiClass final : private EthernetClass {
 public:
  /** @brief Security type for joining a network. */
  using Security   = driver_cyw4343w::Security;

  /** @brief One network found by scanNetworks(). */
  using ScanResult = driver_cyw4343w::ScanResult;

  /** @brief The default scanNetworks() timeout, in milliseconds. */
  static constexpr uint32_t kDefaultScanTimeout = 10000;  // 10 seconds

  // These have the same meaning as in EthernetClass; see there for details.
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

  /**
   * @brief Sets the network to join when the interface starts.
   *
   * This takes effect at the next begin() call. If the interface is already
   * started, call begin() again to join the new network. Use this before one
   * of the static-address begin(ip, ...) functions.
   *
   * @param ssid       The network name, 1-32 characters.
   * @param passphrase The passphrase, 8-63 characters for a secured network.
   *                   Ignored for an open network; may be NULL.
   * @param security   The security type.
   * @return Whether the credentials are valid. If not, errno is set
   *         to EINVAL.
   */
  bool setCredentials(const char* ssid, const char* passphrase,
                      Security security = Security::kWPA2);

  /**
   * @brief Sets the network and starts the interface with DHCP.
   *
   * Joining happens in the background and is retried if it fails or the link
   * is lost. Use waitForLink() or waitForLocalIP() to wait for it, or status()
   * to check it.
   *
   * The first call boots the WiFi chip, which takes about a second.
   *
   * @param ssid       The network name, 1-32 characters.
   * @param passphrase The passphrase. If NULL or empty, the network is assumed
   *                   to be open; otherwise WPA2 is used.
   * @return Whether the credentials were valid (errno is EINVAL if not) and
   *         the interface was started.
   * @see setCredentials()
   */
  bool begin(const char* ssid, const char* passphrase = nullptr);

  /**
   * @brief Sets the network, with an explicit security type, and starts the
   *        interface with DHCP.
   *
   * @param ssid       The network name, 1-32 characters.
   * @param passphrase The passphrase, 8-63 characters for a secured network.
   *                   Ignored for an open network; may be NULL.
   * @param security   The security type.
   * @return Whether the credentials were valid (errno is EINVAL if not) and
   *         the interface was started.
   * @see begin(const char*, const char*)
   */
  bool begin(const char* ssid, const char* passphrase, Security security);

  /**
   * @brief Leaves the network and stops the interface.
   *
   * This is the same as end().
   */
  void disconnect() {
    end();
  }

  // --------------------------------------------------------------------------
  //  Connection Information
  // --------------------------------------------------------------------------

  /**
   * @brief Returns the connection status.
   *
   * This reflects joining the network, not whether an IP address has been
   * assigned; use localIP() or waitForLocalIP() for that.
   *
   * @return The status. This is WL_NO_SHIELD only if the chip was probed and
   *         not found.
   */
  wl_status_t status() const;

  /**
   * @brief Returns the SSID of the configured network.
   *
   * @return The SSID, or an empty string if none has been set.
   */
  const char* SSID() const;

  /**
   * @brief Returns the signal strength of the joined network.
   *
   * @return The RSSI in dBm, or zero if not joined.
   */
  int32_t RSSI() const;

  /**
   * @brief Gets the BSSID (the access point's MAC address) of the joined
   *        network.
   *
   * @param bssid Filled in with the BSSID.
   * @return `bssid`, or NULL if not joined or `bssid` is NULL.
   */
  uint8_t* BSSID(uint8_t bssid[6]) const;

  // --------------------------------------------------------------------------
  //  Scanning
  // --------------------------------------------------------------------------

  /**
   * @brief Scans for networks.
   *
   * This blocks, so the network stack doesn't run while scanning; a few frames
   * received over WiFi are queued and the rest are dropped. This boots the
   * chip if needed and doesn't require the interface to be started.
   *
   * Duplicate access points are removed and hidden networks are skipped. Up to
   * 59 networks are kept.
   *
   * @param timeout The maximum time to wait, in milliseconds.
   * @return The number of networks found, or -1 on error.
   * @see scanResult()
   */
  int scanNetworks(uint32_t timeout = kDefaultScanTimeout);

  /**
   * @brief Gets one result from the last scan.
   *
   * @param index  The result index, in [0, scanNetworks() result).
   * @param result Filled in with the result.
   * @return Whether the index was in range.
   */
  bool scanResult(int index, ScanResult& result) const;

  /**
   * @brief Returns the SSID of a network from the last scan.
   *
   * @param index The result index.
   * @return The SSID, or an empty string if the index is out of range. The
   *         string is valid until the next call.
   */
  const char* SSID(int index);

  /**
   * @brief Returns the signal strength of a network from the last scan.
   *
   * @param index The result index.
   * @return The RSSI in dBm, or zero if the index is out of range.
   */
  int32_t RSSI(int index) const;

  /**
   * @brief Returns the channel of a network from the last scan.
   *
   * @param index The result index.
   * @return The channel, or zero if the index is out of range.
   */
  uint8_t channel(int index) const;

  /**
   * @brief Gets the BSSID of a network from the last scan.
   *
   * @param index The result index.
   * @param bssid Filled in with the BSSID.
   * @return `bssid`, or NULL if the index is out of range or `bssid` is NULL.
   */
  uint8_t* BSSID(int index, uint8_t bssid[6]) const;

  /**
   * @brief Returns the security of a network from the last scan.
   *
   * @param index The result index.
   * @return A combination of driver_cyw4343w::ScanSecurityBits, or zero (open)
   *         if the index is out of range.
   */
  uint8_t encryptionType(int index) const;

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

/** @brief The instance for interacting with WiFi. */
STATIC_INIT_DECL(WiFiClass, WiFi);

/**
 * @brief A TCP client. This is the same type as EthernetClient.
 *
 * Sockets aren't tied to an interface: outgoing traffic leaves through the
 * interface whose subnet holds the destination, otherwise through the
 * default interface (see EthernetClass::setAsDefaultInterface()).
 */
using WiFiClient = EthernetClient;

/**
 * @brief A TCP server. This is the same type as EthernetServer.
 *
 * A server accepts connections on all started interfaces.
 */
using WiFiServer = EthernetServer;

/**
 * @brief A UDP socket. This is the same type as EthernetUDP.
 *
 * A socket receives on all started interfaces, and multicast groups are joined
 * on all started interfaces.
 */
using WiFiUDP    = EthernetUDP;

}  // namespace network
}  // namespace qindesign

#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS
