// Only compile when the CYW4343W driver is selected
#include "qnethernet/driver_select.h"
#if defined(QNETHERNET_INTERNAL_DRIVER_CYW4343W)

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

#include "Arduino.h"
#include <stdint.h>
#include "cyw43_T4_SDIO.h"
#include "event.h"
#include "SdioRegs.h"
#include "misc_defs.h"

using namespace qindesign::network;

W4343WCard WIFIcard;

extern void cwydump(unsigned char *memory, unsigned int len);
extern uint8_t my_mac[6];
extern int display_mode;
extern IOCTL_MSG ioctl_txmsg, ioctl_rxmsg;

// The one Event instance; the event handlers and state are shared.
Event evt;
static event_handler_t event_handlers[MAX_HANDLERS];
EVENT_INFO event_info;
// Event field displays
char ioctl_event_hdr_fields[] = "2:len 2: 1:seq 1:chan 1: 1:hdrlen 1:flow 1:credit";
char eth_hdr_fields[]         = "6:dest 6:srce 2;type";
char event_hdr_fields[]       = "2;sub 2;len 1: 3;oui 2;usr";
char event_msg_fields[]       = "2;ver 2;flags 4;type 4;status 4;reason 4:auth 4;dlen 6;addr 18:";

static int num_handlers = 0; // Counter for number of event handlers.
static IPADDR my_ip; // Local IP address variable.
uint8_t scan_count = 0;

sdpcm_header_t iehh; // Used here and scan.cpp as well.

// =====================================================================
// Add an event handler to the chain
// =====================================================================
bool Event::add_event_handler(event_handler_t fn) {
  return(add_server_event_handler(fn , 0));
}

// =====================================================================
// Add a server event handler to the chain (with local port number)
// =====================================================================
bool Event::add_server_event_handler(event_handler_t fn, uint16_t port) {
  bool ok = num_handlers < MAX_HANDLERS;
  if(ok) {
    event_ports[num_handlers] = port;
    event_handlers[num_handlers++] = fn;
  }
  return (ok);
}

// =====================================================================
/* Calculate TCP-style checksum, add to old value */
// =====================================================================
uint16_t Event::add_csum(uint16_t sum, void *dp, int count) {
  uint16_t n=count>>1, *p=(uint16_t *)dp, last=sum;
  while (n--) {
    sum += *p++;
    if(sum < last) sum++;
    last = sum;
  }
  if (count & 1) sum += *p & 0x00ff;
  if (sum < last) sum++;
  return(sum);
}

// Add data to buffer, return length
int Event::ip_add_data(uint8_t *buff, const void *data, int len) {
    if (len>0 && data) memcpy(buff, data, len);
    return(len);
}

// Add IP header to buffer, return length
int Event::ip_add_hdr(uint8_t *buff, IPADDR dip, uint8_t pcol, uint16_t dlen) {
    static uint16_t ident=1;
    IPHDR *ip=(IPHDR *)buff;

    ip->ident = htons(ident++);
    ip->frags = 0;
    ip->vhl = 0x40+(sizeof(IPHDR)>>2);
    ip->service = 0;
    ip->ttl = 100; // Time To Live.
    ip->pcol = pcol;
    ip_cpy(ip->sip, my_ip);
    ip_cpy(ip->dip, dip);
    ip->len = htons(dlen + sizeof(IPHDR));
    ip->check = 0;
    ip->check = 0xffff ^ add_csum(0, ip, sizeof(IPHDR));
    return(sizeof(IPHDR));
}

// =====================================================================
// Add Ethernet header to buffer, return byte count
// =====================================================================
int Event::ip_add_eth(uint8_t *buff, MACADDR dmac, MACADDR smac, uint16_t pcol) {
    ETHERHDR *ehp = (ETHERHDR *)buff;
    MAC_CPY(ehp->dest, dmac);
    MAC_CPY(ehp->srce, smac);
    ehp->ptype = htons(pcol);
    return(sizeof(ETHERHDR));
}

// =====================================================================
// Run event handlers, until one returns non-zero
// =====================================================================
int Event::event_handle(EVENT_INFO *eip) {
  int i, ret=0;
  for(i=0; i<num_handlers && !ret; i++) {
    eip->server_port = event_ports[i];
    ret = event_handlers[i](eip);
  }
  return(ret);
}

// =====================================================================
// Enable events
// =====================================================================
int Event::ioctl_enable_evts(EVT_STR *evtp) {
  currentE_evts = evtp;
  memset(event_mask, 0, sizeof(event_mask));
  while (evtp->num >= 0) {
    if(evtp->num / 8 < (int32_t)sizeof(event_mask))
      SET_EVENT(event_mask, evtp->num);
    evtp++;
  }
  return WIFIcard.ioctl_set_data("event_msgs", 0, event_mask, sizeof(event_mask));
}

// =====================================================================
// Poll events
// =====================================================================
int Event::pollEvents() {
  int n, ret = 0;
  EVENT_INFO *eip = &event_info;
  
  //Check for an event response
  if((n=ioctl_get_event(&iehh, eventbuf, sizeof(eventbuf))) > 0) {
    eip->chan = iehh.sw_header.chan; // chan = ctrl, evt or data.
    eip->flags = SWAP16(eep->event.msg.flags);
    eip->event_type = SWAP32(eep->event.msg.event_type);
    eip->status = SWAP32(eep->event.msg.status);
    eip->reason = SWAP32(eep->event.msg.reason);
    // Vendor-specific (Broadcom) Ethernet header (sdpcm_bcmeth_header_t) // 10 bytes
    eip->data = eventbuf+sizeof(BCMETH_HDR); // +10
    //NOTE: Need to move eventbuf ahead by 10 bytes.
    eip->dlen = n; // Size of received data in bytes.
    eip->sock = -1;
    ret = event_handle(eip); // Distribute to proper event handler.
  }
  return ret;
}

// =====================================================================
// Get event data, return data length excluding header
// =====================================================================
uint32_t Event::ioctl_get_event(sdpcm_header_t *hp, uint8_t *data, int maxlen) {
  int n=0, dlen=0, blklen;
  bool res = false;
  hp->len = 0;
  res = WIFIcard.cardCMD53_read(SD_FUNC_RAD, SB_32BIT_WIN, (uint8_t *)hp, sizeof(sdpcm_header_t), false);
  if(res == true && hp->len > sizeof(sdpcm_header_t) && hp->notlen > 0 && hp->len == (hp->notlen^0xffff)) {
    dlen = hp->len - sizeof(sdpcm_header_t);  //Strip off sdpcm_header_t.
    while (n < dlen && n < maxlen) {
      blklen = MIN(MIN(maxlen - n, hp->len - n), IOCTL_MAX_BLKLEN_T4);
      WIFIcard.cardCMD53_read(SD_FUNC_RAD, SB_32BIT_WIN, (uint8_t *)(&data[n]), blklen, false);
      n += blklen;
    }
    //Read and discard remaining bytes over maxlen
    while (n < dlen) {
      blklen = MIN(hp->len - n, IOCTL_MAX_BLKLEN_T4);
      WIFIcard.cardCMD53_read(SD_FUNC_RAD, SB_32BIT_WIN, 0, blklen, false);
      n += blklen;
    }
  }
  return dlen > maxlen ? maxlen : dlen;
}

// =====================================================================
// Transmit network data
// =====================================================================
int Event::event_net_tx(void *data, int len) {
  TX_MSG *txp = &tx_msg;
  uint8_t *dp = (uint8_t *)txp;
  int txlen = sizeof(SDPCM_HDR)+2+sizeof(BDC_HDR_T4)+len;
  txp->sdpcm.len = txlen;
  txp->sdpcm.notlen = ~txp->sdpcm.len;
  txp->sdpcm.seq = sd_tx_seq++;
  memcpy(txp->data, (uint8_t *)data, len);
  while (txlen & 3) dp[txlen++] = 0;
  return (WIFIcard.cardCMD53_write(SD_FUNC_RAD, 0, (uint8_t *)dp, txlen, false));
}

// =====================================================================
// Convert byte-order in a 'short' variable (local version)
// =====================================================================
uint16_t Event::htons(uint16_t w) {
  return(w<<8 | w>>8);
}

// =====================================================================
// Copy IP address (byte-by-byte, in case misaligned)
// =====================================================================
void Event::ip_cpy(uint8_t *dest, uint8_t *src) {
  *dest++ = *src++;
  *dest++ = *src++;
  *dest++ = *src++;
  *dest = *src;
}

// =====================================================================
// Display IP address
// =====================================================================
void Event::print_ip_addr(IPADDR a) {
  printf("%u.%u.%u.%u", a[0],a[1],a[2],a[3]);
}

// =====================================================================
// Display MAC address
// =====================================================================
void Event::print_mac_addr(MACADDR mac) {
  printf("%02X:%02X:%02X:%02X:%02X:%02X",
         mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

#endif  // QNETHERNET_INTERNAL_DRIVER_CYW4343W
