# WiFiBeginEndCycle

Starts and stops WiFi repeatedly (20 times by default), checking each time that an address is assigned and the gateway answers a ping, then prints a summary. Use it to test that starting and stopping the interface is reliable.

## What it does

For each cycle:

1. Calls `WiFi.begin(ssid, passphrase)`.
2. Waits up to 30 seconds for an address.
3. Pings the gateway once.
4. Calls `WiFi.disconnect()` and waits two seconds.

Then prints how many cycles fully succeeded, the counts of each kind of failure, and the min/avg/max time to get an address.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

`kCycles`, `kDHCPTimeout`, and `kOffTime` set the number of cycles and the timing.

## Expected output

```
Cycle 1: address 192.168.1.42 after 3210 ms, ping ok
Cycle 2: address 192.168.1.42 after 2480 ms, ping ok
...
Summary:
  OK (address and ping): 20/20
  begin() failures:      0
  No address:            0
  Ping failures:         0
  Time to address min/avg/max: 2310/2655/3210 ms
```

## What to check

- All cycles succeed. The first cycle is slower because it boots the chip.
- Times to an address stay consistent; times growing from cycle to cycle suggest a resource leak.
- The program doesn't hang. A hang during `begin()` or `disconnect()` is a bug worth reporting with the cycle number.
