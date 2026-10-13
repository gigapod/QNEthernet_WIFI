# WiFiConnect

The first test for a WiFi setup: joins a network with DHCP and reports each step.

## What it does

1. Calls `WiFi.begin(ssid, passphrase)`. The first call boots the WiFi chip.
2. Prints every `WiFi.status()` change and the moment the link comes up while waiting for an address.
3. Prints the connection details: interface name, SSID, BSSID, RSSI, MAC, IP, subnet, gateway, DNS, and hostname.
4. Prints how long `begin()`, the link, and the address each took.
5. Keeps printing status changes, so you can see what happens when the access point goes away and comes back.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network. `kTimeout` is how long to wait for an address.

## Expected output

```
WiFiConnect
Joining "mySSID"...
[  1043 ms] status: IDLE (joining)
[  3120 ms] status: CONNECTED
[  3125 ms] link up

Connected:
  Interface  wl0
  SSID       mySSID
  BSSID      AA:BB:CC:DD:EE:FF
  RSSI       -52 dBm
  ...
  IP         192.168.1.42
```

## What to check

- `begin()` returns in about a second; that is the chip booting.
- The status goes `IDLE` → `CONNECTED`. `CONNECT_FAILED` usually means a wrong passphrase or SSID; the driver retries every 10 seconds.
- `NO_SHIELD` means the chip wasn't found: check the wiring and the board selection in `misc_defs.h`.
- An address arrives a few seconds after the link comes up (DHCP).
