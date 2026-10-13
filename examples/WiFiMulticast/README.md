# WiFiMulticast

Joins a WiFi network and the UDP multicast group 239.255.0.1 port 5007, prints every packet received on the group, and sends a numbered heartbeat to the group every five seconds. Use it to test that WiFi receives multicast.

## What it does

1. Joins the network with DHCP.
2. Calls `udp.beginMulticast()`, which joins the group on all started interfaces and adds the group's MAC address to the WiFi chip's multicast filter.
3. Prints each packet received on the group with its sender.
4. Sends `heartbeat N from <ip>` to the group every five seconds.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

`kGroup`, `kPort`, and `kHeartbeatInterval` set the group, port, and heartbeat timing.

## Testing from another computer

On a computer on the same network:

```
# Receive the Teensy's heartbeats
socat -u UDP4-RECV:5007,ip-add-membership=239.255.0.1:0.0.0.0 -

# Send to the group; the Teensy should print it
echo "hello group" | socat -u - UDP4-DATAGRAM:239.255.0.1:5007
```

## What to check

- Packets sent to the group from another computer show up on the Teensy. This is the important test: it shows the chip's multicast filter (`mcast_list`) is set.
- The computer receives the Teensy's heartbeats, which shows multicast sending works.
- After power-cycling the access point, received multicast still works once the link is back (the filter is re-sent on each link-up).

## Notes

- Join the group after the interface has started; an interface started later isn't added to the group.
- The WiFi chip accepts up to 10 multicast MAC addresses. The IGMP all-systems group uses one, and mDNS uses one if enabled.
- Some access points filter multicast between wireless clients or need IGMP snooping configured. If nothing arrives, test from a wired computer too.
