# Dev Notes

## CYW4343W/CYW43439 ioctl hang after `WiFi.begin()`

Branch: `fix/cyw43-ioctl-irq-race` (commit 4407712)

### Symptom

On the CYW4343W, `examples/WiFiConnect` sometimes stalled and never connected
unless a `delay(1)` was added after `WiFi.begin()`. The delay doesn't meet any
chip requirement. It only shifts timing around a race in the driver.

### Cause

`W4343WCard::ioctl_cmd_poll_device()` (`src/qnethernet/drivers/cyw4343w/src/cyw43_T4_SDIO.cpp`)
waited for the ioctl response like this, with no timeout:

```cpp
while (dataISRReceived == false) {}
dataISRReceived = false;
ioctl_wait(IOCTL_WAIT_USEC);   // also spun with no timeout
```

- The WL_IRQ interrupt sets `dataISRReceived` (`onWLIRQInterruptHandler()`).
  The interrupt is attached on a FALLING edge.
- The interrupt fires for every chip interrupt, not just ioctl responses.
  Event and data frames trigger it too.
- Only the ioctl path clears the flag. `proc_input()` reads frames without
  touching it.

So the flag doesn't reliably mean "the response is ready":

- **Stale flag set:** an earlier event's interrupt left the flag set. The wait
  passes immediately, before the response exists.
- **Edge missed:** the IRQ line is still asserted from an event nobody has
  handled yet. The response creates no new edge, the ISR never runs, and the
  wait spins forever. This is the stall.

The window is widest right after `begin()`, when the chip sends a burst of
association, auth and link events while the stack is still sending ioctls,
such as the multicast list.

### Changes

`src/qnethernet/drivers/cyw4343w/src/ioctl_T4.h`

- Added `IOCTL_IRQ_WAIT_USEC` (5000): the longest wait for the response
  interrupt before falling back to polling.
- Added `IOCTL_BUSY_MAX_USEC` (100000): the longest wait for the bus to go
  ready.

`src/qnethernet/drivers/cyw4343w/src/cyw43_T4_SDIO.cpp`

1. **`ioctl_cmd()`:** clears `dataISRReceived` right before `cardCMD53_write()`,
   so an interrupt left over from an earlier frame can't count as the response.
2. **`ioctl_cmd_poll_device()`:** waits for the interrupt for at most
   `IOCTL_IRQ_WAIT_USEC`, then falls through to the existing
   `SB_INT_STATUS_REG` poll. That poll finds a pending response even when the
   edge was missed, and its write-back acknowledges the interrupt so later
   edges fire again.
3. **`ioctl_wait()`:** the GPIO ready spin now gives up after
   `IOCTL_BUSY_MAX_USEC`. It doesn't use its `usec` argument as the limit,
   because the original TODO notes the real wait often takes longer than the
   2 ms requested.

### Effect on boot and join time

- **Normal case:** the interrupt arrives within microseconds, so there's no
  change.
- **Missed edge:** costs at most about 5 ms plus one 2 ms poll step, instead of
  hanging.
- **Slow ioctls** (for example the `clmload` chunks during boot): these
  continue in the existing polling loop, adding at most one 2 ms poll step
  each.

### CYW43439

Both chips run the same ioctl and IRQ code. Only the firmware, CLM and NVRAM
(`WIFI_DEVICE` in `misc_defs.h`) and the pin numbers differ, and both use
SDIO2 with a FALLING-edge out-of-band IRQ. The CYW43439 has the same race and
gets the same fix.

### Considered and rejected: "duplicate join" removal

`join_start()` seems to repeat the whole configuration that `join_restart()`
sends, including `WLC_SET_SSID`. It doesn't. `CHECK(...)` in `cyw43_T4_SDIO.h`
expands to nothing, because its body is all comments and the `\` line
continuations sit inside those comments. Every `CHECK` line in `join_start()`
is a no-op, and `join_restart()` (called through `join_state_poll()`) is the
only code that sets up security and sends the SSID. Removing that call would
stop the board from joining.

### Testing (pending, needs hardware)

- [ ] CYW4343W: run `WiFiConnect` without the `delay(1)` after `begin()`, and
      connect several times from a cold power-up
- [ ] CYW43439: same test
- [ ] Compare the "begin() returned after" and "address after" times against
      the previous build

## `EthernetAndWiFi` assertion: "single netif already set"

Branch: `fix/cyw43-ioctl-irq-race`

### Symptom

On both the CYW4343W and the CYW43439, `examples/EthernetAndWiFi` stopped
right after starting WiFi:

```text
Assertion "single netif already set" failed at line 305 in .../src/lwip/netif.c
```

### Cause

`src/lwipopts.h` still had `LWIP_SINGLE_NETIF 1`, a setting carried over from
upstream QNEthernet. With it, lwIP allows only one network interface, so
`netif_add()` asserts when a second one is added. Ethernet starts first in this
example, so WiFi's interface is the one rejected. The WiFi-only examples never
add a second interface, which is why they worked. The multi-interface work
never changed this setting, so it affects both chips the same way.

### Change

`src/lwipopts.h`: `LWIP_SINGLE_NETIF` now depends on whether WiFi is compiled
in:

```c
#if defined(QNETHERNET_INTERNAL_MULTIPLE_DRIVERS)
// Ethernet and WiFi each add a netif
#define LWIP_SINGLE_NETIF              0
#else
#define LWIP_SINGLE_NETIF              1  /* 0 */
#endif  // QNETHERNET_INTERNAL_MULTIPLE_DRIVERS
```

- `lwipopts.h` already includes `qnethernet/driver_select.h`, which defines
  `QNETHERNET_INTERNAL_MULTIPLE_DRIVERS` whenever
  `ARDUINO_TEENSY41_CYW4343W=1`.
- Ethernet-only builds keep the original single-interface setting.
- With the setting at `0`, lwIP keeps a list of interfaces, and
  `ETHARP_TABLE_MATCH_NETIF` defaults to on, so each ARP entry is tied to its
  interface. That's what you want with two networks. It costs about 2 KB of
  code space.

### Build check

- With WiFi (`ARDUINO_TEENSY41_CYW4343W=1`): `EthernetAndWiFi`, `WiFiConnect`
  and `LinkWatcher` build.
- Ethernet only (`ARDUINO_TEENSY41_CYW4343W=0`): `Ping` builds.

### Testing (pending, needs hardware)

- [ ] CYW4343W: run `EthernetAndWiFi` and check that both interfaces get an
      address
- [ ] CYW43439: same test
- [ ] Confirm traffic leaves through the right interface. With two interfaces,
      lwIP picks one by subnet and falls back to the default.
