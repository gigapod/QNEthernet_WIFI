// SPDX-FileCopyrightText: (c) 2024-2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// lwip_driver.c implements Ethernet interface functions.
// This file is part of the QNEthernet library.

#include "qnethernet/lwip_driver.h"

// C++ includes
#include <cerrno>
#include <cstring>

#include "lwip/autoip.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/prot/ieee.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "qnethernet/platforms/pgmspace.h"

namespace qindesign {
namespace network {
namespace enet {

// --------------------------------------------------------------------------
//  Internal Variables
// --------------------------------------------------------------------------

#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT && QNETHERNET_ENABLE_RAW_FRAME_LOOPBACK
static const uint8_t kBroadcastMAC[ETH_HWADDR_LEN]{0xff, 0xff, 0xff,
                                                   0xff, 0xff, 0xff};
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT && QNETHERNET_ENABLE_RAW_FRAME_LOOPBACK

// Network interface state.
struct Interface {
  const DriverOps* const ops;
  struct netif netif;
  uint8_t mac[ETH_HWADDR_LEN];  // Current MAC address
  bool isNetifAdded;
  EventFn eventFn;
  void* owner;

  // Structs for avoiding memory allocation
#if LWIP_DHCP
  struct dhcp dhcp;
#endif  // LWIP_DHCP
#if LWIP_AUTOIP
  struct autoip autoip;
#endif  // LWIP_AUTOIP
};

// Creates an interface, getting around some platforms'
// missing-field-initializers warnings.
static Interface create_interface(const DriverOps& ops, const uint8_t num) {
  Interface iface{&ops, {}, {}, false, nullptr, nullptr};
  iface.netif.name[0] = ops.ifname[0];
  iface.netif.name[1] = ops.ifname[1];
  iface.netif.num = num;
  return iface;
}

static Interface s_primary =
    create_interface(QNETHERNET_INTERNAL_PRIMARY_DRIVER_OPS, 0);
#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
static Interface s_wifi = create_interface(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS, 0);
#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS

// All the interfaces
static Interface* const s_interfaces[]{
    &s_primary,
#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
    &s_wifi,
#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS
};

#if LWIP_NETIF_EXT_STATUS_CALLBACK
// One callback for all interfaces; it dispatches to the owning interface.
NETIF_DECLARE_EXT_CALLBACK(s_netifCallback)/*;*/
#endif  // LWIP_NETIF_EXT_STATUS_CALLBACK

// How often to check timeouts and poll the drivers, in milliseconds.
static constexpr uint32_t kPollInterval = 125;  // About 8 times a second
static uint32_t s_lastPollTime = 0;

// Gets the interface from a netif that was added by init().
static inline Interface* iface_from(struct netif* const netif) {
  return static_cast<Interface*>(netif->state);
}

// --------------------------------------------------------------------------
//  Internal Functions
// --------------------------------------------------------------------------

// Outputs the given pbuf to the driver.
ATTRIBUTE_NODISCARD
static err_t link_output(struct netif* const netif, struct pbuf* const p) {
  if (p == nullptr) {
    return ERR_ARG;
  }

  return iface_from(netif)->ops->output(p);
}

#if LWIP_IGMP && !QNETHERNET_ENABLE_PROMISCUOUS_MODE
// Multicast filter for letting the hardware know which packets to let in.
ATTRIBUTE_NODISCARD
static err_t multicast_filter(struct netif* const netif,
                              const ip4_addr_t* const group,
                              const enum netif_mac_filter_action action) {
  Interface* const iface = iface_from(netif);

  bool retval = true;
  switch (action) {
    case NETIF_ADD_MAC_FILTER:
      retval = join_group(iface, group);
      break;
    case NETIF_DEL_MAC_FILTER:
      retval = leave_group(iface, group);
      break;
    default:
      break;
  }
  return retval ? ERR_OK : ERR_USE;
      // ERR_USE seems like the best fit of the choices
      // Next best seems to be ERR_IF
}
#endif  // LWIP_IGMP && !QNETHERNET_ENABLE_PROMISCUOUS_MODE

#if LWIP_NETIF_EXT_STATUS_CALLBACK
// Sends netif events to the interface that owns the netif.
static void netif_event(struct netif* const netif,
                        const netif_nsc_reason_t reason,
                        const netif_ext_callback_args_t* const args) {
  for (Interface* const iface : s_interfaces) {
    if (&iface->netif == netif) {
      if (iface->eventFn != nullptr) {
        iface->eventFn(iface->owner, reason, args);
      }
      return;
    }
  }
}
#endif  // LWIP_NETIF_EXT_STATUS_CALLBACK

// Initializes the netif. This expects netif->state to point to
// the Interface.
ATTRIBUTE_NODISCARD
FLASHMEM static err_t init_netif(struct netif* const netif) {
  if (netif == nullptr) {
    return ERR_ARG;
  }
  Interface* const iface = iface_from(netif);

  netif->linkoutput = link_output;
#if LWIP_IPV4
  netif->output     = etharp_output;
#endif  // LWIP_IPV4
  netif->mtu        = static_cast<uint16_t>(iface->ops->mtu);
  netif->flags = 0
                 | NETIF_FLAG_BROADCAST
#if LWIP_IPV4
                 | NETIF_FLAG_ETHARP
#endif  // LWIP_IPV4
                 | NETIF_FLAG_ETHERNET
#if LWIP_IGMP
                 | NETIF_FLAG_IGMP
#endif  // LWIP_IGMP
                 ;

  (void)std::memcpy(netif->hwaddr, iface->mac, ETH_HWADDR_LEN);
  netif->hwaddr_len = ETH_HWADDR_LEN;

#if LWIP_NETIF_HOSTNAME
  netif_set_hostname(netif, nullptr);
#endif  // LWIP_NETIF_HOSTNAME

#if LWIP_DHCP
  dhcp_set_struct(netif, &iface->dhcp);
#endif  // LWIP_DHCP

#if LWIP_AUTOIP
  autoip_set_struct(netif, &iface->autoip);
#endif  // LWIP_AUTOIP

#if LWIP_IGMP && !QNETHERNET_ENABLE_PROMISCUOUS_MODE
  // Multicast filtering, to allow desired multicast packets in
  netif_set_igmp_mac_filter(netif, &multicast_filter);
#endif  // LWIP_IGMP && !QNETHERNET_ENABLE_PROMISCUOUS_MODE

#if LWIP_CHECKSUM_CTRL_PER_NETIF
  // Let the stack do whatever the hardware doesn't
  uint16_t chksumFlags = NETIF_CHECKSUM_ENABLE_ALL;
  if (iface->ops->hw_checksum_gen) {
    chksumFlags &= ~(NETIF_CHECKSUM_GEN_IP | NETIF_CHECKSUM_GEN_UDP |
                     NETIF_CHECKSUM_GEN_TCP | NETIF_CHECKSUM_GEN_ICMP);
  }
  if (iface->ops->hw_checksum_check) {
    chksumFlags &= ~(NETIF_CHECKSUM_CHECK_IP | NETIF_CHECKSUM_CHECK_UDP |
                     NETIF_CHECKSUM_CHECK_TCP | NETIF_CHECKSUM_CHECK_ICMP);
  }
  NETIF_SET_CHECKSUM_CTRL(netif, chksumFlags);
#endif  // LWIP_CHECKSUM_CTRL_PER_NETIF

  return ERR_OK;
}

// Removes the interface's netif, if added.
FLASHMEM static void remove_netif(Interface* const iface) {
  if (!iface->isNetifAdded) {
    return;
  }

  const bool wasDefault = (netif_default == &iface->netif);
  netif_remove(&iface->netif);  // This also clears the default if needed
  iface->isNetifAdded = false;

  // Fall back to another interface for the default route
  if (wasDefault) {
    for (Interface* const other : s_interfaces) {
      if (other->isNetifAdded) {
        netif_set_default(&other->netif);
        break;
      }
    }
  }
}

// --------------------------------------------------------------------------
//  Public Interface
// --------------------------------------------------------------------------

Interface* primary_interface() {
  return &s_primary;
}

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)
Interface* wifi_interface() {
  return &s_wifi;
}
#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS

const DriverOps& driver_ops(const Interface* const iface) {
  return *iface->ops;
}

struct netif* netif(Interface* const iface) {
  return &iface->netif;
}

void get_system_mac(Interface* const iface, uint8_t mac[ETH_HWADDR_LEN]) {
  if (mac != nullptr) {
    iface->ops->get_system_mac(mac);
  }
}

bool get_mac(Interface* const iface, uint8_t mac[ETH_HWADDR_LEN]) {
  if (mac == nullptr) {
    return false;
  }
  return iface->ops->get_mac(mac);
}

bool set_mac(Interface* const iface, const uint8_t mac[ETH_HWADDR_LEN]) {
  if (mac == nullptr) {
    return false;
  }
  return iface->ops->set_mac(mac);
}

// This only uses the callback if the interface has not been added.
FLASHMEM bool init(Interface* const iface,
                   const uint8_t mac[ETH_HWADDR_LEN],
                   const EventFn fn, void* const owner,
                   DriverCapabilities* const dc) {
  const DriverOps& ops = *iface->ops;

  if (!ops.init()) {
    errno = ENODEV;
    return false;
  }

  if (dc != nullptr) {
    ops.get_capabilities(dc);
  }

  // Sanitize the inputs
  uint8_t m[ETH_HWADDR_LEN];
  if (mac == nullptr) {
    ops.get_system_mac(m);
    mac = m;
  }

  // Only execute the following code once
  static bool isFirstInit = true;
  if (isFirstInit) {
    lwip_init();
#if LWIP_NETIF_EXT_STATUS_CALLBACK
    netif_add_ext_callback(&s_netifCallback, &netif_event);
#endif  // LWIP_NETIF_EXT_STATUS_CALLBACK
    isFirstInit = false;
  }

  if (iface->isNetifAdded &&
      (std::memcmp(iface->mac, mac, ETH_HWADDR_LEN) != 0)) {
    // If the MAC address has changed then remove the interface and
    // start again
    remove_netif(iface);
  }

  // TODO: Do we really need to check the return of set_mac()?
  if (!ops.set_mac(mac) || !ops.get_mac(iface->mac)) {
    // This shouldn't happen because the driver's been initialized
    errno = EFAULT;
    return false;
  }

  if (!iface->isNetifAdded) {
    // Set these before adding so the "added" event is delivered
    iface->eventFn = fn;
    iface->owner   = owner;
    if (netif_add_noaddr(&iface->netif, iface, init_netif, ethernet_input) ==
        nullptr) {
      errno = ENETDOWN;
      return false;
    }
    if ((netif_default == nullptr) || (iface == &s_primary)) {
      netif_set_default(&iface->netif);
    }
    iface->isNetifAdded = true;
  } else {
    // Just set the MAC address

    (void)std::memcpy(iface->netif.hwaddr, iface->mac, ETH_HWADDR_LEN);
    iface->netif.hwaddr_len = ETH_HWADDR_LEN;
  }

  return true;
}

bool set_default(Interface* const iface) {
  if (!iface->isNetifAdded) {
    return false;
  }
  netif_set_default(&iface->netif);
  return true;
}

FLASHMEM void deinit(Interface* const iface) {
  // Restore state
  (void)std::memset(iface->mac, 0, sizeof(iface->mac));

  remove_netif(iface);  // TODO: This also causes issues (see notes in init())

  iface->ops->deinit();
}

// Processes any input from the interface's driver.
static void proc_input(Interface* const iface) {
  struct netif* const netif = &iface->netif;
  int counter = 0;
  while (true) {
    // Note: It is expected that the driver's proc_input() will return NULL
    //       at some point
    struct pbuf* const p = iface->ops->proc_input(netif, counter++);
    if (p == nullptr) {  // Happens on frame error, pbuf allocation error, or loop end
      break;
    }

    // Process one chunk of input data
    if (netif->input(p, netif) != ERR_OK) {
      (void)pbuf_free(p);
    }
  }
}

void loop() {
  bool anyAdded = false;
  for (Interface* const iface : s_interfaces) {
    if (!iface->isNetifAdded) {
      continue;
    }
    anyAdded = true;
    proc_input(iface);

#if LWIP_NETIF_LOOPBACK || LWIP_HAVE_LOOPIF
    // Poll the netif to allow for loopback
    netif_poll(&iface->netif);
#endif  // LWIP_NETIF_LOOPBACK || LWIP_HAVE_LOOPIF
  }
  if (!anyAdded) {
    return;
  }

  if ((sys_now() - s_lastPollTime) >= kPollInterval) {
    (void)sys_check_timeouts();
    for (Interface* const iface : s_interfaces) {
      if (iface->isNetifAdded) {
        iface->ops->poll(&iface->netif);
      }
    }
    s_lastPollTime = sys_now();
  }
}

#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
bool output_frame(Interface* const iface,
                  const void* const frame, const size_t len) {
  if (iface->ops->output_frame == nullptr) {
    return false;
  }
  if ((frame == nullptr) || (len < (6 + 6 + 2))) {  // dst + src + len/type
    return false;
  }
  if (len > (UINT16_MAX - ETH_PAD_SIZE)) {
    return false;
  }

  const size_t maxFrameLen = iface->ops->max_frame_len;

  // Check length depending on VLAN
  if ((static_cast<const uint8_t*>(frame)[12] ==
       static_cast<uint8_t>(ETHTYPE_VLAN >> 8)) &&
      ((static_cast<const uint8_t*>(frame)[13] ==
        static_cast<uint8_t>(ETHTYPE_VLAN)))) {
    if (len < (6 + 6 + 2 + 2 + 2)) {  // dst + src + VLAN tag + VLAN info + len/type
      return false;
    }
    if (len > maxFrameLen) {
      return false;
    }
  } else {
    // Don't include 4-byte FCS and VLAN
    if ((maxFrameLen < 4) || (len > (maxFrameLen - 4))) {
      return false;
    }
  }

#if QNETHERNET_ENABLE_RAW_FRAME_LOOPBACK
  // Check for a loopback frame
  const bool isOurMAC = (std::memcmp(frame, iface->mac, 6) == 0);
  if (isOurMAC || (std::memcmp(frame, kBroadcastMAC, 6) == 0)) {
    struct pbuf* const p = pbuf_alloc(
        PBUF_RAW, static_cast<uint16_t>(len + ETH_PAD_SIZE), PBUF_POOL);
    if (p != nullptr) {
      const err_t err =
          pbuf_take_at(p, frame, static_cast<uint16_t>(len), ETH_PAD_SIZE);
      if (err != ERR_OK) {
        LWIP_PLATFORM_ASSERT("Expected space for pbuf fill");
      }

      if (iface->netif.input(p, &iface->netif) != ERR_OK) {
        (void)pbuf_free(p);
      }
    }
    // TODO: Collect stats?

    if (isOurMAC) {
      return true;
    }
  }
#endif  // QNETHERNET_ENABLE_RAW_FRAME_LOOPBACK

  return iface->ops->output_frame(frame, len);
}
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT

// --------------------------------------------------------------------------
//  MAC Address Filtering
// --------------------------------------------------------------------------

#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE

bool set_incoming_mac_address_allowed(Interface* const iface,
                                      const uint8_t mac[ETH_HWADDR_LEN],
                                      const bool allow) {
  if (iface->ops->set_incoming_mac_address_allowed == nullptr) {
    return false;
  }
  return iface->ops->set_incoming_mac_address_allowed(mac, allow);
}

#if LWIP_IPV4

// Joins or leaves a multicast group. The flag should be true to join and false
// to leave. This returns whether successful.
ATTRIBUTE_NODISCARD
static bool join_notleave_group(Interface* const iface,
                                const ip4_addr_t* const group,
                                const bool flag) {
  if (group == nullptr) {
    return false;
  }

  // Multicast MAC address.
  const uint8_t multicastMAC[ETH_HWADDR_LEN]{
      LL_IP4_MULTICAST_ADDR_0,
      LL_IP4_MULTICAST_ADDR_1,
      LL_IP4_MULTICAST_ADDR_2,
      static_cast<uint8_t>(ip4_addr2(group) & 0x7f),
      ip4_addr3(group),
      ip4_addr4(group),
  };

  return set_incoming_mac_address_allowed(iface, multicastMAC, flag);
}

bool join_group(Interface* const iface, const ip4_addr_t* const group) {
  return join_notleave_group(iface, group, true);
}

bool leave_group(Interface* const iface, const ip4_addr_t* const group) {
  return join_notleave_group(iface, group, false);
}

#endif  // LWIP_IPV4
#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE

}  // namespace enet
}  // namespace network
}  // namespace qindesign
