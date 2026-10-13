# WiFiLinkMonitor

Joins a WiFi network and logs every link, address, interface, and status change with a timestamp, plus a status line with the RSSI every five seconds. Use it to test that WiFi recovers on its own.

## What it does

1. Registers `onLinkState()`, `onAddressChanged()`, and `onInterfaceStatus()` callbacks on `WiFi`.
2. Calls `WiFi.begin(ssid, passphrase)`.
3. Logs each `WiFi.status()` change, and how long the link was up or down at each change.
4. Every five seconds, prints the status, RSSI, IP address, and counts of link-ups, link-downs, and address changes.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

`kReportInterval` sets the time between status lines.

## Tests to run

- **Access point restart:** power-cycle the access point. The link should go down, the status should cycle through `IDLE`/`CONNECT_FAILED`, and the link and address should come back by themselves once the access point is up. The driver retries every 10 seconds after a failure.
- **Range:** move away from the access point and back, watching the RSSI and any link drops.
- **Wrong passphrase:** set a wrong passphrase. The status should show `CONNECT_FAILED` and retry, without hanging.

## Expected output

```
[    1.205] Joining "mySSID"...
[    2.240] Interface UP
[    2.251] Status: DISCONNECTED -> IDLE
[    3.402] Status: IDLE -> CONNECTED
[    3.405] Link UP (was down 2195 ms)
[    5.811] Address: 192.168.1.42
[    6.210] CONNECTED, RSSI -51 dBm, IP 192.168.1.42, ups=1 downs=0 addr=1
```
