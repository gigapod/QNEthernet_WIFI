// SPDX-FileCopyrightText: (c) 2026 SparkFun Electronics
// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file driver_CYW4343W_wifi.h
 * @brief Declares the WiFi-specific functions of the CYW4343W driver.
 *
 * These are used by WiFiClass. The driver's general functions are in its
 * DriverOps table (driver_cyw4343w::kDriverOps).
 */

#pragma once

// C++ includes
#include <cstddef>
#include <cstdint>

namespace qindesign {
namespace network {
/** @brief The CYW4343W/CYW43439 WiFi driver. */
namespace driver_cyw4343w {

/**
 * @brief Security type for joining a network.
 *
 * The values match what the chip's join code expects.
 */
enum class Security : uint8_t {
  kOpen    = 0,  ///< No security.
  kWPA     = 1,  ///< WPA-TKIP.
  kWPA2    = 2,  ///< WPA2-AES.
};

/**
 * @brief Security bits for a scan result.
 *
 * A network may advertise more than one, for example WPA and WPA2.
 */
enum ScanSecurityBits : uint8_t {
  kScanSecurityOpen = 0,         ///< No security.
  kScanSecurityWEP  = (1 << 0),  ///< WEP.
  kScanSecurityWPA  = (1 << 1),  ///< WPA.
  kScanSecurityWPA2 = (1 << 2),  ///< WPA2.
  kScanSecurityWPA3 = (1 << 3),  ///< WPA3 (SAE).
};

/** @brief The state of joining a network. */
enum class JoinStatus : uint8_t {
  kStopped,  ///< Not trying to join.
  kJoining,  ///< Trying to join (or rejoin).
  kJoined,   ///< Joined and authenticated.
  kFailed,   ///< The last attempt failed; it will be retried.
};

/** @brief One network found by a scan. */
struct ScanResult {
  char ssid[33];     ///< The network name, NUL-terminated.
  uint8_t bssid[6];  ///< The access point's MAC address.
  int16_t rssi;      ///< The signal strength, in dBm.
  uint8_t channel;   ///< The radio channel.
  uint8_t security;  ///< A combination of ScanSecurityBits.
};

/** @brief The maximum SSID length, not including a NUL terminator. */
constexpr size_t kMaxSSIDLen       = 32;

/** @brief The minimum passphrase length for a secured network. */
constexpr size_t kMinPassphraseLen = 8;

/**
 * @brief The maximum passphrase length, not including a NUL terminator.
 */
constexpr size_t kMaxPassphraseLen = 63;

/**
 * @brief Sets the network to join.
 *
 * This takes effect the next time the driver is initialized, that is, when
 * the interface is started.
 *
 * @param ssid       The network name, 1 to kMaxSSIDLen characters.
 * @param passphrase The passphrase, kMinPassphraseLen to kMaxPassphraseLen
 *                   characters. Ignored for an open network; may be NULL.
 * @param security   The security type.
 * @return Whether the SSID and passphrase are valid for the security type.
 */
bool set_credentials(const char* ssid, const char* passphrase,
                     Security security);

/**
 * @brief Returns the SSID set by set_credentials().
 *
 * @return The SSID, or an empty string if none has been set.
 */
const char* ssid();

/**
 * @brief Returns the state of joining the network.
 *
 * @return The join state.
 */
JoinStatus join_status();

/**
 * @brief Gets the signal strength of the joined network.
 *
 * @param value Filled in with the RSSI, in dBm.
 * @return Whether successful. This is false if not joined or on error.
 */
bool rssi(int32_t* value);

/**
 * @brief Gets the BSSID (the access point's MAC address) of the
 *        joined network.
 *
 * @param mac Filled in with the BSSID.
 * @return Whether successful. This is false if not joined or on error.
 */
bool bssid(uint8_t mac[6]);

/**
 * @brief Scans for networks.
 *
 * This boots the chip if needed. It blocks, so the network stack doesn't run
 * while scanning; a few received data frames are queued and the rest
 * are dropped.
 *
 * @param timeout The maximum time to wait, in milliseconds.
 * @return The number of networks found, or -1 on error.
 */
int scan(uint32_t timeout);

/**
 * @brief Gets one result from the last scan.
 *
 * @param index  The result index, in [0, scan() result).
 * @param result Filled in with the result.
 * @return Whether the index was in range and `result` isn't NULL.
 */
bool scan_result(int index, ScanResult* result);

}  // namespace driver_cyw4343w
}  // namespace network
}  // namespace qindesign
