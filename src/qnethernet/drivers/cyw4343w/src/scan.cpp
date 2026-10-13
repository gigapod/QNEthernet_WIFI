// Only compile when the CYW4343W driver is selected
#include "qnethernet/driver_select.h"
#if defined(QNETHERNET_INTERNAL_DRIVER_CYW4343W)

// Scan.cpp

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "cyw43_T4_SDIO.h"
#include "misc_defs.h"
#include "SdioRegs.h"
#include "ioctl_T4.h"
#include "event.h"
#include "scan.h"

extern W4343WCard WIFIcard;
extern sdpcm_header_t iehh;
Scan scn;
extern uint8_t scan_count;
//simple_scan_result_t filtered_scan_results[MAX_SCAN_ENTRIES] = {};

// =====================================================================
// Network scan parameters
// =====================================================================
SCAN_PARAMS scan_params = {
  .version=1,
  .action=1,
  .sync_id=1,
  .ssidlen=0,
  .ssid={0},
  .bssid={0xff,0xff,0xff,0xff,0xff,0xff},
  .bss_type=2,
  .scan_type=SCANTYPE_PASSIVE,
  .nprobes=-1,
  .active_time=-1,
  .passive_time=-1,
  .home_time=-1,
  .nchans=14,
  .nssids=0,
  .chans{{0,0}},
  .ssids{{0}}
};

// =====================================================================
// Handler for scan events
// =====================================================================
int Scan::scan_event_handler(EVENT_INFO *eip) {
  ESCAN_RESULT *erp=(ESCAN_RESULT *)eip->data;
  int ret = (eip->chan==SDPCM_CHAN_EVT) &&
            (eip->event_type==WLC_E_ESCAN_RESULT);
  if(ret) { // Is this a scan event ?
    // Check for scan completion.
    if(erp->eventh.status == 0) {
      return(-1); // Return completion.
    } else {
#if SHOW_HIDDEN == false  // Skip displaying hidden sites if false.
      if(erp->info.SSID_len != 0) {
#endif
      // Check for invalid length info struct data length.
      if((erp->info.ie_offset + erp->info.ie_length) > erp->info.length) {
        erp->eventh.status = (uint32_t)-1; // set invalid
        printf("Scan Failed\n");
        return erp->eventh.status; // Return invalid status.
      }
      // Entry 0 is unused, so there's room for MAX_SCAN_ENTRIES - 1
      if(scan_count < MAX_SCAN_ENTRIES - 1) {
        scan_count++; // Start scan count at 1. 
        parseScanResult(erp); // Parse Information Element RSN entries.
      }
#if SHOW_HIDDEN == false
	  }
#endif
    }
  }
  return(ret);
}

// =====================================================================
// Start a network scan
// =====================================================================
int Scan::scan_start(void) {
    int ret;
    
  scan_count = 0; // Zero out scan entry counter.
  evt.ioctl_enable_evts((EVT_STR *)escan_evts); // Enable scan events.
  ret = WIFIcard.ioctl_wr_int32(WLC_SET_SCAN_CHANNEL_TIME, 10, SCAN_CHAN_TIME) > 0 &&
        WIFIcard.ioctl_set_uint32("pm2_sleep_ret", IOCTL_WAIT, 0xc8) > 0 &&
        WIFIcard.ioctl_set_uint32("bcn_li_bcn", IOCTL_WAIT, 1) > 0 &&
        WIFIcard.ioctl_set_uint32("bcn_li_dtim", IOCTL_WAIT, 1) > 0 &&
        WIFIcard.ioctl_set_uint32("assoc_listen", IOCTL_WAIT, 0x0a) > 0 &&
        WIFIcard.ioctl_wr_int32(WLC_SET_BAND, IOCTL_WAIT, WIFI_BAND_ANY) > 0 &&
        WIFIcard.ioctl_wr_int32(WLC_UP, IOCTL_WAIT, 0) > 0 &&
        WIFIcard.ioctl_set_data("escan", IOCTL_WAIT, &scan_params, sizeof(scan_params)) > 0;
        WIFIcard.ioctl_err_display(ret);
  return(ret);
}

// =====================================================================
// Remove duplicate scan entries and return new scan entry count.
// =====================================================================
uint8_t Scan::getScanCount(void) {
  return removeDuplicates();	
}

// =====================================================================
// Return pointer to filtered scan results.
// =====================================================================
simple_scan_result_t *Scan::getFilteredScanResults(void) {
  return filtered_scan_results;
}

// =====================================================================
// Comparison function for qsort
// =====================================================================
int Scan::compareBSSID(const void *a, const void *b) {
  simple_scan_result_t *scnA = (simple_scan_result_t *)a;
  simple_scan_result_t *scnB = (simple_scan_result_t *)b;
  // memcmp returns (< 0) if a < b, returns (0) if a == b, returns (> 0) if a > b.
  return memcmp(scnA->bssid, scnB->bssid, MACLEN);
}

// =====================================================================
// Remove duplicate scan entries and return new scan entry count.
// =====================================================================
int Scan::removeDuplicates(void) {
  if(scan_count == 0) return 0;
    // Get pointer to scan results array.
    simple_scan_result_t *scnrslt = scn.getScanResults();    
  // Valid filtered_scan_results[] entry counter index.
  uint8_t index = 0;
  // We need to sort the entries first before checking for and removing
  // duplicate entries into the user result array.
  // First entry (0) is empty so add 1 to scnrslt to start at entry (1).
  qsort(scnrslt+1, scan_count, sizeof(simple_scan_result_t), scn.compareBSSID);
  // Remove duplicate entries.
  for(int i=1; i <= scan_count; i++) {
    if(i < scan_count && MAC_CMP(scnrslt[i].bssid, scnrslt[i+1].bssid)) {
      continue;
	} else {
      filtered_scan_results[index++] = scnrslt[i];   
	}
  }	
  return index; // Returns the new entry count (index).
}

// =====================================================================
// Parse out security type in IE RSN section and populate scan result array.
// =====================================================================
uint32_t Scan::parseScanResult(ESCAN_RESULT *evsrp) {
  uint32_t security_mask = SEC_OPEN;

  // Start of the Information Elements loop
  const uint8_t *ie_start = (uint8_t *)&evsrp->info + evsrp->info.ie_offset;
  // Total length from start of ie_offset address.
  uint32_t ie_len = evsrp->info.ie_length;
  uint32_t parsed = 0;
  bool has_rsn = false; // True if RSN IE_ID == 48 (0x30) found.
  bool has_wpa = false; // True if AKM suite type == 0x00,0x50,0xF2,0x01 found.
  // Work through IE entry looking for valid AKM suites.
  while(parsed < ie_len) {
    uint8_t ie_id = ie_start[parsed]; // Start at RSN EI_ID.
    uint8_t ie_element_len = ie_start[parsed + 1]; // Get length of this IE.
    // Ensure we do not overflow malicious/malformed frames
    if(parsed + 2 + ie_element_len > ie_len) break;
    const uint8_t *ie_data = &ie_start[parsed + 2]; // Skip over IE length word.
    if(ie_id == DOT11_IE_ID_RSN) { // EI_ID = 48 (0x30)
      has_rsn = true; // Valid IE_ID found.
      // Optional: Parse deep into RSN AKM suites to check for WPA3 (SAE)
      // If AKM suite count > 0, check suite type (OUI 00-0F-AC, Type 8 = SAE)
      if(ie_element_len >= 18) {
        // Quick look ahead for WPA3 SAE suite selector
         for(int i = 0; i < ie_element_len - 4; i++) {
            if(memcmp(&ie_data[i], "\x00\x0F\xAC\x08", 4) == 0) {
              security_mask |= SEC_WPA3; // -----^^ type.
            }
         }
      }
    } else if(ie_id == DOT11_IE_ID_VENDOR_SPECIFIC) {
        // Check for WPA1 Vendor OUI: 00:50:F2 with Type 1
        if(ie_element_len >= 4 && memcmp(ie_data, "\x00\x50\xF2\x01", 4) == 0) {
          has_wpa = true; // -----------------------------------^^ type.
        }
      }
      parsed += 2 + ie_element_len; // Move to next AKM suite.
  }
  // Custom security bitfield returns. Defined in event.h file.
  //#define SEC_OPEN   0        // 0
  //#define SEC_WEP    (1 << 0) // 1
  //#define SEC_WPA    (1 << 1) // 2
  //#define SEC_WPA2   (1 << 2) // 4
  //#define SEC_WPA3   (1 << 3) // 8

  // Evaluate flags using 802.11 rules combined with your raw IE checks.
  if(evsrp->info.capability & DOT11_CAP_PRIVACY) { // Found in BSS info struct.
    if(!has_rsn && !has_wpa) {
      security_mask |= SEC_WEP;
    }
    if(has_wpa) {
      security_mask |= SEC_WPA;
    }
    if(has_rsn && !(security_mask & SEC_WPA3)) {
      security_mask |= SEC_WPA2;
    }
  }
  // Convert the security type of the scan result to the corresponding
  // security string (See Above defs).
  const char* security_type_string = "UNKNOWN";
  switch (security_mask) {
    case SEC_OPEN:
      security_type_string = "OPEN"; //SECURITY_OPEN;
      break;
    case SEC_WEP:
      security_type_string = "WEP"; //SECURITY_WEP_PSK;
      break;
    case SEC_WPA:
      security_type_string = "WPA"; //SECURITY_WPA_TKIP_PSK;
      break;
    case SEC_WPA2:
      security_type_string = "WPA2"; //SECURITY_WPA2_MIXED_PSK;
      break;
    case SEC_WPA+SEC_WPA2:
      security_type_string = "AUTO"; //SECURITY_AUTO; //SECURITY_WPA3_WPA2_PSK;
      break;
    case SEC_WPA3:
      security_type_string = "AUTO"; //SECURITY_AUTO; // SECURITY_WPA3_SAE;
      break;
  }
  // Fill in scan results array entry with the current scan entry index (scan_count).
  MAC_CPY(scan_results[scan_count].bssid, evsrp->info.BSSID.octet);
  // The SSID isn't necessarily NUL-terminated
  memset(scan_results[scan_count].ssid, 0, sizeof(scan_results[scan_count].ssid));
  if(evsrp->info.SSID_len == 0) {
    strcpy((char *)scan_results[scan_count].ssid, "[HIDDEN]");
  } else {
    memcpy(scan_results[scan_count].ssid, evsrp->info.SSID,
           evsrp->info.SSID_len > sizeof(scan_results[scan_count].ssid)
               ? sizeof(scan_results[scan_count].ssid)
               : evsrp->info.SSID_len);
  }
  scan_results[scan_count].signal_strength = evsrp->info.RSSI;
  strcpy((char *)scan_results[scan_count].security, (const char *)security_type_string);
  scan_results[scan_count].channel = evsrp->info.chanspec&0xff;
  scan_results[scan_count].security_mask = security_mask;   
  return security_mask;
}

// =====================================================================
// Get scan results buffer.
// =====================================================================
simple_scan_result_t *Scan::getScanResults(void) {
  return scan_results;
}

#endif  // QNETHERNET_INTERNAL_DRIVER_CYW4343W
