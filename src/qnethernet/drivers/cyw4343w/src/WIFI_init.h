//WIFI_init.h
#ifndef WIFI_INIT_H
#define WIFI_INIT_H

#include "join.h"
#include "event.h"

// Boots the chip and registers the join event handler. This does not join a
// network. This returns whether successful.
bool WIFIboot();

#endif
