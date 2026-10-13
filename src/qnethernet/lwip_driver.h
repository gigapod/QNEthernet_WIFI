// SPDX-FileCopyrightText: (c) 2021-2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file lwip_driver.h
 * @brief Defines the driver interface and the network interface functions
 *        that connect drivers to lwIP.
 *
 * Based on code from manitou48 and others:
 * https://github.com/PaulStoffregen/teensy41_ethernet
 *
 * @par Requirements for driver-specific headers
 * 1. Define MTU.
 * 2. Define MAX_FRAME_LEN (not including the 4-byte FCS (frame check
 *    sequence)). It will usually be MTU + 14 (2 6-byte MACs, 2-byte tag) +
 *    4 (VLAN).
 * 3. If necessary, define lwIP options (see lwip/opt.h) with appropriate values
 *    for your driver. For example, Ethernet padding, checksum generation, and
 *    checksum checking.
 *
 * When more than one driver is compiled in, their headers must agree on MTU,
 * MAX_FRAME_LEN, and ETH_PAD_SIZE, and checksum offloading is chosen per
 * netif (see DriverOps::hw_checksum_gen).
 *
 * @par How to create a driver
 * 1. Create a header that defines MTU and MAX_FRAME_LEN. Don't forget to use
 *    either `#pragma once` or a `#define` guard.
 * 2. Create driver source and include lwip_driver.h. Implement the driver
 *    functions inside a driver-specific namespace (for example, `driver_y`)
 *    and define a `const DriverOps kDriverOps` in that namespace that points
 *    to them. See DriverOps for which functions are optional.
 * 3. Declare the ops table below, next to the others, and point
 *    QNETHERNET_INTERNAL_PRIMARY_DRIVER_OPS at it in driver_select.h.
 * 4. Adjust the driver selection logic in driver_select.h to define an
 *    appropriate macro (such as INTERNAL_DRIVER_Y) when the desired driver
 *    condition is satisfied.
 * 5. Include your driver header in the correct place in driver_select.h.
 * 6. In your driver source, gate the whole file(s) on the macro you chose
 *    above. Of course, test the macro after the lwip_driver.h include.
 *    (Example: INTERNAL_DRIVER_Y)
 * 7. Add lwIP options (see lwip/opt.h) with appropriate values for your driver.
 *    For example, Ethernet padding, checksum generation, and checksum checking.
 * 8. Optionally set DriverOps::hardware_status to an appropriate
 *    EthernetHardwareStatus value. If set to -1 (`EthernetOtherHardware`),
 *    that will be returned if hardware is found (has_hardware() returns true).
 *
 * @par How to create an external driver that isn't inside the distribution
 * 1. Create a header named "qnethernet_external_driver.h" that defines MTU and
 *    MAX_FRAME_LEN. MAX_FRAME_LEN should not include the 4-byte FCS. Don't
 *    forget to use either `#pragma once` or a `#define` guard.
 * 2. Add lwIP options (see lwip/opt.h) with appropriate values for your driver.
 *    For example, Ethernet padding, checksum generation, and checksum checking.
 * 3. Create driver source and include lwip_driver.h. Implement all the
 *    `driver::x()` functions. The library wraps these in an ops table
 *    (see driver_external.cpp).
 *
 * This file is part of the QNEthernet library.
 */

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
  MIN_FRAME_LEN = 60,  ///< The minimum Ethernet frame length, without the FCS.
};

// STATIC_ASSERT(MIN_FRAME_LEN >= 0, "MIN_FRAME_LEN must be >= 0");

// Check some sizes
static_assert(ETH_PAD_SIZE <= UINT16_MAX, "ETH_PAD_SIZE must be <= UINT16_MAX");

namespace qindesign {
namespace network {

// --------------------------------------------------------------------------
//  Types
// --------------------------------------------------------------------------

/** @brief Flags that indicate driver capabilities. */
struct DriverCapabilities {
  bool isMACSettable                = false;  ///< The MAC address can be set.
  bool isLinkStateDetectable        = false;  ///< The link state is detected.
  bool isLinkSpeedDetectable        = false;  ///< The link speed is detected.
  bool isLinkSpeedSettable          = false;  ///< The link speed can be set.
  bool isLinkFullDuplexDetectable   = false;  ///< The duplex mode is detected.
  bool isLinkFullDuplexSettable     = false;  ///< The duplex mode can be set.
  bool isAutoNegotiationSettable    = false;  ///< Auto-negotiation can be set.
  bool isLinkCrossoverDetectable    = false;  ///< Crossover is detected.
  bool isAutoNegotiationRestartable = false;  ///< Auto-negotiation can be restarted.
  bool isPHYResettable              = false;  ///< The PHY can be reset.
};

/**
 * @brief Link information.
 *
 * The defaults are 100 Mbps, full-duplex, auto-negotiation enabled, and
 * no crossover.
 */
struct LinkInfo {
  int speed              = 100;    ///< The speed, in Mbps.
  bool fullNotHalfDuplex = true;   ///< Full-duplex if true, half if false.
  bool isAutoNegotiation = true;   ///< Whether auto-negotiation is enabled.
  bool isCrossover       = false;  ///< Whether the link is crossed over.
};

/**
 * @brief Link settings.
 *
 * The defaults are 100 Mbps, full-duplex, and auto-negotiation enabled.
 *
 * This is separate from LinkInfo because some things are detectable but
 * not settable.
 */
struct LinkSettings {
  int speed              = 100;   ///< The speed, in Mbps.
  bool fullNotHalfDuplex = true;  ///< Full-duplex if true, half if false.
  bool autoNegotiation   = true;  ///< Whether to use auto-negotiation.
};

// --------------------------------------------------------------------------
//  Driver Interface
// --------------------------------------------------------------------------

/**
 * @brief A driver's function table.
 *
 * Each driver defines one of these so that more than one driver can be
 * compiled in and used at the same time. Each function has the same meaning
 * as the function of the same name in the `driver` namespace.
 *
 * It can be assumed that any parameters passed in will not be NULL.
 *
 * Functions marked "optional" may be NULL; callers check before calling.
 */
struct DriverOps {
  /** @brief The interface name prefix, for example "en" or "wl". */
  char ifname[2];

  /** @brief The MTU. */
  size_t mtu;

  /** @brief The maximum frame length, not including the 4-byte FCS. */
  size_t max_frame_len;

  /** @brief The EthernetHardwareStatus value to report when hardware is found. */
  int hardware_status;

  /**
   * @brief Whether the hardware generates IP, UDP, TCP, and ICMP checksums.
   *
   * This is only used when LWIP_CHECKSUM_CTRL_PER_NETIF is enabled; otherwise
   * the CHECKSUM_GEN_x options apply.
   */
  bool hw_checksum_gen;

  /**
   * @brief Whether the hardware checks IP, UDP, TCP, and ICMP checksums.
   *
   * This is only used when LWIP_CHECKSUM_CTRL_PER_NETIF is enabled; otherwise
   * the CHECKSUM_CHECK_x options apply.
   */
  bool hw_checksum_check;

  /** @brief See driver::get_capabilities(). */
  void (*get_capabilities)(DriverCapabilities* dc);
  /** @brief See driver::is_unknown(). */
  bool (*is_unknown)();
  /** @brief See driver::get_system_mac(). */
  void (*get_system_mac)(uint8_t mac[ETH_HWADDR_LEN]);
  /** @brief See driver::get_mac(). */
  bool (*get_mac)(uint8_t mac[ETH_HWADDR_LEN]);
  /** @brief See driver::set_mac(). */
  bool (*set_mac)(const uint8_t mac[ETH_HWADDR_LEN]);
  /** @brief See driver::has_hardware(). */
  bool (*has_hardware)();
  /** @brief See driver::set_chip_select_pin(). Optional. */
  void (*set_chip_select_pin)(int pin);
  /** @brief See driver::init(). */
  bool (*init)();
  /** @brief See driver::deinit(). */
  void (*deinit)();
  /** @brief See driver::proc_input(). */
  struct pbuf* (*proc_input)(struct netif* netif, int counter);
  /** @brief See driver::poll(). */
  void (*poll)(struct netif* netif);
  /** @brief See driver::get_link_info(). */
  void (*get_link_info)(LinkInfo* li);
  /** @brief See driver::set_link(). Optional. */
  bool (*set_link)(const LinkSettings* ls);
  /** @brief See driver::output(). */
  err_t (*output)(struct pbuf* p);
#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
  /** @brief See driver::output_frame(). Optional. */
  bool (*output_frame)(const void* frame, size_t len);
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE
  /** @brief See driver::set_incoming_mac_address_allowed(). Optional. */
  bool (*set_incoming_mac_address_allowed)(const uint8_t mac[ETH_HWADDR_LEN],
                                           bool allow);
#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE
  /** @brief See driver::notify_manual_link_state(). Optional. */
  void (*notify_manual_link_state)(bool flag);
  /** @brief See driver::restart_auto_negotiation(). Optional. */
  void (*restart_auto_negotiation)();
  /** @brief See driver::reset_phy(). Optional. */
  void (*reset_phy)();
};

// Ops tables for the compiled-in drivers. Each is named `kDriverOps` in its
// driver's namespace.
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

/**
 * @brief The function interface an external driver implements.
 *
 * Internal drivers implement the same functions in their own namespaces and
 * expose them via a DriverOps table.
 *
 * It can be assumed that any parameters passed in will not be NULL.
 */
namespace driver {

/**
 * @brief Gets the driver capabilities.
 *
 * This may be called more than once but may not be valid until after init()
 * is called.
 *
 * @param dc Filled in with the capabilities.
 */
void get_capabilities(DriverCapabilities* dc);

/**
 * @brief Returns whether the hardware hasn't yet been probed.
 *
 * @return Whether the hardware state is unknown.
 */
ATTRIBUTE_NODISCARD
bool is_unknown();

/**
 * @brief Gets the built-in Ethernet MAC address.
 *
 * For systems without a built-in address, this should retrieve some
 * valid default.
 *
 * @param mac Filled in with the address.
 */
void get_system_mac(uint8_t mac[ETH_HWADDR_LEN]);

/**
 * @brief Gets the internal MAC address.
 *
 * This will retrieve the system default if the internal address has not yet
 * been set.
 *
 * @param mac Filled in with the address.
 * @return Whether the address was retrieved.
 */
ATTRIBUTE_NODISCARD
bool get_mac(uint8_t mac[ETH_HWADDR_LEN]);

/**
 * @brief Sets the internal MAC address.
 *
 * This will be set as a transmitted Ethernet frame's source address.
 *
 * @param mac The address.
 * @return Whether successful.
 * @see get_capabilities()
 */
ATTRIBUTE_NODISCARD
bool set_mac(const uint8_t mac[ETH_HWADDR_LEN]);

/**
 * @brief Determines if there's hardware.
 *
 * If the hardware hasn't yet been probed (is_unknown() would return true),
 * then this will check the hardware.
 *
 * @return Whether there's hardware.
 */
ATTRIBUTE_NODISCARD
bool has_hardware();

/**
 * @brief Sets the SPI chip select pin given in Ethernet.init().
 *
 * @param pin The pin, or -1 if it has not been set.
 */
void set_chip_select_pin(int pin);

/**
 * @brief Does low-level initialization.
 *
 * Most functions depend on the driver being initialized.
 *
 * @return Whether the initialization was successful.
 */
ATTRIBUTE_NODISCARD
bool init();

/** @brief Uninitializes the driver. */
void deinit();

/**
 * @brief Processes any input and returns a received frame.
 *
 * This is called repeatedly until it returns NULL, so it is expected to return
 * NULL at some point to give the rest of the program a chance to run.
 *
 * @param netif   The interface's netif.
 * @param counter How many times this has been called in the current loop.
 *                Drivers can use this to know when to stop.
 * @return A received frame, including any ETH_PAD_SIZE bytes at the start, or
 *         NULL if there are no more for now.
 */
ATTRIBUTE_NODISCARD
struct pbuf* proc_input(struct netif* netif, int counter);

/**
 * @brief Polls anything that needs to be polled, for example, the link status.
 *
 * @param netif The interface's netif, for setting the link state.
 */
void poll(struct netif* netif);

/**
 * @brief Gets info about the link.
 *
 * The values will only be valid when the link is up.
 *
 * @param li Filled in with the link info.
 * @see get_capabilities()
 */
void get_link_info(LinkInfo* li);

/**
 * @brief Sets some link parameters all at once.
 *
 * @param ls The settings.
 * @return Whether successful. This is expected to be false if the hardware is
 *         not initialized or if any of the parameters are invalid.
 */
ATTRIBUTE_NODISCARD
bool set_link(const LinkSettings* ls);

/**
 * @brief Outputs the given pbuf data.
 *
 * @param p The frame. It already contains any extra ETH_PAD_SIZE bytes at
 *          the start.
 * @return ERR_OK if successful, or another error code.
 */
ATTRIBUTE_NODISCARD
err_t output(struct pbuf* p);

#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
/**
 * @brief Outputs a raw Ethernet frame.
 *
 * The driver should add, to the start, any extra padding bytes given by
 * ETH_PAD_SIZE.
 *
 * @param frame The frame, without padding or the FCS.
 * @param len   The frame length.
 * @return Whether successful.
 */
ATTRIBUTE_NODISCARD
bool output_frame(const void* frame, size_t len);
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT

// --------------------------------------------------------------------------
//  MAC Address Filtering
// --------------------------------------------------------------------------

#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE

/**
 * @brief Allows or disallows frames addressed to the specified MAC address.
 *
 * In other words, this controls the allowable "destination address" of
 * received Ethernet frames.
 *
 * This is not meant to be used for joining or leaving a multicast group at the
 * IP layer; use the IP stack for that.
 *
 * Because the underlying system might use a hash of the MAC address, it's
 * possible for there to be collisions. This means that it's not always
 * possible to disallow an address once it's been allowed.
 *
 * Note that this function may be passed a NULL MAC address.
 *
 * @param mac   The address.
 * @param allow Whether to allow or disallow it.
 * @return Whether adding or removing the address was successful. This is false
 *         if the address has a collision and can't be removed, or if `mac`
 *         is NULL.
 */
ATTRIBUTE_NODISCARD
bool set_incoming_mac_address_allowed(const uint8_t mac[ETH_HWADDR_LEN],
                                      bool allow);

#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE

// --------------------------------------------------------------------------
//  Notifications from Upper Layers
// --------------------------------------------------------------------------

/**
 * @brief Notifies the driver that the system manually set the link state.
 *
 * It is assumed that if the new link state is true then it should be sticky.
 *
 * @param flag The new link state.
 */
void notify_manual_link_state(bool flag);

// --------------------------------------------------------------------------
//  Link Functions
// --------------------------------------------------------------------------

/**
 * @brief Restarts auto-negotiation, if the driver supports it.
 *
 * @see get_capabilities()
 */
void restart_auto_negotiation();

/**
 * @brief Resets the PHY, if the driver supports it.
 *
 * @see get_capabilities()
 */
void reset_phy();

}  // namespace driver

// --------------------------------------------------------------------------
//  Public Interface
// --------------------------------------------------------------------------

/**
 * @brief The network interface layer between EthernetClass, the drivers,
 *        and lwIP.
 *
 * Each Interface pairs a driver with its lwIP netif. Unless noted, functions
 * taking an Interface expect a non-NULL pointer from primary_interface() or
 * wifi_interface().
 */
namespace enet {

/**
 * @brief A network interface: a driver plus its lwIP netif and
 *        related state.
 *
 * This is opaque; use the functions in this namespace.
 */
struct Interface;

/**
 * @brief Returns the primary interface, the one bound to the `Ethernet`
 *        instance.
 *
 * @return The interface.
 */
ATTRIBUTE_NODISCARD
Interface* primary_interface();

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
/**
 * @brief Returns the WiFi interface, the one bound to the `WiFi` instance.
 *
 * @return The interface.
 */
ATTRIBUTE_NODISCARD
Interface* wifi_interface();
#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS

/**
 * @brief Receives netif events for one interface.
 *
 * @param owner  The value given to init().
 * @param reason The lwIP reason flags.
 * @param args   The lwIP event arguments. This may be NULL.
 */
using EventFn = void (*)(void* owner, netif_nsc_reason_t reason,
                         const netif_ext_callback_args_t* args);

/**
 * @brief Returns the interface's driver ops.
 *
 * @param iface The interface.
 * @return The driver's function table.
 */
ATTRIBUTE_NODISCARD
const DriverOps& driver_ops(const Interface* iface);

/**
 * @brief Returns the interface's MTU.
 *
 * @param iface The interface.
 * @return The MTU.
 */
ATTRIBUTE_NODISCARD
inline size_t get_mtu(const Interface* const iface) {
  return driver_ops(iface).mtu;
}

/**
 * @brief Returns the interface's maximum frame length.
 *
 * @param iface The interface.
 * @return The maximum frame length, not including the 4-byte FCS (frame check
 *         sequence).
 */
ATTRIBUTE_NODISCARD
inline size_t get_max_frame_len(const Interface* const iface) {
  return driver_ops(iface).max_frame_len;
}

/**
 * @brief Gets the built-in MAC address.
 *
 * For systems without a built-in address, this should retrieve some default.
 *
 * @param iface The interface.
 * @param mac   Filled in with the address. This does nothing if it's NULL.
 */
void get_system_mac(Interface* iface, uint8_t mac[ETH_HWADDR_LEN]);

/**
 * @brief Gets the current MAC address.
 *
 * @param iface The interface.
 * @param mac   Filled in with the address.
 * @return Whether the address was retrieved. This is false if `mac` is NULL.
 */
ATTRIBUTE_NODISCARD
bool get_mac(Interface* iface, uint8_t mac[ETH_HWADDR_LEN]);

/**
 * @brief Sets the current MAC address.
 *
 * @param iface The interface.
 * @param mac   The address.
 * @return Whether the address was changed. This is false if `mac` is NULL.
 */
ATTRIBUTE_NODISCARD
bool set_mac(Interface* iface, const uint8_t mac[ETH_HWADDR_LEN]);

/**
 * @brief Initializes the interface.
 *
 * This initializes the driver and adds the netif to lwIP, initializing lwIP
 * the first time. It does not set the interface to "up".
 *
 * This may be called more than once, but if the MAC address has changed then
 * the netif is first removed and then re-added.
 *
 * The netif becomes the default netif if there isn't one already or if this
 * is the primary interface.
 *
 * It is suggested to initialize the entropy generator with
 * qnethernet_hal_init_entropy() before calling this.
 *
 * @param iface The interface.
 * @param mac   The MAC address to use. If the MAC is not settable or this is
 *              NULL then the system MAC address is used.
 * @param fn    Receives events for this interface's netif.
 * @param owner Passed to `fn`.
 * @param dc    Filled in with the driver capabilities, if not NULL.
 * @return Whether successful. If not, errno is set to something appropriate.
 * @see DriverOps::get_capabilities
 */
ATTRIBUTE_NODISCARD
bool init(Interface* iface,
          const uint8_t mac[ETH_HWADDR_LEN],
          EventFn fn, void* owner,
          DriverCapabilities* dc);

/**
 * @brief Removes the interface's netif and shuts down its driver.
 *
 * If its netif was the default then another added interface becomes
 * the default.
 *
 * @param iface The interface.
 */
void deinit(Interface* iface);

/**
 * @brief Makes the interface's netif the default netif (the default route).
 *
 * @param iface The interface.
 * @return Whether successful. This is false if the interface hasn't
 *         been added.
 */
ATTRIBUTE_NODISCARD
bool set_default(Interface* iface);

/**
 * @brief Gets a pointer to the interface's netif.
 *
 * This is useful for the netif callback before the default netif has
 * been assigned.
 *
 * @param iface The interface.
 * @return The netif.
 */
ATTRIBUTE_NODISCARD
struct netif* netif(Interface* iface);

/**
 * @brief Moves the whole stack along.
 *
 * This processes input on all added interfaces and, periodically, checks the
 * stack's timeouts and polls their drivers (for example, the link status). It
 * does nothing if no interface has been added. This is meant to be called
 * often by the main loop.
 */
void loop();

#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
/**
 * @brief Outputs a raw Ethernet frame.
 *
 * The proper length range is [14, MAX_FRAME_LEN-4] for non-VLAN frames and
 * [18, MAX_FRAME_LEN] for VLAN frames. Note that these ranges exclude the
 * 4-byte FCS (frame check sequence).
 *
 * @param iface The interface.
 * @param frame The frame.
 * @param len   The frame length.
 * @return The result of the driver's output_frame(), if the frame checks pass.
 *         This is false if `frame` is NULL, the length is out of range, or the
 *         driver doesn't support raw frames.
 */
ATTRIBUTE_NODISCARD
bool output_frame(Interface* iface, const void* frame, size_t len);
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT

// --------------------------------------------------------------------------
//  MAC Address Filtering
// --------------------------------------------------------------------------

#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE

/**
 * @brief Allows or disallows frames addressed to the specified MAC address.
 *
 * @param iface The interface.
 * @param mac   The address.
 * @param allow Whether to allow or disallow it.
 * @return The result of the driver's set_incoming_mac_address_allowed(), or
 *         false if the driver doesn't support it.
 * @see driver::set_incoming_mac_address_allowed()
 */
ATTRIBUTE_NODISCARD
bool set_incoming_mac_address_allowed(Interface* iface,
                                      const uint8_t mac[ETH_HWADDR_LEN],
                                      bool allow);

#if LWIP_IPV4

/**
 * @brief Allows frames for an IPv4 multicast group's MAC address.
 *
 * Note that this affects low-level MAC filtering and not the IP stack's use of
 * multicast groups.
 *
 * @param iface The interface.
 * @param group The group.
 * @return The result of set_incoming_mac_address_allowed(), or false if
 *         `group` is NULL.
 */
ATTRIBUTE_NODISCARD
bool join_group(Interface* iface, const ip4_addr_t* group);

/**
 * @brief Disallows frames for an IPv4 multicast group's MAC address.
 *
 * @param iface The interface.
 * @param group The group.
 * @return The result of set_incoming_mac_address_allowed(), or false if
 *         `group` is NULL.
 * @see join_group()
 */
ATTRIBUTE_NODISCARD
bool leave_group(Interface* iface, const ip4_addr_t* group);

#endif  // LWIP_IPV4
#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE

}  // namespace enet

}  // namespace network
}  // namespace qindesign
