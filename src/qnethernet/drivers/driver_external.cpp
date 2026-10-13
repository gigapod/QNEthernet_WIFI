// SPDX-FileCopyrightText: (c) 2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// driver_external.cpp wraps an external driver's `driver::x()` functions in an
// ops table.
// This file is part of the QNEthernet library.

#include "qnethernet/lwip_driver.h"

#if defined(QNETHERNET_INTERNAL_DRIVER_EXTERNAL)

namespace qindesign {
namespace network {
namespace driver_external {

const DriverOps kDriverOps{
    {'e', 'n'},
    MTU,
    MAX_FRAME_LEN,
    -1 /* EthernetOtherHardware */,
    &driver::get_capabilities,
    &driver::is_unknown,
    &driver::get_system_mac,
    &driver::get_mac,
    &driver::set_mac,
    &driver::has_hardware,
    &driver::set_chip_select_pin,
    &driver::init,
    &driver::deinit,
    &driver::proc_input,
    &driver::poll,
    &driver::get_link_info,
    &driver::set_link,
    &driver::output,
#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
    &driver::output_frame,
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE
    &driver::set_incoming_mac_address_allowed,
#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE
    &driver::notify_manual_link_state,
    &driver::restart_auto_negotiation,
    &driver::reset_phy,
};

}  // namespace driver_external
}  // namespace network
}  // namespace qindesign

#endif  // QNETHERNET_INTERNAL_DRIVER_EXTERNAL
