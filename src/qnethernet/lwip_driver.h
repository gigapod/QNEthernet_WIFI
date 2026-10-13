// SPDX-FileCopyrightText: (c) 2021-2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// lwip_driver.h defines Ethernet interface functions.
// Based on code from manitou48 and others:
// https://github.com/PaulStoffregen/teensy41_ethernet
// This file is part of the QNEthernet library.

#pragma once

// C++ includes
#include <cstddef>
#include <cstdint>

#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include "qnethernet/compat/c++11_compat.h"
#include "qnethernet/driver_select.h"
#include "qnethernet_opts.h"

// Check things that are supposed to be set properly by the driver headers
static_assert(MTU > 0, "MTU must be defined and > 0");
static_assert(MAX_FRAME_LEN >= 0, "MAX_FRAME_LEN must be defined and >= 0");

enum {
  MIN_FRAME_LEN = 60,
};

// STATIC_ASSERT(MIN_FRAME_LEN >= 0, "MIN_FRAME_LEN must be >= 0");

// Check some sizes
static_assert(ETH_PAD_SIZE <= UINT16_MAX, "ETH_PAD_SIZE must be <= UINT16_MAX");

// Requirements for driver-specific headers:
// 1. Define MTU
// 2. Define MAX_FRAME_LEN (not including the 4-byte FCS (frame check sequence))
//    1. It will usually be MTU + 14 (2 6-byte MACs, 2-byte tag) + 4 (VLAN)
// 3. If necessary, define lwIP options (see lwip/opt.h) with appropriate values
//    for your driver. For example, Ethernet padding, checksum generation, and
//    checksum checking.

// How to create a driver:
// 1. Create a header that defines MTU and MAX_FRAME_LEN. Don't forget to use
//    either `#pragma once` or a #define guard.
// 2. Create driver source and include lwip_driver.h. Implement the driver
//    functions inside a driver-specific namespace (for example,
//    `driver_y`) and define a `const DriverOps kDriverOps` in that namespace
//    that points to them. See DriverOps for which functions are optional.
// 3. Declare the ops table below, next to the others, and point
//    QNETHERNET_INTERNAL_PRIMARY_DRIVER_OPS at it in driver_select.h.
// 4. Adjust the driver selection logic in driver_select.h to define an
//    appropriate macro (such as INTERNAL_DRIVER_Y) when the desired driver
//    condition is satisfied.
// 5. Include your driver header in the correct place in driver_select.h.
// 6. In your driver source, gate the whole file(s) on the macro you chose
//    above. Of course, test the macro after the lwip_driver.h include.
//    (Example: INTERNAL_DRIVER_Y)
// 7. Add lwIP options (see lwip/opt.h) with appropriate values for your driver.
//    For example, Ethernet padding, checksum generation, and checksum checking.
// 8. Optionally set DriverOps::hardware_status to an appropriate
//    EthernetHardwareStatus value. If set to -1 ('EthernetOtherHardware'), that
//    will be returned if hardware is found (has_hardware() returns true).

// How to create an external driver that isn't inside the distribution:
// 1. Create a header named "qnethernet_external_driver.h" that defines MTU and
//    MAX_FRAME_LEN. MAX_FRAME_LEN should not include the 4-byte FCS. Don't
//    forget to use either `#pragma once` or a #define guard.
// 2. Add lwIP options (see lwip/opt.h) with appropriate values for your driver.
//    For example, Ethernet padding, checksum generation, and checksum checking.
// 3. Create driver source and include lwip_driver.h. Implement all the
//    `driver::x()` functions. The library wraps these in an ops table
//    (see driver_external.cpp).

namespace qindesign {
namespace network {

// --------------------------------------------------------------------------
//  Types
// --------------------------------------------------------------------------

// Flags that indicate driver capabilities.
struct DriverCapabilities {
  bool isMACSettable                = false;
  bool isLinkStateDetectable        = false;
  bool isLinkSpeedDetectable        = false;
  bool isLinkSpeedSettable          = false;
  bool isLinkFullDuplexDetectable   = false;
  bool isLinkFullDuplexSettable     = false;
  bool isAutoNegotiationSettable    = false;
  bool isLinkCrossoverDetectable    = false;
  bool isAutoNegotiationRestartable = false;
  bool isPHYResettable              = false;
};

// Link information. Defaults are:
// * 100 Mbps
// * full-duplex
// * Auto-negotiation enabled
// * Crossover is false
struct LinkInfo {
  int speed              = 100;
  bool fullNotHalfDuplex = true;
  bool isAutoNegotiation = true;
  bool isCrossover       = false;
};

// Link settings. Defaults are:
// * 100 Mbps
// * full-duplex
// * Auto-negotiation enabled
//
// This is separate from LinkInfo because some things are detecatable but
// not settable.
struct LinkSettings {
  int speed              = 100;
  bool fullNotHalfDuplex = true;
  bool autoNegotiation   = true;
};

// --------------------------------------------------------------------------
//  Driver Interface
// --------------------------------------------------------------------------

// A driver's function table. Each driver defines one of these so that more
// than one driver can be compiled in and used at the same time. See the
// `driver` namespace below for a description of each function.
//
// It can be assumed that any parameters passed in will not be NULL.
//
// Functions marked "optional" may be NULL; callers check before calling.
struct DriverOps {
  // Interface name prefix, for example "en" or "wl".
  char ifname[2];

  // The MTU and the maximum frame length (not including the 4-byte FCS).
  size_t mtu;
  size_t max_frame_len;

  // An EthernetHardwareStatus value to report when hardware is found.
  int hardware_status;

  // Whether the hardware generates and checks IP, UDP, TCP, and ICMP
  // checksums. These are only used when LWIP_CHECKSUM_CTRL_PER_NETIF is
  // enabled; otherwise the CHECKSUM_GEN_x and CHECKSUM_CHECK_x options apply.
  bool hw_checksum_gen;
  bool hw_checksum_check;

  void (*get_capabilities)(DriverCapabilities* dc);
  bool (*is_unknown)();
  void (*get_system_mac)(uint8_t mac[ETH_HWADDR_LEN]);
  bool (*get_mac)(uint8_t mac[ETH_HWADDR_LEN]);
  bool (*set_mac)(const uint8_t mac[ETH_HWADDR_LEN]);
  bool (*has_hardware)();
  void (*set_chip_select_pin)(int pin);  // Optional
  bool (*init)();
  void (*deinit)();
  struct pbuf* (*proc_input)(struct netif* netif, int counter);
  void (*poll)(struct netif* netif);
  void (*get_link_info)(LinkInfo* li);
  bool (*set_link)(const LinkSettings* ls);  // Optional
  err_t (*output)(struct pbuf* p);
#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
  bool (*output_frame)(const void* frame, size_t len);  // Optional
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE
  bool (*set_incoming_mac_address_allowed)(const uint8_t mac[ETH_HWADDR_LEN],
                                           bool allow);  // Optional
#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE
  void (*notify_manual_link_state)(bool flag);  // Optional
  void (*restart_auto_negotiation)();           // Optional
  void (*reset_phy)();                          // Optional
};

// Ops tables for the compiled-in drivers
#if defined(QNETHERNET_INTERNAL_DRIVER_EXTERNAL)
namespace driver_external { extern const DriverOps kDriverOps; }
#endif
#if defined(QNETHERNET_INTERNAL_DRIVER_W5500)
namespace driver_w5500 { extern const DriverOps kDriverOps; }
#endif
#if defined(QNETHERNET_INTERNAL_DRIVER_CYW4343W)
namespace driver_cyw4343w { extern const DriverOps kDriverOps; }
#endif
#if defined(QNETHERNET_INTERNAL_DRIVER_TEENSY41)
namespace driver_teensy41 { extern const DriverOps kDriverOps; }
#endif
#if defined(QNETHERNET_INTERNAL_DRIVER_UNSUPPORTED)
namespace driver_unsupported { extern const DriverOps kDriverOps; }
#endif

// The function interface an external driver implements. Internal drivers
// implement the same functions in their own namespaces and expose them via
// a DriverOps table.
namespace driver {

// It can be assumed that any parameters passed in will not be NULL.

// Gets the driver capabilities and fills in the given struct. This may be
// called more than once but may not be valid until after init()
// is called.
void get_capabilities(DriverCapabilities* dc);

// Returns if the hardware hasn't yet been probed.
ATTRIBUTE_NODISCARD
bool is_unknown();

// Gets the built-in Ethernet MAC address.
//
// For systems without a built-in address, this should retrieve some
// valid default.
void get_system_mac(uint8_t mac[ETH_HWADDR_LEN]);

// Gets the internal MAC address. This will retrieve the system default if the
// internal address has not yet been set.
//
// This returns whether the address was retrieved.
ATTRIBUTE_NODISCARD
bool get_mac(uint8_t mac[ETH_HWADDR_LEN]);

// Sets the internal MAC address and returns whether successful. This will be
// set as a transmitted Ethernet frame's source address.
//
// See also: get_capabilities(dc)
ATTRIBUTE_NODISCARD
bool set_mac(const uint8_t mac[ETH_HWADDR_LEN]);

// Determines if there's Ethernet hardware. If the hardware hasn't yet been
// probed (is_unknown() would return 'true'), then this will check
// the hardware.
ATTRIBUTE_NODISCARD
bool has_hardware();

// Sets the SPI chip select pin given in Ethernet.init(). The pin will be -1 if
// it has not been initialized.
void set_chip_select_pin(int pin);

// Does low-level initialization. This returns whether the initialization
// was successful. Most functions depend on the driver being initialized.
ATTRIBUTE_NODISCARD
bool init();

// Uninitializes the driver.
void deinit();

// Processes any input and returns any received frames as a pbuf. The counter
// parameter indicates how many times the call has looped. This is useful so
// that drivers can know when to stop checking to give the rest of the program a
// chance to run. In other words, it is expected that this will return NULL at
// some point.
ATTRIBUTE_NODISCARD
struct pbuf* proc_input(struct netif* netif, int counter);

// Polls anything that needs to be polled, for example, the link status.
void poll(struct netif* netif);

// Gets info about the link. The values will only be valid when the link is up.
//
// See also: get_capabilities(dc)
void get_link_info(LinkInfo* li);

// Sets some link parameters all at once. This returns whether the call
// was successful. It is expected that this will return false if the hardware is
// not initialized.
//
// This will also return false if any of the parameters are invalid.
ATTRIBUTE_NODISCARD
bool set_link(const LinkSettings* ls);

// Outputs the given pbuf data.
//
// Note that the data will already contain any extra ETH_PAD_SIZE bytes.
ATTRIBUTE_NODISCARD
err_t output(struct pbuf* p);

#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
// Outputs a raw Ethernet frame and returns whether successful.
//
// This should add, to the start, any extra padding bytes given by ETH_PAD_SIZE.
ATTRIBUTE_NODISCARD
bool output_frame(const void* frame, size_t len);
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT

// --------------------------------------------------------------------------
//  MAC Address Filtering
// --------------------------------------------------------------------------

#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE

// Allows or disallows frames addressed to the specified MAC address. In other
// words, this controls the allowable "destination address" of received
// Ethernet frames.
//
// This is not not meant to be used for joining or leaving a multicast group at
// the IP layer; use the IP stack for that.
//
// Because the underlying system might use a hash of the MAC address, it's
// possible for there to be collisions. This means that it's not always possible
// to disallow an address once it's been allowed.
//
// This returns true if adding or removing the MAC was successful. If an address
// has a collision, then it can't be removed and this will return false. This
// will also return false if 'mac' is NULL. Otherwise, this will return true.
//
// Note that this function may be passed a NULL MAC address.
ATTRIBUTE_NODISCARD
bool set_incoming_mac_address_allowed(const uint8_t mac[ETH_HWADDR_LEN],
                                      bool allow);

#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE

// --------------------------------------------------------------------------
//  Notifications from Upper Layers
// --------------------------------------------------------------------------

// Notifies the driver that the system manually set the link state. It is
// assumed that if the new link state is TRUE then it should be sticky.
void notify_manual_link_state(bool flag);

// --------------------------------------------------------------------------
//  Link Functions
// --------------------------------------------------------------------------

// Restarts auto-negotiation, if the driver supports it.
//
// See also: get_capabilities(dc)
void restart_auto_negotiation();

// Resets the PHY, if the driver supports it.
//
// See also: get_capabilities(dc)
void reset_phy();

}  // namespace driver

// --------------------------------------------------------------------------
//  Public Interface
// --------------------------------------------------------------------------

namespace enet {

// A network interface: a driver plus its lwIP netif and related state.
struct Interface;

// Returns the primary interface, the one bound to the `Ethernet` instance.
ATTRIBUTE_NODISCARD
Interface* primary_interface();

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
// Returns the WiFi interface, the one bound to the `WiFi` instance.
ATTRIBUTE_NODISCARD
Interface* wifi_interface();
#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS

// Receives netif events for one interface. 'owner' is the value given
// to init().
using EventFn = void (*)(void* owner, netif_nsc_reason_t reason,
                         const netif_ext_callback_args_t* args);

// Returns the interface's driver ops.
ATTRIBUTE_NODISCARD
const DriverOps& driver_ops(const Interface* iface);

// Returns the MTU.
ATTRIBUTE_NODISCARD
inline size_t get_mtu(const Interface* const iface) {
  return driver_ops(iface).mtu;
}

// Returns the maximum frame length. This does not include the 4-byte FCS (frame
// check sequence).
ATTRIBUTE_NODISCARD
inline size_t get_max_frame_len(const Interface* const iface) {
  return driver_ops(iface).max_frame_len;
}

// Gets the built-in Ethernet MAC address. This does nothing if 'mac' is NULL.
//
// For systems without a built-in address, this should retrieve some default.
void get_system_mac(Interface* iface, uint8_t mac[ETH_HWADDR_LEN]);

// Gets the current MAC address and returns whether it was retrieved. This does
// nothing if 'mac' is NULL.
ATTRIBUTE_NODISCARD
bool get_mac(Interface* iface, uint8_t mac[ETH_HWADDR_LEN]);

// Sets the current MAC address and returns whether it was changed. This does
// nothing if 'mac' is NULL.
ATTRIBUTE_NODISCARD
bool set_mac(Interface* iface, const uint8_t mac[ETH_HWADDR_LEN]);

// Initializes the interface and returns whether successful. This does not set
// the interface to "up". If the MAC is not settable or 'mac' is NULL then this
// will use the system MAC address and 'mac' will be ignored. This also fills in
// the driver capabilities struct.
//
// If this returns false then errno will be set to something appropriate.
//
// This may be called more than once, but if the MAC address has changed then
// the interface is first removed and then re-added.
//
// The netif becomes the default netif if there isn't one already or if this
// is the primary interface.
//
// Events for this interface's netif are sent to 'fn' with 'owner'.
//
// It is suggested to initialize the entropy generator with
// qnethernet_hal_init_entropy() before calling this.
//
// See also: DriverOps::get_capabilities
ATTRIBUTE_NODISCARD
bool init(Interface* iface,
          const uint8_t mac[ETH_HWADDR_LEN],
          EventFn fn, void* owner,
          DriverCapabilities* dc);

// Removes the interface and shuts down its driver. If its netif was the
// default then another added interface becomes the default.
void deinit(Interface* iface);

// Makes the interface's netif the default netif (the default route). This
// returns false if the interface hasn't been added.
ATTRIBUTE_NODISCARD
bool set_default(Interface* iface);

// Gets a pointer to the netif structure. This is useful for the netif callback
// before the default netif has been assigned.
ATTRIBUTE_NODISCARD
struct netif* netif(Interface* iface);

// Moves the whole stack along: processes input on all added interfaces and,
// periodically, checks the stack's timeouts and polls their drivers (for
// example, the link status). This is meant to be called often by the
// main loop.
void loop();

#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
// Outputs a raw ethernet frame. This returns false if frame is NULL or if the
// length is not in the correct range. The proper range is [14, MAX_FRAME_LEN-4]
// for non-VLAN frames and [18, MAX_FRAME_LEN] for VLAN frames. Note that these
// ranges exclude the 4-byte FCS (frame check sequence).
//
// This returns the result of the driver's output_frame(), if the frame checks
// pass, or false if the driver doesn't support it.
ATTRIBUTE_NODISCARD
bool output_frame(Interface* iface, const void* frame, size_t len);
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT

// --------------------------------------------------------------------------
//  MAC Address Filtering
// --------------------------------------------------------------------------

#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE

// Calls the driver's set_incoming_mac_address_allowed(), returning false if
// the driver doesn't support it.
ATTRIBUTE_NODISCARD
bool set_incoming_mac_address_allowed(Interface* iface,
                                      const uint8_t mac[ETH_HWADDR_LEN],
                                      bool allow);

#if LWIP_IPV4

// For joining and leaving multicast groups; these call
// set_incoming_mac_address_allowed() with the MAC addresses related to the
// given multicast group. Note that this affects low-level MAC filtering and not
// the IP stack's use of multicast groups.
//
// If 'group' is NULL then these return false. Otherwise, these return the
// result of 'set_incoming_mac_address_allowed()'.
ATTRIBUTE_NODISCARD
bool join_group(Interface* iface, const ip4_addr_t* group);
ATTRIBUTE_NODISCARD
bool leave_group(Interface* iface, const ip4_addr_t* group);

#endif  // LWIP_IPV4
#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE

}  // namespace enet

}  // namespace network
}  // namespace qindesign
