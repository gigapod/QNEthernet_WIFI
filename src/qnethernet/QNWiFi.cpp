// SPDX-FileCopyrightText: (c) 2026 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// QNWiFi.cpp contains the WiFi interface implementation.
// This file is part of the QNEthernet library.

#include "QNWiFi.h"

#if defined(QNETHERNET_INTERNAL_WIFI_DRIVER_OPS)

namespace qindesign {
namespace network {

// A reference to the singleton.
STATIC_INIT_DEFN(WiFiClass, WiFi);

}  // namespace network
}  // namespace qindesign

#endif  // QNETHERNET_INTERNAL_WIFI_DRIVER_OPS
