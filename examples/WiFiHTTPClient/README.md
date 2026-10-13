# WiFiHTTPClient

Joins a WiFi network and fetches a web page over TCP every ten seconds, printing the HTTP status line, the bytes received, and the timing. Use it to test DNS and TCP over WiFi.

## What it does

1. Joins the network with DHCP.
2. Every ten seconds, connects to `HTTP_HOST` port 80 with a `WiFiClient`, sends a `GET` for `HTTP_PATH` with `Connection: close`, and reads until the server closes the connection.
3. Prints the status line, the total bytes, the connect and transfer times, the transfer rate, and a running failure count.

## Requirements

- Teensy 4.1 with a CYW4343W or CYW43439 WiFi board
- `ARDUINO_TEENSY41_CYW4343W` defined to `1`

## Configuration

Set `WIFI_SSID` and `WIFI_PASSPHRASE` in the sketch, or with build flags:

```
-DWIFI_SSID=\"mySSID\" -DWIFI_PASSPHRASE=\"myPassphrase\"
```

An empty passphrase joins an open network.

Set `HTTP_HOST` (default `example.com`) and `HTTP_PATH` (default `/`) the same way. Use a plain-HTTP server; HTTPS isn't supported by this example.

## Expected output

```
[1] Connecting to example.com:80...
  "HTTP/1.1 200 OK"
  1591 bytes; connect 48 ms, transfer 52 ms (30 kB/s); failures=0
```

## What to check

- The status line is `HTTP/1.1 200 OK` (or a redirect such as 301, which still shows TCP works).
- The failure count stays at zero over many fetches.
- To test larger transfers, point `HTTP_HOST`/`HTTP_PATH` at a larger file on a local web server.
