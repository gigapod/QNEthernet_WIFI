// SPDX-FileCopyrightText: (c) 2021-2024,2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// driver_teensy41.h defines Teensy 4.1-specific things.
// This file is part of the QNEthernet library.

#pragma once

#define MTU           1500
#define MAX_FRAME_LEN 1518  /* Does not include the 4-byte FCS (frame check sequence) */

// lwIP options

// ARP options
#define ETH_PAD_SIZE 2  /* 0 */

// Checksum offloading done by the hardware. Raw frame support needs the
// stack to generate checksums because the hardware would modify raw frames.
#if !QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
#define QNETHERNET_INTERNAL_TEENSY41_HW_CHECKSUM_GEN 1
#else
#define QNETHERNET_INTERNAL_TEENSY41_HW_CHECKSUM_GEN 0
#endif  // !QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
#define QNETHERNET_INTERNAL_TEENSY41_HW_CHECKSUM_CHECK 1

// Checksum options
// When other drivers are compiled in, offloading is chosen per netif instead
// (see LWIP_CHECKSUM_CTRL_PER_NETIF in driver_select.h).
#if !defined(QNETHERNET_INTERNAL_MULTIPLE_DRIVERS)

#if QNETHERNET_INTERNAL_TEENSY41_HW_CHECKSUM_GEN
#define CHECKSUM_GEN_IP      0  /* 1 */
#define CHECKSUM_GEN_UDP     0  /* 1 */
#define CHECKSUM_GEN_TCP     0  /* 1 */
#define CHECKSUM_GEN_ICMP    0  /* 1 */
// #define CHECKSUM_GEN_ICMP6   1
#endif  // QNETHERNET_INTERNAL_TEENSY41_HW_CHECKSUM_GEN

#define CHECKSUM_CHECK_IP    0  /* 1 */
#define CHECKSUM_CHECK_UDP   0  /* 1 */
#define CHECKSUM_CHECK_TCP   0  /* 1 */
#define CHECKSUM_CHECK_ICMP  0  /* 1 */
// #define CHECKSUM_CHECK_ICMP6 1

#endif  // !QNETHERNET_INTERNAL_MULTIPLE_DRIVERS
