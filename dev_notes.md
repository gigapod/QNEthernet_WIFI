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
