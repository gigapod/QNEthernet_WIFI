// driver_CYW4343W_wifi.h declares the WiFi-specific functions of the CYW4343W
// driver, used by WiFiClass.

#pragma once

// C++ includes
#include <cstddef>
#include <cstdint>

namespace qindesign {
namespace network {
namespace driver_cyw4343w {

// Security types for joining a network. The values match what the
// join code expects.
enum class Security : uint8_t {
  kOpen    = 0,
  kWPA     = 1,  // WPA-TKIP
  kWPA2    = 2,  // WPA2-AES
};

// Security bits for a scan result.
enum ScanSecurityBits : uint8_t {
  kScanSecurityOpen = 0,
  kScanSecurityWEP  = (1 << 0),
  kScanSecurityWPA  = (1 << 1),
  kScanSecurityWPA2 = (1 << 2),
  kScanSecurityWPA3 = (1 << 3),
};

// Join state.
enum class JoinStatus : uint8_t {
  kStopped,  // Not trying to join
  kJoining,  // Trying to join (or rejoin)
  kJoined,   // Joined and authenticated
  kFailed,   // The last attempt failed; it will be retried
};

// One network found by a scan.
struct ScanResult {
  char ssid[33];  // NUL-terminated
  uint8_t bssid[6];
  int16_t rssi;  // dBm
  uint8_t channel;
  uint8_t security;  // ScanSecurityBits
};

// Maximum lengths, not including any NUL terminator.
constexpr size_t kMaxSSIDLen       = 32;
constexpr size_t kMinPassphraseLen = 8;
constexpr size_t kMaxPassphraseLen = 63;

// Sets the network to join. This takes effect the next time the driver is
// initialized (when the interface is started). This returns false if the SSID
// is empty or too long or if the passphrase length is invalid for the security
// type; an open network ignores the passphrase.
bool set_credentials(const char* ssid, const char* passphrase,
                     Security security);

// Returns the SSID set by set_credentials(), or an empty string.
const char* ssid();

// Returns the join state.
JoinStatus join_status();

// Gets the RSSI of the joined network, in dBm. This returns false if not
// joined or on error.
bool rssi(int32_t* value);

// Gets the BSSID of the joined network. This returns false if not joined or
// on error.
bool bssid(uint8_t mac[6]);

// Scans for networks, waiting up to 'timeout' milliseconds. This boots the
// chip if needed. This returns the number of networks found, or -1 on error.
// While scanning, received frames are dropped.
int scan(uint32_t timeout);

// Gets one result from the last scan. This returns false if the index is out
// of range.
bool scan_result(int index, ScanResult* result);

}  // namespace driver_cyw4343w
}  // namespace network
}  // namespace qindesign
