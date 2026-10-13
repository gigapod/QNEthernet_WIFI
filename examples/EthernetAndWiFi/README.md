# EthernetAndWiFi

Brings up wired Ethernet and WiFi at the same time, prints each interface's address as it arrives, and pings both gateways every two seconds. Use it to test running both interfaces together.

## What it does

1. Sets link and address callbacks on both `Ethernet` and `WiFi`.
2. Starts `Ethernet` with DHCP and `WiFi` with DHCP on the configured network.
3. Waits for both addresses, then prints the WiFi SSID, BSSID, and RSSI.
4. Every two seconds, pings each interface's gateway.

## Requirements

- Teensy 4.1 with the Ethernet kit connected to a network
- A CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

## Expected output

```
[Ethernet] Link ON
[Ethernet] en0: IP=192.168.1.40 gateway=192.168.1.1
[WiFi] Link ON
[WiFi] wl1: IP=192.168.1.42 gateway=192.168.1.1
[WiFi] SSID=mySSID BSSID=AA:BB:CC:DD:EE:FF RSSI=-50 dBm
[Ethernet] Ping 192.168.1.1: 1 ms
[WiFi] Ping 192.168.1.1: 4 ms
```

## What to check

- Both interfaces get addresses and their pings succeed.
- Unplug the Ethernet cable: WiFi keeps working. Plug it back in: Ethernet recovers.
- Stop the access point: Ethernet keeps working, and WiFi rejoins when the access point returns.

## Notes

- Each ping goes out the interface whose subnet holds the address. If both interfaces are on the same subnet, as in the example output, both pings may use the same interface (the default, which is `Ethernet`). For a true per-interface test, put them on different networks.
- `Ethernet` is the default route whenever it's started; use `setAsDefaultInterface()` to change that.
