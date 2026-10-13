// WIFI_init.cpp

#include "cyw43_T4_SDIO.h"
#include "misc_defs.h"
#include "WIFI_init.h"

extern W4343WCard WIFIcard;
Join join;
Event evt;
MACADDR llmac;

bool WIFIboot() {
  // Add our event handler to the array of event handlers.
  static bool handlerAdded = false;
  if (!handlerAdded) {
    evt.add_event_handler(join.join_event_handler);
    handlerAdded = true;
  }
  delayMicroseconds(1000);

  //////////////////////////////////////////
  // Begin parameters: 
  // SDIO1 (false), SDIO2 (true)
  // WL_REG_ON pin 
  // WL_IRQ pin (-1 to ignore)
  // EXT_LPO pin (optional, -1 to ignore)
  // WIFIcard.begin(true, 33, 34, -1) for CYW4343W board or 
  // WIFIcard.begin(true, 30, 29, -1) for CYW43439 board. 
  //////////////////////////////////////////
  if (WIFIcard.begin(true, REG_ON, WL_IRQ, -1) != true) {
    printf("*************** Initialization Failed! ****************\n");
    return false;
  }
  WIFIcard.wifiSetup(); // Only needed for wifi scan usage
  WIFIcard.postInitSettings();
  delay(500); // Give the CYW4343W time to settle in.
  WIFIcard.getMACAddress(llmac); // Get Low Level MAC Addrees.
  return true;
}
