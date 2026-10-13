# WiFiTCPThroughput

Joins a WiFi network and runs a TCP server on port 5001 that receives data as fast as the sender can send it, printing the throughput each second and for each connection. Use it to load-test the WiFi receive path.

## What it does

1. Joins the network with DHCP and listens on port 5001.
2. For each connection, reads and discards all data until the sender closes the connection.
3. Prints the rate for each one-second interval and the total for the connection, in Mbit/s.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

`kPort` and `kProgressInterval` set the port and progress interval.

## Testing from another computer

```
# Send 10 MB
head -c 10000000 /dev/zero | nc -N <teensy-ip> 5001

# Or with pv to watch progress
head -c 50000000 /dev/zero | pv | nc -N <teensy-ip> 5001
```

(Use `nc -q0` instead of `-N` on some systems.)

## Expected output

```
Connection from 192.168.1.10
  interval: 1310720 bytes in 1000 ms = 10.485 Mbit/s
  ...
Total: 10000000 bytes in 7710 ms = 10.376 Mbit/s
```

## What to check

- The total byte count matches what was sent; any difference means lost data.
- The transfer completes without stalling. Stalls usually show up as intervals near zero.
- Repeat several transfers back to back to check that the receive path stays stable under load.

## Notes

The rate depends on the signal, the access point, and the SDIO clock. It's useful for comparing runs on the same setup rather than as an absolute figure.
