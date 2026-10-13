// PicoWi IP functions, see http://iosoft.blog/picowi for details
//
// Copyright (c) 2022, Jeremy P Bentham
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =====================================================================
// = Modified for testing with Dogbone06 CYW4343W board and T4.1/DB5   =
// =====================================================================
#ifndef EVENT_H
#define EVENT_H

#include "Arduino.h"
#include <stdint.h>
#include "ioctl_T4.h"
#include "misc_defs.h"
#include "QNEthernet.h"

#define MAX_HANDLERS     20
#define MAX_EVENT_STATUS 16
#define TXDATA_LEN       2048 //1600
#define RXDATA_LEN       2048 //1600

/* IP address is an array of bytes, to avoid misalignment problems */
#define IPLEN    4
typedef uint8_t  IPADDR[IPLEN];


//==============================================================================
// Scan result defines.
//==============================================================================
// For determining security type from a scan
#define DOT11_CAP_PRIVACY             (0x0010)
#define DOT11_IE_ID_RSN               (48)
#define DOT11_IE_ID_VENDOR_SPECIFIC   (221)
#define WPA_OUI_TYPE1                 "\x00\x50\xF2\x01"
// Custom security bitfield returns
#define SEC_OPEN   0        // 0
#define SEC_WEP    (1 << 0) // 1
#define SEC_WPA    (1 << 1) // 2
#define SEC_WPA2   (1 << 2) // 4
#define SEC_WPA3   (1 << 3) // 8
//==============================================================================

//==============================================================================
// Event handling
//==============================================================================
// Event command string struct.
typedef struct {
    int32_t num;
    const char * str;
} EVT_STR;

#define EVT(e)      {e, #e}
#define NO_EVTS     {EVT(-1)}
#define EVENT_SET_SSID      0
#define EVENT_JOIN          1
#define EVENT_AUTH          3
#define EVENT_LINK          16
#define EVENT_MAX           208
#define SET_EVENT(msk, e)   msk[e/8] |= 1 << (e & 7)
//==============================================================================

//==============================================================================
//CYW4343x WIFI struct's.
//==============================================================================
#pragma pack(1)

//==============================================================================
// Ethernet header (sdpcm_ethernet_header_t)
//==============================================================================
typedef struct {
  uint8_t  dest_addr[6],
           srce_addr[6];
  uint16_t type;
} ETHER_HDR;
//==============================================================================

//==============================================================================
// Structure to store ethernet header fields in event packets
//==============================================================================
typedef struct whd_event_eth_hdr {
  uint16_t subtype;      // Vendor specific..32769
  uint16_t length;       // Length of ethernet header
  uint8_t version;       // Version is 0
  uint8_t oui[3];        // Organizationally Unique Identifier
  uint16_t usr_subtype;  // User specific data
} whd_event_eth_hdr_t;
//==============================================================================

//==============================================================================
//  Structure to store fields after ethernet header in event message
//==============================================================================
struct whd_event_msg {
  uint16_t version;               // Version 
  uint16_t flags;                 // see flags below
  uint32_t event_type;            // Event type indicating a response from firmware for IOCTLs/IOVARs sent
  uint32_t status;                // Status code corresponding to any event type
  uint32_t reason;                // Reason code associated with the event occurred
  uint32_t auth_type;             // WLC_E_AUTH: 802.11 AUTH request
  uint32_t datalen;               // Length of data in event message
  whd_mac_t addr;                 // Station address (if applicable)
  char ifname[WHD_MSG_IFNAME_MAX];  // name of the incoming packet interface
  uint8_t ifidx;                  // destination OS i/f index
  uint8_t bsscfgidx;              // source bsscfg index
};

//==============================================================================
// Structure to store ethernet destination, source and ethertype in event packets
//==============================================================================
typedef struct whd_event_ether_header {
  whd_mac_t destination_address; // Ethernet destination address
  whd_mac_t source_address;      // Ethernet source address
  uint16_t ethertype;            // Ethertype for identifying event packets
} whd_event_ether_header_t;
//==============================================================================

//==============================================================================
// Eth event struct.
//==============================================================================
typedef struct {
  whd_event_eth_hdr_t   hdr;
  struct whd_event_msg  msg;
  uint8_t data[1];
} ETH_EVENT;
//==============================================================================

//==============================================================================
// Eth event frame struct.
//==============================================================================
typedef struct {
  uint8_t pad[10];
  whd_event_ether_header_t eth_hdr;
  union {
    ETH_EVENT event;
    uint8_t data[1];
  };
} ETH_EVENT_FRAME;
//==============================================================================

//==============================================================================
// Vendor-specific (Broadcom) Ethernet header (sdpcm_bcmeth_header_t) // 10 bytes
//==============================================================================
typedef struct {
  uint16_t subtype,
           len;
  uint8_t  ver,
           oui[3];
  uint16_t usr_subtype;
} BCMETH_HDR;
//==============================================================================

//==============================================================================
// Raw event header (sdpcm_raw_event_header_t)
//==============================================================================
typedef struct {
  uint16_t ver,
           flags;
  uint32_t event_type,
           status,
           reason,
           auth_type,
           datalen;
  uint8_t  addr[6];
  char     ifname[16];
  uint8_t  ifidx,
           bsscfgidx;
} EVENT_HDR;
//==============================================================================

//==============================================================================
// Async event parameters, used internally
//==============================================================================
typedef struct {
  uint32_t chan;                      // From SDPCM header
  uint32_t event_type, status, reason;// From async event (null if not event)
  uint16_t flags;
  uint16_t link;                      // Link state
  uint32_t join;                      // Joining state
  uint8_t  *data;                     // Data block
  int      dlen;
  int      server_port;               // Port number if server
  int      sock;                      // Socket number if TCP
} EVENT_INFO;
//==============================================================================

//==============================================================================
// Ethernet (DIX) header.
//==============================================================================
typedef struct {
  MACADDR  dest;               /* Destination MAC address */
  MACADDR  srce;               /* Source MAC address */
  uint16_t ptype;              /* Protocol type or length */
} ETHERHDR;
//==============================================================================

//==============================================================================
// IP (Internet Protocol) header.
//==============================================================================
typedef struct {
  uint8_t  vhl,         /* Version and header len */
           service;     /* Quality of IP service */
  uint16_t len,         /* Total len of IP datagram */
           ident,       /* Identification value */
           frags;       /* Flags & fragment offset */
  uint8_t  ttl,         /* Time to live */
           pcol;        /* Protocol used in data area */
  uint16_t check;       /* Header checksum */
  IPADDR   sip,         /* IP source addr */
           dip;         /* IP dest addr */
} IPHDR;
//==============================================================================

//==============================================================================
// WIFI transmit message struct.
//==============================================================================
typedef struct {
  SDPCM_HDR sdpcm;
  uint16_t pad;
  BDC_HDR_T4 bdc;
  uint8_t data[TXDATA_LEN];
} TX_MSG;
//==============================================================================

#pragma pack()

//==============================================================================
// Event handler callback function pointer.
//==============================================================================
typedef int (*event_handler_t)(EVENT_INFO *eip);
//==============================================================================
  
class Event;

//==============================================================================
// WIFI event class.
//==============================================================================
class Event {
public:
  
  bool init();
  bool add_event_handler(event_handler_t);
  bool add_server_event_handler(event_handler_t fn, uint16_t port);
  int event_handle(EVENT_INFO *eip);
  int pollEvents(void);
  int ioctl_enable_evts(EVT_STR *evtp);  
  uint16_t add_csum(uint16_t sum, void *dp, int count);
  int ip_add_data(uint8_t *buff, const void *data, int len);
  int ip_add_hdr(uint8_t *buff, IPADDR dip, uint8_t pcol, uint16_t dlen);
  int ip_add_eth(uint8_t *buff, MACADDR dmac, MACADDR smac, uint16_t pcol);
  uint32_t ioctl_get_event(sdpcm_header_t *hp, uint8_t *data, int maxlen);
  int event_net_tx(void *data, int len);
  uint16_t htons(uint16_t w);

protected:
  
private:
  void ip_cpy(uint8_t *dest, uint8_t *src);
  void print_ip_addr(IPADDR a);
  void print_mac_addr(MACADDR mac);
  uint8_t sd_tx_seq;
  uint8_t eventbuf[RXDATA_LEN];
  uint8_t event_mask[EVENT_MAX / 8];
  ETH_EVENT_FRAME *eep = (ETH_EVENT_FRAME *)eventbuf;
  EVT_STR *currentE_evts;
  uint16_t event_ports[MAX_HANDLERS];
  const char * event_status[MAX_EVENT_STATUS] = {
    "SUCCESS","FAIL","TIMEOUT","NO_NETWORK","ABORT","NO_ACK",
    "UNSOLICITED","ATTEMPT","PARTIAL","NEWSCAN","NEWASSOC",
    "11HQUIET","SUPPRESS","NOCHANS","CCXFASTRM","CS_ABORT" };
  MACADDR bcast_mac = {0xff,0xff,0xff,0xff,0xff,0xff};
  TX_MSG tx_msg = {.sdpcm = {
	               .chan = SDPCM_CHAN_DATA,
	               .hdrlen = sizeof(SDPCM_HDR)+2},
                   .bdc = {
				   .flags=0x20}
				  };
};
// The one Event instance, defined in event.cpp. Use this rather than creating
// more; each instance has large frame buffers.
extern Event evt;

//==============================================================================
// EOF
#endif
