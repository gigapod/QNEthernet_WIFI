// driver_CYW4343W.h

#pragma once

// The netif MTU. 802.11 networks bridge to Ethernet, so use the Ethernet MTU.
// These match driver_teensy41.h so that both drivers can be compiled in.
#define MTU           1500
#define MAX_FRAME_LEN 1518  /* Does not include the 4-byte FCS (frame check sequence) */

// Size of the buffer for frames read from the chip. This is larger than
// MAX_FRAME_LEN because non-data (event) frames may also arrive here.
#define QNETHERNET_INTERNAL_CYW4343W_RX_BUF_SIZE 4018

// The driver handles any ETH_PAD_SIZE, so it doesn't define one.

#define EVENT_POLL_USEC    100000 //100000

#define LINK_UP 1
#define LINK_DOWN 0
