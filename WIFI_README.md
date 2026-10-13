# Using WiFi with QNEthernet

This library adds a `WiFi` interface for the Teensy 4.1. It uses a CYW43439 (SparkFun shield) or CYW4343W (Dogbone06 board) WiFi chip connected over SDIO. `WiFi` runs on the same lwIP network stack as the wired `Ethernet` interface, so you can use either one alone or both at once. It also works with the same client, server, and UDP classes.

## Contents

- [Hardware](#hardware)
- [Enabling WiFi](#enabling-wifi)
- [Quick start](#quick-start)
- [Joining a network](#joining-a-network)
- [Status and connection information](#status-and-connection-information)
- [Scanning](#scanning)
- [Events](#events)
- [Sockets](#sockets)
- [Using Ethernet and WiFi together](#using-ethernet-and-wifi-together)
- [Multicast](#multicast)
- [Stopping WiFi](#stopping-wifi)
- [Limitations](#limitations)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)

## Hardware

The WiFi chip uses the Teensy 4.1's second SDIO port (uSDHC2). The data and clock pins are the same for both boards; the interrupt and power-enable pins are different.

| Teensy 4.1 pin | Signal | CYW43439 (SparkFun) | CYW4343W (Dogbone06) |
|---|---|---|---|
| 23 | CLK | ✓ | ✓ |
| 22 | CMD | ✓ | ✓ |
| 40 | D0 | ✓ | ✓ |
| 41 | D1 | ✓ | ✓ |
| 17 | D2 | ✓ | ✓ |
| 16 | D3 | ✓ | ✓ |
| — | INT | 29 | 34 |
| — | WL_ON | 30 | 33 |

The board is chosen in `src/qnethernet/drivers/cyw4343w/src/misc_defs.h`. The default is the SparkFun CYW43439:

```c++
#define WIFI_DEVICE CYW43439  // or CYW4343W
```

## Enabling WiFi

WiFi is compiled in when `ARDUINO_TEENSY41_CYW4343W` is `1`, which is the default in `src/qnethernet_opts.h`. Then both interfaces exist:

- `Ethernet`: the Teensy 4.1's wired Ethernet.
- `WiFi`: the CYW43439/CYW4343W interface.

Set it to `0` to build an Ethernet-only library. This saves about 10 KB of RAM1.

To change the setting, either edit `src/qnethernet_opts.h` (the simplest option in the Arduino IDE) or pass a build flag. In PlatformIO:

```ini
build_flags = -DARDUINO_TEENSY41_CYW4343W=1
```

Your code can check whether WiFi is available:

```c++
#if !defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
#error "Define ARDUINO_TEENSY41_CYW4343W=1 to compile in WiFi"
#endif
```

## Quick start

```c++
#include <QNEthernet.h>

using namespace qindesign::network;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {}

  // Join a WPA2 network with DHCP. The first call boots the chip (~1 s).
  if (!WiFi.begin("yourSSID", "yourPassphrase")) {
    Serial.printf("begin() failed: errno=%d\r\n", errno);
    return;
  }

  if (WiFi.waitForLocalIP(15000)) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.printf("No address yet (status=%u)\r\n", WiFi.status());
  }
}

void loop() {
  // The stack is serviced from yield(), which runs after every loop() and
  // during delay(). Don't block for long without calling yield().
}
```

`WiFi.waitForLocalIP()`, `delay()`, and returning from `loop()` all call `yield()`, which runs the network stack. If your code blocks for a long time, call `yield()` or `WiFi.loop()` regularly.

## Joining a network

The network is chosen at runtime. There is no secrets file.

### DHCP

```c++
WiFi.begin(ssid, passphrase);                               // WPA2, or open if passphrase is NULL or ""
WiFi.begin(ssid, passphrase, WiFiClass::Security::kWPA);    // Explicit security type
```

Security types (`WiFiClass::Security`):

| Value | Meaning |
|---|---|
| `kOpen` | No security; the passphrase is ignored |
| `kWPA` | WPA-TKIP |
| `kWPA2` | WPA2-AES (default) |

The SSID must be 1–32 characters. A passphrase for a secured network must be 8–63 characters. If the credentials aren't valid, `begin()` returns `false` and sets `errno` to `EINVAL`.

### Static address

Set the credentials first, then call one of the static-address `begin()` overloads inherited from `EthernetClass`:

```c++
WiFi.setCredentials("yourSSID", "yourPassphrase", WiFiClass::Security::kWPA2);
WiFi.begin(IPAddress{192, 168, 1, 50},    // IP
           IPAddress{255, 255, 255, 0},   // Subnet
           IPAddress{192, 168, 1, 1},     // Gateway
           IPAddress{192, 168, 1, 1});    // DNS (optional)
WiFi.waitForLink(15000);                  // The address is set right away; wait for the join
```

### Background joining and retries

`begin()` returns as soon as the interface has started. Joining happens in the background. If an attempt fails, or the link drops later (for example, the access point restarts), the driver keeps retrying about every 10 seconds until you call `end()`.

To change networks, call `setCredentials()` (or `begin(ssid, ...)`) and then `begin()` again.

## Status and connection information

`WiFi.status()` reports the join state. It doesn't tell you whether an address has been assigned; use `localIP()` or `waitForLocalIP()` for that.

| Status | Meaning |
|---|---|
| `WL_IDLE_STATUS` | Joining (or rejoining) |
| `WL_CONNECTED` | Joined and authenticated |
| `WL_CONNECT_FAILED` | The last attempt failed; it will be retried. Usually a wrong SSID or passphrase |
| `WL_DISCONNECTED` | Not trying to join (not started, or stopped) |
| `WL_NO_SHIELD` | The chip was probed and not found |

Connection details:

```c++
WiFi.SSID();              // The configured SSID
WiFi.RSSI();              // Signal strength in dBm, or 0 if not joined
uint8_t bssid[6];
WiFi.BSSID(bssid);        // Access point MAC; returns NULL if not joined
WiFi.macAddress();        // This chip's MAC address (fixed; it can't be changed)
WiFi.interfaceName();     // "wl" plus the interface number, e.g. "wl1"
```

The usual `EthernetClass` address functions all work on `WiFi`: `localIP()`, `subnetMask()`, `gatewayIP()`, `dnsServerIP()`, `broadcastIP()`, `isDHCPActive()`, `renewDHCP()`, `setHostname()`, `hostname()`, `linkState()`, `hostByName()`, and `ping()`.

## Scanning

`scanNetworks()` boots the chip if needed. The interface doesn't have to be started.

```c++
int n = WiFi.scanNetworks();       // Blocks, up to 10 s by default
for (int i = 0; i < n; i++) {
  WiFiClass::ScanResult r;
  if (WiFi.scanResult(i, r)) {
    Serial.printf("%-32s %4d dBm  ch %2u  sec 0x%02x\r\n",
                  r.ssid, r.rssi, r.channel, r.security);
  }
}
```

There are also Arduino-style accessors: `SSID(i)`, `RSSI(i)`, `channel(i)`, `BSSID(i, mac)`, and `encryptionType(i)`. The security value is a bitmask of `driver_cyw4343w::ScanSecurityBits` (`kScanSecurityWEP`, `kScanSecurityWPA`, `kScanSecurityWPA2`, `kScanSecurityWPA3`). A value of `0` means open.

Notes:

- Duplicate access points are removed and hidden networks are skipped. Up to 59 results are kept.
- `scanNetworks()` returns `-1` on error.
- While a scan runs, the network stack doesn't run. A few frames received over WiFi are queued and the rest are dropped. Avoid scanning during time-sensitive traffic.

## Events

The `EthernetClass` callbacks are available on `WiFi`:

```c++
WiFi.onLinkState([](bool up) {
  Serial.printf("[WiFi] link %s\r\n", up ? "up" : "down");
});
WiFi.onAddressChanged([]() {
  Serial.print("[WiFi] address: ");
  Serial.println(WiFi.localIP());
});
WiFi.onInterfaceStatus([](bool up) {
  Serial.printf("[WiFi] interface %s\r\n", up ? "up" : "down");
});
```

On WiFi, "link up" means the chip has joined and authenticated with the access point.

## Sockets

`WiFiClient`, `WiFiServer`, and `WiFiUDP` are the same types as `EthernetClient`, `EthernetServer`, and `EthernetUDP`. Code written for Ethernet works unchanged:

```c++
WiFiClient client;
if (client.connect("example.com", 80)) {
  client.writeFully("GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
}
```

Sockets aren't bound to an interface:

- Outgoing traffic leaves through the interface whose subnet contains the destination. Otherwise it uses the default interface.
- Servers and UDP sockets accept traffic on all started interfaces.

## Using Ethernet and WiFi together

Start each interface independently:

```c++
Ethernet.begin();                          // Wired, DHCP
WiFi.begin("yourSSID", "yourPassphrase");  // WiFi, DHCP

Ethernet.waitForLocalIP(15000);
WiFi.waitForLocalIP(15000);
```

Keep these in mind:

- **Default route:** `Ethernet` becomes the default interface whenever it starts. To send off-subnet traffic (such as internet traffic) over WiFi instead, call `WiFi.setAsDefaultInterface()`. You can switch back with `Ethernet.setAsDefaultInterface()`.
- **Shared stack:** `Ethernet.loop()` and `WiFi.loop()` both service every interface, and both are called from `yield()`.
- **Shared DNS:** DNS servers and `hostByName()` are global in lwIP. Whichever interface last set a DNS server (for example, through DHCP) sets it for both.
- **Same subnet:** If both interfaces are on the same subnet, traffic to that subnet goes out the first matching interface. Put them on different networks for predictable routing.

See [examples/EthernetAndWiFi](examples/EthernetAndWiFi).

## Multicast

`joinGroup()`/`leaveGroup()` and `WiFiUDP::beginMulticast()` work over WiFi. Groups are joined on all started interfaces.

The chip's hardware filter accepts at most **10 multicast MAC addresses** at once. The IGMP all-systems group and mDNS each use one. Promiscuous mode (`QNETHERNET_ENABLE_PROMISCUOUS_MODE`) receives all multicast instead.

## Stopping WiFi

```c++
WiFi.end();         // or WiFi.disconnect()
```

This leaves the network, stops retrying, and takes the interface down. You can call `begin()` again later.

## Limitations

These `EthernetClass` features aren't available on `WiFi`:

- Link settings (speed, duplex, crossover) and the SPI chip-select setting
- Changing the MAC address
- Raw Ethernet frames (`EthernetFrame`), which go to the primary (`Ethernet`) interface only

Also:

- mDNS isn't per-interface yet.
- Access-point (soft AP) mode isn't supported; WiFi works as a station only.
- WPA3-only networks can be scanned but not joined.
- The MTU is 1500 for both interfaces.

## Examples

Each example has a README that explains what it does and what to check. Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

| Example | Shows |
|---|---|
| [WiFiScan](examples/WiFiScan) | Booting the chip and scanning, without joining |
| [WiFiConnect](examples/WiFiConnect) | Joining with DHCP, status changes, connection details, timing |
| [WiFiStaticIP](examples/WiFiStaticIP) | Joining with a static address |
| [WiFiLinkMonitor](examples/WiFiLinkMonitor) | Link, address, and status events; rejoining after the AP drops |
| [WiFiPing](examples/WiFiPing) | Ping and DNS, with loss and latency statistics |
| [WiFiHTTPClient](examples/WiFiHTTPClient) | DNS lookup and a TCP client |
| [WiFiUDPEcho](examples/WiFiUDPEcho) | UDP and TCP servers |
| [WiFiMulticast](examples/WiFiMulticast) | Multicast receive and send |
| [WiFiTCPThroughput](examples/WiFiTCPThroughput) | Receive throughput |
| [WiFiBeginEndCycle](examples/WiFiBeginEndCycle) | Starting and stopping WiFi repeatedly |
| [EthernetAndWiFi](examples/EthernetAndWiFi) | Wired Ethernet and WiFi together |

A good order to try them in: WiFiScan → WiFiConnect → WiFiPing, then the rest.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `status()` is `WL_NO_SHIELD` | The chip wasn't found. Check the wiring and the `WIFI_DEVICE` setting in `misc_defs.h` (INT/WL_ON pins differ between boards) |
| `begin()` returns `false` with `errno == EINVAL` | SSID empty or too long, or passphrase not 8–63 characters for a secured network |
| `status()` stays `WL_CONNECT_FAILED` | Wrong SSID or passphrase, wrong security type, or a WPA3-only network |
| `WL_CONNECTED`, but no address | DHCP hasn't finished, or there's no DHCP server. Wait longer or use a static address |
| Traffic goes out the wrong interface | Ethernet is the default route; call `WiFi.setAsDefaultInterface()` |
| Some multicast groups aren't received | The chip's 10-address filter is full |

The full API reference is in the Doxygen comments in `src/QNWiFi.h` and `src/qnethernet/drivers/driver_CYW4343W_wifi.h`.
