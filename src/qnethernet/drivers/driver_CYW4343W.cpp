

#include "qnethernet/lwip_driver.h"

#ifdef QNETHERNET_INTERNAL_DRIVER_CYW4343W

#pragma message("Using CYW4343Wx QNEthernet Driver")

// C++ includes
#include <atomic>
#include <cstring>

#include <core_pins.h>
#include <imxrt.h>
#include <util/atomic.h>

#include "lwip/debug.h"
#include "lwip/err.h"
#include "lwip/stats.h"

//#include "qnethernet/internal/macro_funcs.h"
#include "qnethernet/platforms/pgmspace.h"

#include "cyw4343w/src/cyw43_T4_SDIO.h"
#include "cyw4343w/src/join.h"
#include "cyw4343w/src/scan.h"
#include "qnethernet/drivers/driver_CYW4343W_wifi.h"

// Chip state shared with the cyw4343w sources
extern W4343WCard WIFIcard;
extern Join join;
extern EVENT_INFO event_info;
extern EVT_STR join_evts[];
extern Scan scn;

namespace qindesign {
namespace network {
namespace driver_cyw4343w {

static uint32_t poll_ticks;
static struct LinkInfo s_linkInfo;

// Network to join
static char s_ssid[kMaxSSIDLen + 1]{'\0'};
static char s_passphrase[kMaxPassphraseLen + 1]{'\0'};
static Security s_security = Security::kWPA2;
static bool s_credentialsChanged = false;

// Whether the join state machine is running
static bool s_joinEnabled = false;

FLASHMEM void get_capabilities(DriverCapabilities* const dc) {
  dc->isMACSettable                = false;
  dc->isLinkStateDetectable        = true;
  dc->isLinkSpeedDetectable        = false;
  dc->isLinkSpeedSettable          = false;
  dc->isLinkFullDuplexDetectable   = false;
  dc->isLinkFullDuplexSettable     = false;
  dc->isAutoNegotiationSettable    = false;
  dc->isLinkCrossoverDetectable    = false;
  dc->isAutoNegotiationRestartable = false;
  dc->isPHYResettable              = false;
}

typedef enum {
  kInitStateStart,           // Unknown hardware
  kInitStateNoHardware,      // No PHY
  kInitStateHasHardware,     // Has PHY
  kInitStatePHYInitialized,  // PHY's been initialized
  kInitStateInitialized,     // PHY and MAC have been initialized
} enet_init_states_t;

static enet_init_states_t s_initState = kInitStateStart;

//==============================================================================
// Has Hardware. This boots the chip the first time.
//==============================================================================
bool has_hardware() {
  switch (s_initState) {
    case kInitStateHasHardware:
      ATTRIBUTE_FALLTHROUGH;
    case kInitStatePHYInitialized:
      ATTRIBUTE_FALLTHROUGH;
    case kInitStateInitialized:
      return true;
    case kInitStateNoHardware:
      return false;
    default:
      break;
  }
  s_initState = WIFIboot() ? kInitStateHasHardware : kInitStateNoHardware;
  return (s_initState != kInitStateNoHardware);
}

//==============================================================================
// Get System MAC address.
//==============================================================================
void get_system_mac(uint8_t mac[ETH_HWADDR_LEN]) {
  if (!has_hardware()) {
    std::memset(mac, 0, ETH_HWADDR_LEN);
    return;
  }
  WIFIcard.getMACAddress(mac);
}

//==============================================================================
// Get current MAC address.
//==============================================================================
bool get_mac(uint8_t mac[ETH_HWADDR_LEN]) {
  if (!has_hardware()) {
    return false;
  }
  WIFIcard.getMACAddress(mac);
  return true;
}

//==============================================================================
// Set MAC address (Not settable).
//==============================================================================
bool set_mac(const uint8_t mac[ETH_HWADDR_LEN]) {
  (void)mac;
  return true;
}

//==============================================================================
// UNUSED.
//==============================================================================
void set_chip_select_pin(int pin) {
  (void)pin;
}

// Starts joining the configured network, if there is one.
static void start_join() {
  s_credentialsChanged = false;
  if (s_ssid[0] == '\0') {
    s_joinEnabled = false;
    return;
  }
  event_info.join = JOIN_IDLE;
  event_info.link = 0;
  if (!join.join_start(s_ssid, s_passphrase, static_cast<int>(s_security))) {
    // Let the join state machine retry
    event_info.join = JOIN_FAIL;
  }
  s_joinEnabled = true;
}

//==============================================================================
// Initialize: boot the chip if needed and start joining the network.
//==============================================================================
bool init(void) {
  if (!has_hardware()) {
    return false;
  }
  s_initState = kInitStateInitialized;
  if (!s_joinEnabled || s_credentialsChanged) {
    start_join();
  }
  return true;
}

//==============================================================================
// Leave network.
//==============================================================================
void deinit() {
  if (s_joinEnabled) {
    join.join_stop();
    s_joinEnabled = false;
  }
  event_info.join = JOIN_IDLE;
  event_info.link = 0;
  if (s_initState == kInitStateInitialized) {
    s_initState = kInitStateHasHardware;
  }
}

//==============================================================================
// driver_proc_input().
//==============================================================================
struct pbuf* proc_input(struct netif *netif, int counter) {
  // Finish any pending link and join status check
  if(netif_is_link_up(netif) == 0) return NULL; 
  sdpcm_header_t hp; // Get capy of WIFI header struct.
  // The Ethernet frame follows a 10-byte header
  constexpr uint32_t kHeaderLen = 10;
  uint8_t bf[QNETHERNET_INTERNAL_CYW4343W_RX_BUF_SIZE + kHeaderLen];
  uint32_t data_len;
  // Check for an available event frame.
  if((data_len = evt.ioctl_get_event(&hp, bf,
                                     QNETHERNET_INTERNAL_CYW4343W_RX_BUF_SIZE)) > 0) {
    // Note: data_len is kept as the frame length for compatibility; any
    //       extra trailing bytes are ignored by the stack
    struct pbuf *p = pbuf_alloc(PBUF_RAW, data_len+ETH_PAD_SIZE, PBUF_RAM);
    if (p == NULL) {
      printf("Failed to allocate pbuf\n");
      return NULL;
    }
    if (pbuf_take_at(p, bf + kHeaderLen, data_len, ETH_PAD_SIZE) != ERR_OK) {
      LWIP_PLATFORM_ASSERT("Expected space for pbuf fill");
    }
    return p;
  }
  return NULL;
}

//==============================================================================
// Poll network.
//==============================================================================
void poll(struct netif *netif) {
  if (s_initState != kInitStateInitialized) {
    return;
  }
  // Get any events, poll the joining state machine.
  if(WIFIcard.ustimeout(&poll_ticks, EVENT_POLL_USEC)) {
    evt.pollEvents(); // Check for an event frame.
    // Poll join status. Try to re-join if link is lost.
    if (s_joinEnabled) {
      join.join_state_poll(s_ssid, s_passphrase, static_cast<int>(s_security));
    }
    WIFIcard.ustimeout(&poll_ticks, 0); // Clear Polling timout counter.
  }
  // Check if link is up.
  const bool linkUp = s_joinEnabled && (join.join_check() == JOIN_OK);
  if (static_cast<bool>(netif_is_link_up(netif)) != linkUp) {
    if (linkUp) {
      netif_set_link_up(netif);
    } else {
      netif_set_link_down(netif);
    }
  }
}

//==============================================================================
// 
//==============================================================================
err_t output(struct pbuf *p) {
  // The pbuf starts with ETH_PAD_SIZE bytes of padding
  if (p->tot_len <= ETH_PAD_SIZE) {
    return ERR_BUF;
  }
  const uint16_t len = p->tot_len - ETH_PAD_SIZE;
  if (len > TXDATA_LEN) {  // event_net_tx() doesn't check
    return ERR_BUF;
  }
  // Create a packet buffer.
  uint8_t *buffer = (uint8_t *)malloc(len);
  if (buffer == NULL) {
    return ERR_MEM;
  }
  // Fill buffer with packet info.
  const uint16_t copied = pbuf_copy_partial(p, buffer, len, ETH_PAD_SIZE);
  if (copied != len) {
    free(buffer);
    return ERR_BUF;
  }
  // Transmit buffer.
  evt.event_net_tx(buffer, copied);
  free(buffer);  // Free buffer mem.
  return ERR_OK;
}

#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
//==============================================================================
// Same as output.
//==============================================================================
bool output_frame(const void *frame, size_t len) {
  if (len > TXDATA_LEN) {  // event_net_tx() doesn't check
    return false;
  }
  // The frame has no padding; event_net_tx() copies it
  evt.event_net_tx(const_cast<void*>(frame), static_cast<int>(len));
  return true;
}
#endif

//==============================================================================
// Basic, is link up or down?
//==============================================================================
void get_link_info(struct LinkInfo* const li) {
  *li = s_linkInfo;
}


#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE

//==============================================================================
// Currently Unused.
//==============================================================================
bool set_incoming_mac_address_allowed(const uint8_t mac[ETH_HWADDR_LEN],bool allow) {
  return false;
}

#endif

//==============================================================================
// Start at begin().
//==============================================================================
bool is_unknown() {
  return s_initState == kInitStateStart;;
}

// --------------------------------------------------------------------------
//  WiFi Functions
// --------------------------------------------------------------------------

bool set_credentials(const char* const ssid, const char* const passphrase,
                     const Security security) {
  if (ssid == nullptr) {
    return false;
  }
  const size_t ssidLen = std::strlen(ssid);
  if ((ssidLen == 0) || (ssidLen > kMaxSSIDLen)) {
    return false;
  }
  const char* const pass =
      ((security == Security::kOpen) || (passphrase == nullptr)) ? ""
                                                                 : passphrase;
  const size_t passLen = std::strlen(pass);
  if ((security != Security::kOpen) &&
      ((passLen < kMinPassphraseLen) || (passLen > kMaxPassphraseLen))) {
    return false;
  }

  if ((std::strcmp(s_ssid, ssid) != 0) ||
      (std::strcmp(s_passphrase, pass) != 0) || (s_security != security)) {
    std::memcpy(s_ssid, ssid, ssidLen + 1);
    std::memcpy(s_passphrase, pass, passLen + 1);
    s_security = security;
    s_credentialsChanged = true;
  }
  return true;
}

const char* ssid() {
  return s_ssid;
}

JoinStatus join_status() {
  if (!s_joinEnabled) {
    return JoinStatus::kStopped;
  }
  switch (event_info.join) {
    case JOIN_OK:
      return JoinStatus::kJoined;
    case JOIN_FAIL:
      return JoinStatus::kFailed;
    default:
      return JoinStatus::kJoining;
  }
}

bool rssi(int32_t* const value) {
  if (join_status() != JoinStatus::kJoined) {
    return false;
  }
  int32_t v = 0;
  if (WIFIcard.ioctl_rd_data(WLC_GET_RSSI, IOCTL_WAIT, &v, sizeof(v)) <= 0) {
    return false;
  }
  *value = v;
  return true;
}

bool bssid(uint8_t mac[6]) {
  if (join_status() != JoinStatus::kJoined) {
    return false;
  }
  return (WIFIcard.ioctl_rd_data(WLC_GET_BSSID, IOCTL_WAIT, mac, 6) > 0);
}

// Number of results from the last scan.
static int s_scanCount = 0;

int scan(const uint32_t timeout) {
  s_scanCount = 0;
  if (!has_hardware()) {
    return -1;
  }

  static bool handlerAdded = false;
  if (!handlerAdded) {
    if (!evt.add_event_handler(&Scan::scan_event_handler)) {
      return -1;
    }
    handlerAdded = true;
  }

  // Note: This replaces the enabled events with the scan events
  const bool started = scn.scan_start();
  if (started) {
    // Don't call yield() here because the stack's input processing would
    // consume the scan events
    const uint32_t start = millis();
    while ((millis() - start) < timeout) {
      if (evt.pollEvents() < 0) {  // Scan complete
        break;
      }
      delayMicroseconds(1000);
    }
    s_scanCount = scn.getScanCount();
  }

  // Restore the join events
  if (s_joinEnabled) {
    (void)evt.ioctl_enable_evts(join_evts);
  }

  return started ? s_scanCount : -1;
}

bool scan_result(const int index, ScanResult* const result) {
  if ((index < 0) || (index >= s_scanCount) || (result == nullptr)) {
    return false;
  }
  const simple_scan_result_t& r = scn.getFilteredScanResults()[index];
  const size_t ssidLen = strnlen(reinterpret_cast<const char*>(r.ssid),
                                 sizeof(r.ssid));
  std::memcpy(result->ssid, r.ssid, ssidLen);
  result->ssid[ssidLen] = '\0';
  std::memcpy(result->bssid, r.bssid, sizeof(result->bssid));
  result->rssi     = r.signal_strength;
  result->channel  = r.channel;
  result->security = r.security_mask;
  return true;
}

// --------------------------------------------------------------------------
//  Ops Table
// --------------------------------------------------------------------------

const DriverOps kDriverOps{
    {'w', 'l'},
    MTU,
    MAX_FRAME_LEN,
    -1 /* EthernetOtherHardware */,
    false,
    false,
    &get_capabilities,
    &is_unknown,
    &get_system_mac,
    &get_mac,
    &set_mac,
    &has_hardware,
    &set_chip_select_pin,
    &init,
    &deinit,
    &proc_input,
    &poll,
    &get_link_info,
    nullptr,
    &output,
#if QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
    &output_frame,
#endif  // QNETHERNET_ENABLE_RAW_FRAME_SUPPORT
#if !QNETHERNET_ENABLE_PROMISCUOUS_MODE
    &set_incoming_mac_address_allowed,
#endif  // !QNETHERNET_ENABLE_PROMISCUOUS_MODE
    nullptr,
    nullptr,
    nullptr,
};

}  // namespace driver_cyw4343w
}  // namespace network
}  // namespace qindesign

#endif
