# WiFiPing

Joins a WiFi network and pings the gateway and a host by name once a second, printing every result and a summary every 20 rounds. Use it to test the WiFi data path and DNS over time.

## What it does

1. Joins the network with DHCP.
2. Looks up `PING_HOST` (default `pjrc.com`) with `WiFi.hostByName()`.
3. Every second, pings the gateway and the host.
4. Every 20 rounds, prints sent, received, lost, and min/avg/max times for each target, plus the RSSI.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

Set `PING_HOST` the same way to ping a different host. `kPingInterval` and `kReportEvery` set the timing.

## What to check

- Gateway pings succeed with low, steady times (typically 2-10 ms).
- The DNS lookup succeeds, which shows DNS and UDP work over WiFi.
- Packet loss stays near zero. Occasional loss on a busy channel is normal; steady loss suggests a receive-path problem.
- Leave it running for a while, including through DHCP lease renewals, to check long-term stability.

## Notes

DNS servers and the ping code are shared with `Ethernet`, so if both interfaces are started, a ping goes out the interface whose subnet holds the address, otherwise the default interface.
