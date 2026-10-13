# WiFiScan

Scans for WiFi networks every five seconds and prints a table of what it finds. It doesn't join a network, so it's the best first check that the chip boots and the radio works.

## What it does

1. Reads the MAC address, which boots the WiFi chip.
2. Calls `WiFi.scanNetworks()` and prints each network's SSID, RSSI, channel, BSSID, and security.
3. Waits five seconds and scans again.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

No network configuration is needed.

## Expected output

```
MAC address: 00:11:22:33:44:55

Scanning...
Found 7 networks
 #  SSID                              RSSI  Ch  BSSID              Security
 1  MyNetwork                          -48   6  AA:BB:CC:DD:EE:01  WPA2
 2  Neighbor                           -81  11  AA:BB:CC:DD:EE:02  WPA/WPA2
```

## What to check

- A non-zero MAC address means the chip booted.
- Your own network shows up with a reasonable RSSI (above about -70 dBm is good).
- Repeated scans give similar results.
- Hidden networks are left out, and at most 59 networks are kept.

## Notes

- `scanNetworks()` blocks for a few seconds. The network stack doesn't run during the scan, so if WiFi is also started, a few received packets are queued and the rest are dropped.
- `kScanInterval` sets the time between scans.
