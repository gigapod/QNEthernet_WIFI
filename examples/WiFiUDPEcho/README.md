# WiFiUDPEcho

Joins a WiFi network, echoes UDP packets back to their sender, and answers TCP connections with a one-line greeting. Use it to test receiving and sending over WiFi from another computer.

## What it does

1. Joins the network with DHCP and prints the address.
2. Listens for UDP on port 8080 and sends every packet back to where it came from.
3. Listens for TCP on port 8081, sends `Hello from Teensy over WiFi`, and closes the connection.

Both listen on all started interfaces, so they also answer over wired Ethernet if it's started.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

## Testing from another computer

```
echo -n "hello" | nc -4u -w1 <teensy-ip> 8080   # Prints "hello" back
nc <teensy-ip> 8081                              # Prints the greeting
```

The Teensy prints each UDP packet's size and sender.

## What to check

- Every UDP packet is echoed. Send a burst (for example, a loop of 100 packets) to check for drops.
- TCP connections get the greeting and close cleanly.
