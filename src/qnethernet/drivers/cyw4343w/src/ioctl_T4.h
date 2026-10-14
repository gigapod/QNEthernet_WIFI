// CYW4343W SDIO IOCTL functions.
//
// Copyright (c) 2026, Warren Watson
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
// ===============================================================
// = Used for testing with Dogbone06 CYW4343W board and T4.1/DB5 =
// ===============================================================
#ifndef IOCTL_T4H
#define IOCTL_T4H

#include <Arduino.h>
#include "whd_wlioctl.h"

//==============================================================================
// IOCTL command defines.
//==============================================================================
#define IOCTL_UP                    2
#define IOCTL_SET_SCAN_CHANNEL_TIME 0xB9

#define IOCTL_POLL_MSEC     2

#define IOCTL_WAIT          30      // Time to wait for ioctl response (msec)
#define IOCTL_WAIT_USEC     2000 //2000
#define IOCTL_IRQ_WAIT_USEC 5000    // Max wait for the response IRQ before polling
#define IOCTL_BUSY_MAX_USEC 100000  // Max wait for the bus to go idle
#define MAX_CHUNK_LEN       400

#define IOCTL_MAX_BLKLEN_T4 512  // cyw43_T4_SDIO needs this.
#define IOCTL_MAX_BLKLEN    1600 // pico versions need this.

#define SSID_MAXLEN         32

#define DL_BEGIN			0x0002
#define DL_END				0x0004
#define DL_TYPE_CLM		    2

#define SDPCM_CHAN_CTRL     0   // SDPCM control channel
#define SDPCM_CHAN_EVT      1   // SDPCM async event channel
#define SDPCM_CHAN_DATA     2   // SDPCM data channel

// WiFi bands
#define WIFI_BAND_ANY       0
#define WIFI_BAND_5GHZ      1
#define WIFI_BAND_2_4GHZ    2

#define WHD_MSG_IFNAME_MAX 16    // Max length of Interface name

typedef uint16_t wl_chanspec_t;  // Channel specified in uint16_t
#define MCSSET_LEN    16         // Maximum allowed mcs rate
//==============================================================================

#pragma pack(1)

//==============================================================================
// WIFI IOCTL structs.
//==============================================================================

//==============================================================================
// Data load struct (litte endian).
//==============================================================================
struct brcmf_dload_data_le {
  uint16_t flag;
  uint16_t dload_type;
  uint32_t len;
  uint32_t crc;
  uint8_t data[1];
};
//==============================================================================

//==============================================================================
// Data SSID struct (litte endian).
//==============================================================================
struct brcmf_ssid_le {
  uint32_t SSID_len;
  uint8_t SSID[SSID_MAXLEN];
};
//==============================================================================

//==============================================================================
// CYW4343x sdpcm header. Used at the beginning of all comms packets.
//==============================================================================
typedef struct {
  uint8_t seq,      
          chan,
          nextlen,
          hdrlen,
          flow,
          credit,
          reserved[2];
} sdpcm_sw_header;
//==============================================================================

//==============================================================================
// IOCTL command header.
//==============================================================================
typedef struct {
  sdpcm_sw_header sw_header;
  uint32_t cmd;       // cdc_header
  uint16_t outlen,
           inlen;
  uint32_t flags,
           status;
  uint8_t data[IOCTL_MAX_BLKLEN_T4];
} IOCTL_CMD_T4;
//==============================================================================

//==============================================================================
// IOCTL glom header.
//==============================================================================
typedef struct {
  uint16_t len;
  uint8_t  reserved1,
           flags,
           reserved2[2],
           pad[2];
} IOCTL_GLOM_HDR;
//==============================================================================

//==============================================================================
// IOCTL glom command.
//==============================================================================
typedef struct {
    IOCTL_GLOM_HDR glom_hdr;
    IOCTL_CMD_T4  cmd;
} IOCTL_GLOM_CMD;
//==============================================================================

//==============================================================================
// IOCTL message header. 
//==============================================================================
typedef struct {
  uint16_t len,    // sdpcm_header.frametag
           notlen;
    union {
      IOCTL_CMD_T4 cmd;
      IOCTL_GLOM_CMD glom_cmd;
    };
} IOCTL_MSG_T4;
//==============================================================================

//==============================================================================
// SDPCM header version one. 
//==============================================================================
typedef struct {
    uint16_t        len,       // sdpcm_header.frametag
                    notlen;
    sdpcm_sw_header sw_header;
} sdpcm_header_t;
//==============================================================================

//==============================================================================
// SDPCM header version two. TODO: Try to replace this version with the above version.
//==============================================================================
typedef struct {
    uint16_t len,       // sdpcm_header.frametag
             notlen;
    uint8_t  seq,       // sdpcm_sw_header
             chan,
             nextlen,   // Data offset
             hdrlen,
             flow,
             credit,
             reserved[2];
} SDPCM_HDR;
//==============================================================================

//==============================================================================
// BDC header.
//==============================================================================
typedef struct {
  uint8_t flags;
  uint8_t priority;
  uint8_t flags2;
  uint8_t offset;
} BDC_HDR_T4;
//==============================================================================

//==============================================================================
// IOCTL response with SDPCM header
// (then an IOCTL header after some padding)
//==============================================================================
typedef union {
  SDPCM_HDR sdpcm;
  uint8_t data[IOCTL_MAX_BLKLEN];
} IOCTL_RSP;
//==============================================================================

//==============================================================================
// IOCTL command or response message
//==============================================================================
typedef struct {
  union {
    IOCTL_CMD_T4 cmd;
    IOCTL_RSP rsp;
    uint8_t data[IOCTL_MAX_BLKLEN];
  };
} IOCTL_MSG;
//==============================================================================

#pragma pack()

/* List of events */
#define WLC_E_NONE                         (0x7FFFFFFE) /**< Indicates the end of the event array list */

#define WLC_E_SET_SSID                     0 /**< Indicates status of set SSID. This event occurs when STA tries to join the AP*/
#define WLC_E_AUTH                         3 /**< 802.11 AUTH request event occurs when STA tries to get authenticated with the AP  */
#define WLC_E_DEAUTH                       5 /**< 802.11 DEAUTH request event occurs when the the SOFTAP is stopped to deuthenticate the connected stations*/
#define WLC_E_DEAUTH_IND                   6 /**< 802.11 DEAUTH indication event occurs when the STA gets deauthenticated by the AP */
#define WLC_E_ASSOC                        7 /**< 802.11 ASSOC request event occurs when STA joins the AP */
#define WLC_E_ASSOC_IND                    8 /**< 802.11 ASSOC indication occurs when a station joins the SOFTAP that is started */
#define WLC_E_REASSOC                      9 /**< 802.11 REASSOC request event when the STA again gets associated with the AP */
#define WLC_E_REASSOC_IND                 10 /**< 802.11 REASSOC indication occurs when a station again reassociates with the SOFTAP*/
#define WLC_E_DISASSOC                    11 /**< 802.11 DISASSOC request occurs when the STA the tries to leave the AP*/
#define WLC_E_DISASSOC_IND                12 /**< 802.11 DISASSOC indication occurs when the connected station gets disassociates from SOFTAP,
                                                  also when STA gets diassociated by the AP*/
#define WLC_E_LINK                        16 /**< generic link indication */
#define WLC_E_PROBREQ_MSG                 44 /**< Indicates probe request received for the SOFTAP started*/
#define WLC_E_PSK_SUP                     46 /**< WPA Handshake fail during association*/
#define WLC_E_ACTION_FRAME                59 /**< Indicates Action frame Rx */
#define WLC_E_ACTION_FRAME_COMPLETE       60 /**< Indicates Action frame Tx complete */
#define WLC_E_ESCAN_RESULT                69 /**< escan result event occurs when we scan for the networks */

/* List of status codes - Applicable for any event type */
#define WLC_E_STATUS_SUCCESS        0   /**< operation was successful */
#define WLC_E_STATUS_FAIL           1   /**< operation failed */
#define WLC_E_STATUS_TIMEOUT        2   /**< operation timed out */
#define WLC_E_STATUS_NO_NETWORKS    3   /**< failed due to no matching network found */
#define WLC_E_STATUS_ABORT          4   /**< operation was aborted */
#define WLC_E_STATUS_NO_ACK         5   /**< protocol failure: packet not ack'd */
#define WLC_E_STATUS_UNSOLICITED    6   /**< AUTH or ASSOC packet was unsolicited */
#define WLC_E_STATUS_ATTEMPT        7   /**< attempt to assoc to an auto auth configuration */
#define WLC_E_STATUS_PARTIAL        8   /**< scan results are incomplete */
#define WLC_E_STATUS_NEWSCAN        9   /**< scan aborted by another scan */
#define WLC_E_STATUS_NEWASSOC       10  /**< scan aborted due to assoc in progress */
#define WLC_E_STATUS_11HQUIET       11  /**< 802.11h quiet period started */
#define WLC_E_STATUS_SUPPRESS       12  /**< user disabled scanning (WLC_SET_SCANSUPPRESS) */
#define WLC_E_STATUS_NOCHANS        13  /**< no allowable channels to scan */
#define WLC_E_STATUS_CCXFASTRM      14  /**< scan aborted due to CCX fast roam */
#define WLC_E_STATUS_CS_ABORT       15  /**< abort channel select */
#define WLC_E_STATUS_ERROR          16  /**< request failed due to error */
#define WLC_E_STATUS_INVALID        0xff /**< Invalid status code to init variables. */

#define WLC_SUP_STATUS_OFFSET      (256) /**< Status offset added to the status codes to match the values from firmware. */

#endif // IOCTL_H
