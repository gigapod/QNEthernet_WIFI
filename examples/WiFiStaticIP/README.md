# WiFiStaticIP

Joins a WiFi network with a static IP address instead of DHCP, then pings the gateway every two seconds to show the address works.

## What it does

1. Chooses the network with `WiFi.setCredentials()`. Open networks use `Security::kOpen`; otherwise WPA2.
2. Starts the interface with `WiFi.begin(ip, subnet, gateway, dns)`.
3. Waits for the link with `WiFi.waitForLink()`. With a static address there's no DHCP step, so the address is set immediately.
4. Prints the address settings and whether DHCP is active (it shouldn't be).
5. Pings the gateway every two seconds.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

Also set `kIP`, `kSubnet`, `kGateway`, and `kDNS` to match your network. Choose an address outside your router's DHCP range.

## What to check

- `DHCP active: no`.
- Gateway pings succeed. If they fail, check that the address, subnet, and gateway match your network.
- If the link never comes up, run WiFiConnect first to check the SSID and passphrase.
