// Only compile when the CYW4343W driver is selected
#include "qnethernet/driver_select.h"
#if defined(QNETHERNET_INTERNAL_DRIVER_CYW4343W)

/**
 * Copyright (c) 2011-2021 Bill Greiman
 * This file is part of the SdFat library for SD memory cards.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
*/

//Broadcom full MAC driver source used for reference:
//https://github.com/torvalds/linux/releases/tag/v5.6-rc4

#include <Arduino.h>
#include "cyw43_T4_SDIO.h"
#include "WiFiCardInfo.h"
#include "SdioCard.h"
#include "SdioRegs.h"
#include "ioctl_T4.h"

//Remove this define to use built-in internal LPO in 4343W
//#define USE_EXTERNAL_LPO
void cwydump(unsigned char *memory, unsigned int len);
#if WIFI_DEVICE == CYW4343W
//==============================================================================
// CYW4343W firmware files.
//==============================================================================
///////////////
//Firmware file 
///////////////
#include "../firmware/CYW4343W_SDIO_bin.c"
////////////
//NVRAM file
/////////////
#include "../firmware/wifi_nvram_1dx.h"
//////////
//CLM file
//////////
#include "../firmware/CYW4343W_clm_blob.c"
//==============================================================================
#else
//==============================================================================
// CYW43439 Firmware files.
//==============================================================================
///////////////
//Firmware file 
///////////////
#include "../firmware/FW_43439-bin.c"
//////////
//CLM file
//////////
#include "../firmware/CLM_43439-bin.c"
////////////
//NVRAM file
/////////////
#include "../firmware/wifi_43439_NVRAM.h"
//==============================================================================
#endif
//==============================================================================
// MAC address stuff.
//==============================================================================
uint8_t my_mac[MACLEN];    // Internal MAC usage.
//==============================================================================


//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
W4343WCard *W4343WCard::s_pSdioCards[2] = {nullptr, nullptr}; // uSDHC1, uSDHC2.
volatile bool W4343WCard::dataISRReceived = false; // Data received ISR flag.
volatile bool W4343WCard::fUseSDIO2 = false; // true = use SDIO2, false = use SDIO1.
static  int display_mode = 0; // Debug Mode.

//==============================================================================
// Error function and macro.
//==============================================================================
#define wifiError(code) setWifiErrorCode(code, __LINE__)
inline bool W4343WCard::setWifiErrorCode(uint8_t code, uint32_t line) {
  m_errorCode = code;
  m_errorLine = line;
  return false;
}

//==============================================================================
// ISR
//==============================================================================
void W4343WCard::wifiISR() {
  USDHC1_INT_SIGNAL_EN = 0;
  s_pSdioCards[0]->m_irqstat = USDHC1_INT_STATUS;
  USDHC1_INT_STATUS = s_pSdioCards[0]->m_irqstat;
  USDHC1_MIX_CTRL &= ~(SDHC_MIX_CTRL_AC23EN | SDHC_MIX_CTRL_DMAEN);
  s_pSdioCards[0]->m_dmaBusy = false;
}

void W4343WCard::wifiISR2() {
  USDHC2_INT_SIGNAL_EN = 0;
  s_pSdioCards[1]->m_irqstat = USDHC2_INT_STATUS;
  USDHC2_INT_STATUS = s_pSdioCards[1]->m_irqstat;
  USDHC2_MIX_CTRL &= ~(SDHC_MIX_CTRL_AC23EN | SDHC_MIX_CTRL_DMAEN);
  s_pSdioCards[1]->m_dmaBusy = false;
}
//==============================================================================

//==============================================================================
// GPIO and clock functions.
//==============================================================================
void W4343WCard::gpioMux(uint8_t mode) {
  if (fUseSDIO2 == false) {
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_05 = mode;  // DAT3
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_04 = mode;  // DAT2
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_00 = mode;  // CMD
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_01 = mode;  // CLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_02 = mode;  // DAT0
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_03 = mode;  // DAT1
  } else {
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_07 = mode; // USDHC2_DATA3
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_06 = mode; // USDHC2_DATA2
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_08 = mode; // USDHC2_CMD
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09 = mode; // USDHC2_CLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_04 = mode; // USDHC2_DATA0
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_05 = mode; // USDHC2_DATA1
  }
}

//==============================================================================
// Switches DAT1 to SDIO mode
//==============================================================================
void W4343WCard::makeSDIO_DAT1()
{
  if (m_wlIrqPin != -1) {
    fUseSDIO2 == false ? IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_03 = 0x00 : IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_05 = 0x06;
  }
}

//==============================================================================
//Switches DAT1 to GPIO mode to enable interrupt when data ready
//==============================================================================
void W4343WCard::makeGPIO_DAT1()
{
  if (m_wlIrqPin != -1) {
    fUseSDIO2 == false ? IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_03 = 0x05 : IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_05 = 0x05;
  }
}
//==============================================================================

//==============================================================================
// add speed strength args?
//==============================================================================
void W4343WCard::enableSDIO(bool enable) {
  const uint32_t CLOCK_MASK = IOMUXC_SW_PAD_CTL_PAD_PKE |
                              IOMUXC_SW_PAD_CTL_PAD_DSE(7) |
                              IOMUXC_SW_PAD_CTL_PAD_SPEED(2);

  const uint32_t DATA_MASK = CLOCK_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE |
                             IOMUXC_SW_PAD_CTL_PAD_PUS(1);
  if (enable == true) {
    gpioMux(fUseSDIO2? 6 : 0);  // Default function. SDIO1 ALT0, SDIO2 is on ALT6
    if (fUseSDIO2 == false) {
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_04 = DATA_MASK;   // DAT2
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_05 = DATA_MASK;   // DAT3
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_00 = DATA_MASK;   // CMD
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_01 = CLOCK_MASK;  // CLK
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_02 = DATA_MASK;   // DAT0
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_03 = DATA_MASK;   // DAT1
    } else {      
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_07 = DATA_MASK;  // USDHC2_DATA3
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_06 = DATA_MASK;  // USDHC2_DATA2
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_08 = DATA_MASK;  // USDHC2_CMD
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_09 = CLOCK_MASK; // USDHC2_CLK
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_04 = DATA_MASK;  // USDHC2_DATA0
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_05 = DATA_MASK;  // USDHC2_DATA1

      // We need to set select input bits...
      IOMUXC_USDHC2_CLK_SELECT_INPUT = 0x1;
      IOMUXC_USDHC2_CMD_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA0_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA1_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA2_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA3_SELECT_INPUT = 0x01;
    }
  } else {
    gpioMux(5); //GPIO function
  }
}

//==============================================================================
// Init USDHC Clock
//==============================================================================
void W4343WCard::initClock() {
  /* set PDF_528 PLL2PFD0 */
  CCM_ANALOG_PFD_528 |= (1 << 7);
  CCM_ANALOG_PFD_528 &= ~(0x3F << 0);
  CCM_ANALOG_PFD_528 |= ((24) & 0x3F << 0);  // 12 - 35
  CCM_ANALOG_PFD_528 &= ~(1 << 7);

  /* Enable USDHC clock. */
  if (fUseSDIO2 == false) {
    CCM_CCGR6 |= CCM_CCGR6_USDHC1(CCM_CCGR_ON);
    CCM_CSCDR1 &= ~(CCM_CSCDR1_USDHC1_PODF(7));
    CCM_CSCMR1 |= CCM_CSCMR1_USDHC1_CLK_SEL;          // PLL2PFD0
  //  CCM_CSCDR1 |= CCM_CSCDR1_USDHC1_CLK_PODF((7)); / &0x7  WHG
    CCM_CSCDR1 |= CCM_CSCDR1_USDHC1_CLK_PODF((1));
  } else {
    CCM_CCGR6 |= CCM_CCGR6_USDHC2(CCM_CCGR_ON);
    CCM_CSCDR1 &= ~(CCM_CSCDR1_USDHC2_PODF(7));
    CCM_CSCMR1 |= CCM_CSCMR1_USDHC2_CLK_SEL;          // PLL2PFD0
  //  CCM_CSCDR1 |= CCM_CSCDR1_USDHC2_CLK_PODF((7)); / &0x7  WHG
    CCM_CSCDR1 |= CCM_CSCDR1_USDHC2_PODF(1);
  }
}

//==============================================================================
// Return base clock value
//==============================================================================
uint32_t W4343WCard::baseClock() {
  uint32_t divider = ((CCM_CSCDR1 >> 11) & 0x7) + 1;
  return (528000000U * 3)/((CCM_ANALOG_PFD_528 & 0x3F)/6)/divider;
}
//==============================================================================

//==============================================================================
// Static functions
//==============================================================================
////////////////////
// IRW New functions
////////////////////
// Union to handle 8/16/32 bit conversions
typedef union
{
  int32_t  int32;
  uint32_t uint32;
  uint32_t uint24:24;
  uint16_t uint16;
  uint8_t  uint8;
  uint8_t  bytes[4];
} u32Data;

//==============================================================================
// CMD52 Read
//==============================================================================
bool W4343WCard::cardCMD52_read(uint32_t functionNumber, uint32_t registerAddress, uint8_t * response, bool logOutput) {
  makeSDIO_DAT1();
  bool res = cardCMD52(functionNumber, registerAddress, 0, SD_RD, 0, response, logOutput);
  makeGPIO_DAT1();
  return res;
}
//==============================================================================

//==============================================================================
// CMD52 Write
//==============================================================================
bool W4343WCard::cardCMD52_write(uint32_t functionNumber, uint32_t registerAddress, uint8_t data, bool logOutput) {
  makeSDIO_DAT1();
  bool res = cardCMD52(functionNumber, registerAddress, data, SD_WR, 0, NULL, logOutput);
  makeGPIO_DAT1();
  return res;
}
//==============================================================================

//==============================================================================
// CMD52 Read/Write
//==============================================================================
bool W4343WCard::cardCMD52(uint32_t functionNumber, uint32_t registerAddress, uint8_t data, bool write, bool readAfterWriteFlag, uint8_t * buffer, bool logOutput) {
  uint32_t arg = (functionNumber << 28);          // Function number in bits 28–30

  arg |= ((registerAddress & 0x1FFFF) << 9);      // Register address, 17 bits in 9-26
  if (write == true) {
    arg |= (1 << 31) | ((uint32_t)readAfterWriteFlag << 27); // Set write bit 31 if writing, raw flag bit 27
    arg |= (uint8_t)data;                                    // Data to write in bits 0–8
  } 
  // Issue the command
  if (!cardCommand(CMD52_XFERTYP, arg)) {
    printf("CMD52 failed\n");
    return false;
  }
//  if (logOutput) printf("CMD52 %s, 0x%08X, response: 0x%02X\n", write == true ? "write" : "read", registerAddress, m_psdhc->CMD_RSP0 & 0xFF);
  // If reading, extract the response byte
  if ((write == false) && buffer) {
    *buffer = m_psdhc->CMD_RSP0 & 0xFF;  //Read the response register, the read byte is in bits 0–7
  }
  return true;
}
//==============================================================================

//==============================================================================
// Set block count transfer size
//==============================================================================
void W4343WCard::setBlockCountSize(bool blockMode, uint32_t functionNumber, uint32_t size) {
  // Set up block count and block size
  if (blockMode == true) {
    switch (functionNumber) {
/* 0 */ case SD_FUNC_BUS: m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKCNT(size) | SDHC_BLKATTR_BLKSIZE(64); 
                        break;
/* 1 */ case SD_FUNC_BAK: m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKCNT(size) | SDHC_BLKATTR_BLKSIZE(SD_BAK_BLK_BYTES);
                        break;
/* 2 */ case SD_FUNC_RAD: m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKCNT(size) | SDHC_BLKATTR_BLKSIZE(SD_RAD_BLK_BYTES);
                        break;
    }
  } else {
    m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKCNT(1) | SDHC_BLKATTR_BLKSIZE(size); // <------- FAILING ------------->
  }
}
//==============================================================================

//==============================================================================
// CMD53 Read
//==============================================================================
bool W4343WCard::cardCMD53_read(uint32_t functionNumber, uint32_t registerAddress, uint8_t * buffer, uint32_t size, bool logOutput) {
  makeSDIO_DAT1();
  bool res = cardCMD53(functionNumber, registerAddress, buffer, size, false, logOutput);
  makeGPIO_DAT1();
  return res;
}
//==============================================================================

//==============================================================================

//==============================================================================
// CMD53 Write
//==============================================================================
bool W4343WCard::cardCMD53_write(uint32_t functionNumber, uint32_t registerAddress, uint8_t * buffer, uint32_t size, bool logOutput) {
  makeSDIO_DAT1();
  bool res = cardCMD53(functionNumber, registerAddress, buffer, size, true, logOutput);
  makeGPIO_DAT1();
  return res;
}
//==============================================================================

//==============================================================================
// CMD53 Read/Write
//==============================================================================
bool W4343WCard::cardCMD53(uint32_t functionNumber, uint32_t registerAddress, uint8_t * buffer, uint32_t size, bool write, bool logOutput) {
  bool return_value = false;
  bool blockMode = false;
  
  if(buffer == nullptr) {
	if (logOutput) printf(" buffer == nullptr\n");
	return false;
  }
  uint8_t opCode = 1;
  // CMD53 argument format:
  // [31]    - Read/Write flag 
  // [30:28] - Function number (0-7)
  // [27]    - Block mode (0 for byte mode, 1 for block mode)
  // [26]    - OP Code (0 for R/W to fixed address, 1 for R/W to incrementing address)
  // [25:9]  - Address (17-bit address)
  // [8:0]   - Byte count (for byte mode) or block count (for block mode)
  uint32_t arg = (write << 31) | (functionNumber << 28) | (blockMode << 27) | 
                 (opCode << 26) | ((registerAddress & 0x1FFFF) << 9) | (size & 0x1FF);
  // Set up the basic transfer type for CMD53
  uint32_t xfertyp = SDHC_XFERTYP_CMDINX(53) | SDHC_XFERTYP_DPSEL | SDHC_XFERTYP_BCEN | CMD_RESP_R5; 
  // Add read direction
  if(write == false) {
    xfertyp |=  SDHC_MIX_CTRL_DTDSEL;
  }
  setBlockCountSize(blockMode, functionNumber, size);
  // Debug info
  // char cmdInfo[90];
  // sprintf(cmdInfo, "CMD: %ld %s  REG: 0x%08lX ARG: 0x%08lX", (xfertyp >> 24) & 0x3F, write ? "WRTE" : "READ", registerAddress, arg);
  // if(logOutput) printf(SER_YELLOW "\n%s\n" SER_RESET, cmdInfo);
  // Check for command inhibit
  if(waitTimeout(&W4343WCard::isBusyCommandInhibit)) {
    // printf(SER_RED "%s isBusyCommandInhibit\n" SER_RESET, cmdInfo);
    return false;  // Caller will set errorCode.
  }
  // Set the argument
  m_psdhc->CMD_ARG = arg;
  // Reset MIX_CTRL
  m_psdhc->MIX_CTRL &= ~SDHC_MIX_CTRL_MASK;
  // Set MIX_CTRL for data transfer
  m_psdhc->MIX_CTRL |= xfertyp & SDHC_MIX_CTRL_MASK;
  // Remove flags from transfer type
  xfertyp &= ~SDHC_MIX_CTRL_MASK;
  // Execute command
  m_psdhc->CMD_XFR_TYP = xfertyp;
  // Set up pointer and loop counter to transfer data
  uint32_t * ptr = (uint32_t *)buffer;
  uint16_t wordCount = (size / 4) + (size % 4 > 0 ? 1 : 0);
  // Handle write data transfer
  if(write == true) {
    m_transferActive = true;
    if((m_psdhc->PRES_STATE & SDHC_PRSSTAT_WTA) == false) {
      m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_SABGREQ;
      m_psdhc->PROT_CTRL |= SDHC_PROCTL_CREQ;
    }
    //Set Stop At Block Gap Request
    m_psdhc->PROT_CTRL |= SDHC_PROCTL_SABGREQ;
    uint32_t index = 0;
    while(index < wordCount) {
      // Wait for the buffer write enable flag
      while (0 == (m_psdhc->PRES_STATE & SDHC_PRSSTAT_BWEN)) {;}
      uint32_t writeCount = (wordCount - index) > FIFO_WML ? FIFO_WML : wordCount - index;
      for(uint32_t i = 0; i < writeCount; i++) {
        m_psdhc->DATA_BUFF_ACC_PORT = ptr[index];
        if(logOutput) printf("Write: 0x%08lX\n", ptr[index]);
        index++;
      }
    }
    m_transferActive = false;
    if(logOutput) printf("Write done\n");
  }
  // Handle read data transfer
  if(write == false) {
    // Read data from the SDIO card
    uint32_t index = 0;
    while(index < wordCount) {
      // Wait for the buffer read enable flag
      while (0 == (m_psdhc->PRES_STATE & SDHC_PRSSTAT_BREN)) {;}
        uint32_t readCount = (wordCount - index) > FIFO_WML ? FIFO_WML : wordCount - index;
        for(uint32_t i = 0; i < readCount; i++) {
          ptr[index] = m_psdhc->DATA_BUFF_ACC_PORT;
          if(logOutput) printf("Read: 0x%08lX\n", ptr[index]);
          index++;
        }
      }
      //TODO test code - dummy read as occasionally more data is returned than expected Likely occurs when asking
      //TODO for more bytes than a register wants to give up. Safe to keep for now, during development.
      while(m_psdhc->PRES_STATE & SDHC_PRSSTAT_BREN) {
        printf(SER_ERROR "Dummy: 0x%08lX\n" SER_RESET, m_psdhc->DATA_BUFF_ACC_PORT);
      }
      if(logOutput) printf("Read done\n");
  }
  // Wait for transfer completion
  if(waitTimeout(&W4343WCard::isBusyTransferComplete)) {
    // printf(SER_RED "%s isBusyTransferComplete\n" SER_RESET, cmdInfo);
    return false;  // Caller will set errorCode.
  } 
  // Wait for command completion
  if(waitTimeout(&W4343WCard::isBusyCommandComplete)) {
    // printf(SER_RED "%s isBusyCommandComplete\n" SER_RESET, cmdInfo);
    return wifiError(WIFI_CARD_ERROR_READ_TIMEOUT);
  }  
  // Check for errors
  m_irqstat = m_psdhc->INT_STATUS;
  m_psdhc->INT_STATUS = m_irqstat;
  return_value = (m_irqstat & SDHC_IRQSTAT_CC) && !(m_irqstat & SDHC_IRQSTAT_CMD_ERROR);
  if(logOutput) {
    printResponse(return_value);
    printf(!return_value ? SER_RED "cmd failed\n" SER_RESET : "cmd complete\n");
  }
  return return_value;
}
//==============================================================================

/////////////////////////////
// WLAN interaction functions
/////////////////////////////
static uint16_t ioctl_reqid=0;
#define DISP_BLOCKLEN       32

// Event handling
uint8_t resp[256] = {0}, eth[7]={0};
IOCTL_MSG_T4 ioctl_txmsg, ioctl_rxmsg; // Used in event.cpp

//==============================================================================
// Send Transfer type and arg to SDIO device.
//==============================================================================
bool W4343WCard::cardCommand(uint32_t xfertyp, uint32_t arg) {
  // Debug string
//  char cmdInfo[90];
//  sprintf(cmdInfo, "CMD: %ld ARG: 0x%08lX", (xfertyp >> 24) & 0x3F,  arg);
//  printf(SER_YELLOW "\n%s\n" SER_RESET, cmdInfo);

  if (waitTimeout(&W4343WCard::isBusyCommandInhibit)) {
    printf("cardCommand(%lx, %lx) error isBusyCommandInhibit\n", xfertyp, arg);
    return false;  // Caller will set errorCode.
  }
  m_psdhc->CMD_ARG = arg;
  // Set MIX_CTRL if data transfer.
  if (xfertyp & SDHC_XFERTYP_DPSEL) {
    m_psdhc->MIX_CTRL &= ~SDHC_MIX_CTRL_MASK;
    m_psdhc->MIX_CTRL |= xfertyp & SDHC_MIX_CTRL_MASK; //Enables DMA based on SDHC_MIX_CTRL_DMAEN set in xfertyp (DATA_READ_DMA, DATA_WRITE_DMA)
  }
  xfertyp &= ~SDHC_MIX_CTRL_MASK;
  m_psdhc->CMD_XFR_TYP = xfertyp;  // Execute command

  if (waitTimeout(&W4343WCard::isBusyCommandComplete)) {
    printf("cardCommand(%lx, %lx) error isBusyCommandComplete\n", xfertyp, arg);
    return false;  // Caller will set errorCode.
  }
  m_irqstat = m_psdhc->INT_STATUS;
  m_psdhc->INT_STATUS = m_irqstat;

  bool return_value = (m_irqstat & SDHC_IRQSTAT_CC) && !(m_irqstat & SDHC_IRQSTAT_CMD_ERROR);
//  printResponse(return_value);
  return return_value;      
}
//==============================================================================

//==============================================================================
// 
//==============================================================================

void W4343WCard::enableDmaIrs() {
  m_dmaBusy = true;
  m_irqstat = 0;
}
//==============================================================================

//==============================================================================
// Init SDIO uSDHC1 or uSDHC2
//==============================================================================
void W4343WCard::initSDHC() {
  // initialize Hardware registers and this pointer
  if(fUseSDIO2 == false) {
    s_pSdioCards[0] = this; 
    m_psdhc = (IMXRT_USDHC_t*)IMXRT_USDHC1_ADDRESS;   
  } else {
    s_pSdioCards[1] = this; 
    m_psdhc = (IMXRT_USDHC_t*)IMXRT_USDHC2_ADDRESS; 
  }
  initClock(); // Initialize uSDHC clock.
  // Disable GPIO
  enableSDIO(false);
  m_psdhc->MIX_CTRL |= 0x80000000;
  // Reset SDHC. Use default Water Mark Level of 16.
  m_psdhc->SYS_CTRL |= SDHC_SYSCTL_RSTA | SDHC_SYSCTL_SDCLKFS(0x80);
  while(m_psdhc->SYS_CTRL & SDHC_SYSCTL_RSTA) {;} // Wait till done.
  // Set initial SCK rate.
  setWiFiclk(WIFI_MAX_INIT_RATE_KHZ);
  enableSDIO(true); // Start SDIO.
  // Enable desired IRQSTAT bits.
  m_psdhc->INT_STATUS_EN = SDHC_IRQSTATEN_MASK;
  // Setup interupt vectorswith ISR routine.
  if(fUseSDIO2 == false) {
    attachInterruptVector(IRQ_SDHC1, wifiISR);
    NVIC_SET_PRIORITY(IRQ_SDHC1, 6*16);
    NVIC_ENABLE_IRQ(IRQ_SDHC1);
  } else {
    attachInterruptVector(IRQ_SDHC2, wifiISR2);
    NVIC_SET_PRIORITY(IRQ_SDHC2, 6*16);
    NVIC_ENABLE_IRQ(IRQ_SDHC2);    
  }
  // Send 80 clocks to card.
  m_psdhc->SYS_CTRL |= SDHC_SYSCTL_INITA;
  while(m_psdhc->SYS_CTRL & SDHC_SYSCTL_INITA) {;}
}
//==============================================================================

//==============================================================================
// Check command finished.
//==============================================================================
bool W4343WCard::isBusyCommandComplete() {
  return !(m_psdhc->INT_STATUS & (SDHC_IRQSTAT_CC | SDHC_IRQSTAT_CMD_ERROR));
}
//==============================================================================

//==============================================================================
// Check command inhibited.
//==============================================================================
bool W4343WCard::isBusyCommandInhibit() {
  return m_psdhc->PRES_STATE & SDHC_PRSSTAT_CIHB;
}
//==============================================================================

//==============================================================================
// Check if transfer is finished.
//==============================================================================
bool W4343WCard::isBusyTransferComplete() {
  return !(m_psdhc->INT_STATUS & (SDHC_IRQSTAT_TC | SDHC_IRQSTAT_ERROR));
}
//==============================================================================

//==============================================================================
//Set WIFI clock rate (25mhz or 33mhz).
//==============================================================================
void W4343WCard::setWiFiclk(uint32_t kHzMax) {
  const uint32_t DVS_LIMIT = 0X10;
  const uint32_t WIFICLKFS_LIMIT = 0X100;
  uint32_t dvs = 1;
  uint32_t wificlkfs = 1;
  uint32_t maxSdclk = 1000*kHzMax;
  uint32_t baseClk = baseClock();

  while((baseClk/(wificlkfs*DVS_LIMIT) > maxSdclk) && (wificlkfs < WIFICLKFS_LIMIT)) {
    wificlkfs <<= 1;
  }
  while((baseClk/(wificlkfs*dvs) > maxSdclk) && (dvs < DVS_LIMIT)) {
    dvs++;
  }
  m_WiFiClkKhz = baseClk/(1000*wificlkfs*dvs);
  wificlkfs >>= 1;
  dvs--;
  // Change dividers.
  uint32_t sysctl = m_psdhc->SYS_CTRL & ~(SDHC_SYSCTL_DTOCV_MASK
                      | SDHC_SYSCTL_DVS_MASK | SDHC_SYSCTL_SDCLKFS_MASK);
  m_psdhc->SYS_CTRL = sysctl | SDHC_SYSCTL_DTOCV(0x0E) | SDHC_SYSCTL_DVS(dvs)
                      | SDHC_SYSCTL_SDCLKFS(wificlkfs);
  // Wait until the SDHC clock is stable.
  while(!(m_psdhc->PRES_STATE & SDHC_PRSSTAT_SDSTB)) {
  }
}
//==============================================================================

//==============================================================================
// Return true if timeout occurs.
//==============================================================================
bool W4343WCard::waitTimeout(pcheckfcn fcn) {
  uint32_t m = micros();
  while ((this->*fcn)()) {
    if ((micros() - m) > BUSY_TIMEOUT_MICROS) {
      return true;
    }
  }
  return false;  // Caller will set errorCode.
}
//==============================================================================

//==============================================================================
// Enable WIFI card SDIO function.
//==============================================================================
bool W4343WCard::SDIOEnableFunction(uint8_t functionEnable) {
  uint8_t readResponse;

  //Read existing register
  cardCMD52_read(SD_FUNC_BUS, BUS_IOEN_REG, &readResponse);
  //Set function enable
  readResponse |= functionEnable;
  //Write back register
  cardCMD52_write(SD_FUNC_BUS, BUS_IOEN_REG, readResponse);
  //Verify
  for(uint8_t i = 0; i < 100; i++) {
    cardCMD52_read(SD_FUNC_BUS, BUS_IOEN_REG, &readResponse);
    if(readResponse & functionEnable) {
    // printf(SER_TRACE "SDIO function mask 0x%02X enabled\n" SER_RESET, functionEnable);
      return true;
    } 
    delay(1);
  }
  // Failed: print error.
  printf(SER_ERROR "SDIO function mask 0x%02X not enabled\n" SER_RESET, functionEnable);
  return false;
}
//==============================================================================

//==============================================================================
// Disable WIFI card SDIO function.
//==============================================================================
bool W4343WCard::SDIODisableFunction(uint8_t functionEnable) { // Need to implement!!
  uint8_t readResponse;

  //Read existing register
  cardCMD52_read(SD_FUNC_BUS, BUS_IOEN_REG, &readResponse);
  //Set function disable
  readResponse &= ~functionEnable;
  //Write back register
  cardCMD52_write(SD_FUNC_BUS, BUS_IOEN_REG, readResponse);
  //TODO validate written?
  printf(SER_TRACE "SDIO function mask 0x%02X disabled\n" SER_RESET, functionEnable);
  return true;
}
//==============================================================================

//==============================================================================
//bcmsdh.c, line 95, brcmf_sdiod_intr_register
//==============================================================================
bool W4343WCard::configureOOBInterrupt() {
  uint8_t readResponse;
#if INIT_DEBUG_MODE == true
  printf(SER_TRACE "Configuring WL_IRQ OOB\n" SER_RESET);
#endif
  // Must configure BUS_INTEN_REG to enable irq
  cardCMD52_read(SD_FUNC_BUS, BUS_INTEN_REG, &readResponse);
  readResponse |= SDIO_CCCR_IEN_FUNC0 | SDIO_CCCR_IEN_FUNC1 | SDIO_CCCR_IEN_FUNC2;
  cardCMD52_write(SD_FUNC_BUS, BUS_INTEN_REG, readResponse);
  // Redirect, configure and enable IO for interrupt signal
  readResponse = SDIO_CCCR_BRCM_SEPINT_MASK | SDIO_CCCR_BRCM_SEPINT_OE;
  cardCMD52_write(SD_FUNC_BUS, BUS_SEP_INT_CTL, readResponse);
  //TODO any validation?
  return true;
}
//==============================================================================

//==============================================================================
// WLAN interrpupt handler.
//==============================================================================
void W4343WCard::onWLIRQInterruptHandler() {
  dataISRReceived = true;
  //Yeah yeah, no Serial in ISRs, I know....
#if INIT_DEBUG_MODE == true
  printf(SER_MAGENTA "WL_IRQ OOB Interrupt\n" SER_RESET);
#endif
}
//==============================================================================

//==============================================================================
// Data interrupt handler.
//==============================================================================
void W4343WCard::onDataInterruptHandler() {
  dataISRReceived = true;
  //Yeah yeah, no Serial in ISRs, I know....
#if INIT_DEBUG_MODE == true
  printf(SER_MAGENTA "DAT1 IB Interrupt\n" SER_RESET);
#endif
}
//==============================================================================

//==============================================================================
// Set backplane window, don't set if already set
//==============================================================================
void W4343WCard::setBackplaneWindow(uint32_t addr) {
  static uint32_t prevAddr = 0;
  
  addr &= SB_WIN_MASK;
  if(addr != prevAddr) {
    cardCMD52_write(SD_FUNC_BAK, BAK_WIN_ADDR_REG, addr >> 8);
    cardCMD52_write(SD_FUNC_BAK, BAK_WIN_ADDR_REG + 1, addr >> 16);
    cardCMD52_write(SD_FUNC_BAK, BAK_WIN_ADDR_REG + 2, addr >> 24);
    prevAddr = addr;
#if INIT_DEBUG_MODE == true
    printf(SER_TRACE "Backplane Window set to 0x%08lX\n" SER_RESET, addr);
#endif
  }
}
//==============================================================================

//==============================================================================
// Set backplane window, and return offset within window
//==============================================================================
uint32_t W4343WCard::setBackplaneWindow_retOffset(uint32_t addr) {
  setBackplaneWindow(addr);
  return(addr & SB_ADDR_MASK);
}
//==============================================================================

//==============================================================================
// Read a 32-bit value via the backplane window
//==============================================================================
uint32_t W4343WCard::backplaneWindow_read32(uint32_t addr, uint32_t *valp) {
  u32Data u32d;
  uint32_t n;

  setBackplaneWindow(addr);
  n = cardCMD53_read(SD_FUNC_BAK, addr | SB_32BIT_WIN, u32d.bytes, 4, false);
  *valp = u32d.uint32;
  return n;
}
//==============================================================================

//==============================================================================
// Write a 32-bit value via the backplane window
//==============================================================================
uint32_t W4343WCard::backplaneWindow_write32(uint32_t addr, uint32_t val) {
  u32Data u32d={.uint32=val};

  setBackplaneWindow(addr);
  return cardCMD53_write(SD_FUNC_BAK, addr | SB_32BIT_WIN, u32d.bytes, 4, false);
}

//==============================================================================
// Upload firmware to CYW4343x (Using byte mode instead of block mode).
// TODO: Need to setup block mode transfers.
//==============================================================================
bool W4343WCard::uploadFirmware(size_t firmwareSize, uintptr_t source) {
  uint32_t nBytesSent = 0;
  uint32_t len = 0;
  uint32_t addr;
#if INIT_DEBUG_MODE == true
  printf(SER_CYAN "Uploading firmware data\n" SER_RESET);
#endif
  while(nBytesSent < firmwareSize) {
    addr = setBackplaneWindow_retOffset(nBytesSent);
    len = MIN(64, firmwareSize - nBytesSent);
    //TODO implement block sends, use byte mode send for now
    cardCMD53_write(SD_FUNC_BAK, SB_32BIT_WIN + addr, (uint8_t *)source + nBytesSent, len, false);
    nBytesSent += len;
  }
#if INIT_DEBUG_MODE == true
  printf(SER_CYAN "Uploaded firmware, %ld of %d bytes\n" SER_RESET, nBytesSent, firmwareSize);
#endif
  return true;
}

//==============================================================================
// Upload settings to NVRAM.
//==============================================================================
bool W4343WCard::uploadNVRAM(size_t nvRAMSize, uintptr_t source) {
  uint32_t nBytesSent = 0;
  uint32_t len = 0;
  //I believe the offset value 0xfd54 in Zero code is too co-incidental equal to 0x10000 - nvRAMSize, 
  //so calc this. Offset 4 bytes earlier, to leave room for size calculation.
  uint32_t offset = (0x10000 - nvRAMSize) - 4;

#if INIT_DEBUG_MODE == true
  printf(SER_CYAN "Uploading NVRAM data\n" SER_RESET);
#endif
  setBackplaneWindow(0x078000);
  while(nBytesSent < nvRAMSize) {
      len = MIN(nvRAMSize - nBytesSent, SD_BAK_BLK_BYTES);
      cardCMD53_write(SD_FUNC_BAK, offset + nBytesSent, (uint8_t *)source + nBytesSent, len, false);
      nBytesSent += len;
  }
  //Calculate size, write at end of section (remainder).
  u32Data u32d;
  u32d.uint32 = ((~(nvRAMSize / 4) & 0xffff) << 16) | (nvRAMSize / 4);
  cardCMD53_write(SD_FUNC_BAK, 0x10000 - 4, u32d.bytes, 4, false);
#if INIT_DEBUG_MODE == true
  printf(SER_CYAN "Uploaded NVRAM, %ld of %d bytes\n" SER_RESET, nBytesSent, nvRAMSize);
#endif
  return true;
}
//==============================================================================

//==============================================================================
// Upload CLM to CYW4343x chip.
//==============================================================================
bool W4343WCard::uploadCLM() {
  brcmf_dload_data_le *chunk_buf;
#if WIFI_DEVICE == CYW43439
  int32_t datalen = FIRMWARE_CLM_LEN_9;
#else
  int32_t datalen = FIRMWARE_CLM_LEN_W;
#endif
  uint32_t cumulative_len = 0;
  uint32_t ret = 0;

#if INIT_DEBUG_MODE == true
  #if WIFI_DEVICE == CYW43439
    printf(SER_CYAN "Uploading CLM, size: %d\n" SER_RESET, FIRMWARE_CLM_LEN_9);
  #else
    printf(SER_CYAN "Uploading CLM, size: %d\n" SER_RESET, FIRMWARE_CLM_LEN_W);
  #endif
#endif
  chunk_buf = (brcmf_dload_data_le *)malloc(sizeof(*chunk_buf) + MAX_CHUNK_LEN - 1);
  chunk_buf->dload_type = DL_TYPE_CLM;
  chunk_buf->crc = 0;
  do {
    chunk_buf->flag = (DLOAD_HANDLER_VER << DLOAD_FLAG_VER_SHIFT);
    chunk_buf->len = datalen > MAX_CHUNK_LEN ? MAX_CHUNK_LEN : datalen;
    if(cumulative_len == 0) {
      chunk_buf->flag |= DL_BEGIN; 
    } else if(datalen <= MAX_CHUNK_LEN) {
	  chunk_buf->flag |= DL_END;
	}
#if WIFI_DEVICE == CYW43439
	memcpy(chunk_buf->data, clm_data_9 + cumulative_len, chunk_buf->len);
#else
	memcpy(chunk_buf->data, clm_data_4343W + cumulative_len, chunk_buf->len);
#endif
	ret = ioctl_set_data("clmload", 1500, chunk_buf, sizeof(brcmf_dload_data_le) + chunk_buf->len - 1, false);
	cumulative_len += chunk_buf->len;
	datalen -= chunk_buf->len;
  } while((datalen > 0) && (ret == 1));
#if INIT_DEBUG_MODE == true
  #if WIFI_DEVICE == CYW43439
    printf(SER_CYAN "CLM uploaded %ld/%d result: %ld\n" SER_RESET, cumulative_len, FIRMWARE_CLM_LEN_9, ret);
#else
    printf(SER_CYAN "CLM uploaded %ld/%d result: %ld\n" SER_RESET, cumulative_len, FIRMWARE_CLM_LEN_W, ret);
#endif
#endif
  return ret;
}
//==============================================================================

//==============================================================================
// Get CYW4343x MAC address. (See warning below)
//==============================================================================
int W4343WCard::getMACAddress(uint8_t * mac) {
  uint32_t n = 0;
  // WARNING:
  // Can only read once after power up or reset!!! Silly stuff happens if read again.
  // So it is assigned to a system wide variable "my_mac".
  if(!IS_MAC_NONZERO(my_mac)) // Make sure my_mac is un-assigned.
    n = ioctl_get_data("cur_etheraddr", 0, my_mac, 6, false);
  MAC_CPY(mac, my_mac); // Copy and return my_mac to requested mac.
  return n;
}
//==============================================================================

//==============================================================================
// Get the current firmware version. Return version string.
//==============================================================================
char *W4343WCard::getFirmwareVersion() {
  uint32_t n = ioctl_get_data("ver", 0, resp, sizeof(resp), false);
  printf("%sFirmware %s\n" SER_RESET, (n ? SER_GREEN : SER_RED), (n ? (char *)resp : "not responding"));
  return (char *)resp;
}
//==============================================================================

//==============================================================================
// Get CLM Version. retun CLM version string.
//==============================================================================
char *W4343WCard::getCLMVersion(void) {
#if INIT_DEBUG_MODE == true
  uint32_t n = ioctl_get_data("clmver", 0, resp, sizeof(resp), false);
  printf("%sCLM version %s\n" SER_RESET, (n ? SER_GREEN : SER_RED), (n ? (char *)resp : "not responding"));
#else
  ioctl_get_data("clmver", 0, resp, sizeof(resp), false);
#endif
  return (char *)resp;
}
//==============================================================================

//==============================================================================
// Finish up initialization.
//==============================================================================
void W4343WCard::postInitSettings() {
  ioctl_set_uint32("bus:txglom", 20, 0); // tx glomming off
//  ioctl_set_uint32("bus:rxglom", 20, 0); // rx glomming off
  ioctl_set_uint32("apsta", 20, 1); // apsta on
  // Upload CLM.
  uploadCLM();
#if INIT_DEBUG_MODE == true
  getCLMVersion();
#endif
}
//==============================================================================

//==============================================================================
// Class W4343WCard begin function.
//==============================================================================
bool W4343WCard::begin(bool SDIOused, int8_t wlOnPin, int8_t wlIrqPin, int8_t extLPOPin) {
  uint32_t kHzWiFiClk;
  m_curState = IDLE_STATE;
  m_initDone = false;
  m_errorCode = WIFI_CARD_ERROR_NONE;
  m_highCapacity = false;
  m_version2 = false;
  fUseSDIO2 = SDIOused;
  m_wlIrqPin = wlIrqPin;
  uint8_t readResponse;
#if INIT_DEBUG_MODE == true
  printf("===========================\nCYW4343W Card::begin: %s\n===========================\n",
                 fUseSDIO2 ? "SDIO2" : "SDIO");
#endif 
  initSDHC(); // Initialize uSHCx
  //////////////
  //Setup pins//
  //////////////

  //////////////////////
  // In-band interrupt//
  //////////////////////
  // Attach in-band interrupt to DAT1 (GPIO mode only) - not in use, yet
  // TODO pin 34 is DAT1 on DB5 (pin #41 on T41). Need to change depending
  // on device. Eventually use lower level attachment, avoiding pin
  // #define DAT1_INTERRUPT_PIN 41 //34
  // printf(SER_TRACE "Attaching IB interrupt to pin %d\n" SER_RESET, DAT1_INTERRUPT_PIN);
  // pinMode(DAT1_INTERRUPT_PIN, INPUT);
  // attachInterrupt(digitalPinToInterrupt(DAT1_INTERRUPT_PIN), onDataInterruptHandler, FALLING);

  //////////////////////////////////
  //out-of-band interrupt on INT pin
  //////////////////////////////////
  if (m_wlIrqPin > -1) {
#if INIT_DEBUG_MODE == true
    printf(SER_TRACE "Attaching OOB interrupt to pin %d\n" SER_RESET, m_wlIrqPin);
#endif
    pinMode(m_wlIrqPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(m_wlIrqPin), onWLIRQInterruptHandler, FALLING);
  }
#if defined(USE_EXTERNAL_LPO)
  ////////////////////
  //External LPO clock
  ////////////////////
  if (extLPOPin > -1) {
    pinMode(extLPOPin, OUTPUT);              // Set the pin as output
    analogWriteFrequency(extLPOPin, 32768);  // Set the frequency to 32.768 kHz
    analogWrite(extLPOPin, 128);             // 50% duty cycle 
  }
#endif
  ///////////
  //WL_ON pin
  ///////////
  pinMode(wlOnPin, OUTPUT);  // Pull high to activate WLAN 
  digitalWriteFast(wlOnPin, HIGH); // Enable WLAN
  delay(185);                      // Delay to allow reset to complete

  //==============================================================================
  //Sequence is heavily influenced by the Pi Zero blog. Enhancements made to the flow by studying
  //Broadcom Full MAC and other drivers - Pi Zero code is a playback of a 'recording' of SDIO commands,
  //and shows apparently unused reads before writes that, in driver code, show that actually the data read
  //is manipulated, and written back.
  //==============================================================================
  //CMD 0
  cardCommand(CMD0_XFERTYP, 0);
  //CMD 5
  cardCommand(CMD5_XFERTYP, 0);
  //CMD 5
  //cardCommand(CMD5_XFERTYP, 0x200000); // Not needed
  //CMD 3
  // get RCA
  cardCommand(CMD3_XFERTYP, 0);
  m_rca = m_psdhc->CMD_RSP0 & 0xFFFF0000;
  //CMD 7
  cardCommand(CMD7_XFERTYP, m_rca);
  //Set function block sizes. CYW4343W datasheet p16 - F0/1/2 - 32/64/512
  cardCMD52_write(SD_FUNC_BUS, BUS_SDIO_BLKSIZE_REG, SD_BUS_BLK_BYTES & 0xFF);
  cardCMD52_write(SD_FUNC_BUS, BUS_SDIO_BLKSIZE_REG + 1, SD_BUS_BLK_BYTES >> 8);
  //Set backplane block size
  cardCMD52_write(SD_FUNC_BUS, BUS_BAK_BLKSIZE_REG, SD_BAK_BLK_BYTES & 0xFF);
  cardCMD52_write(SD_FUNC_BUS, BUS_BAK_BLKSIZE_REG + 1, SD_BAK_BLK_BYTES >> 8);
  //Set radio block size
  cardCMD52_write(SD_FUNC_BUS, BUS_RAD_BLKSIZE_REG, SD_RAD_BLK_BYTES & 0xFF);
  cardCMD52_write(SD_FUNC_BUS, BUS_RAD_BLKSIZE_REG + 1, SD_RAD_BLK_BYTES >> 8);
  //Device must support high-speed mode
  cardCMD52_read(SD_FUNC_BUS, BUS_SPEED_CTRL_REG, &readResponse);
  if(readResponse & 1) {
    //Set card bus high speed interface
    cardCMD52_write(SD_FUNC_BUS, BUS_SPEED_CTRL_REG, readResponse | 2); // Zero set to 0x03
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "Enabled CYW4343W bus high speed interface\n" SER_RESET);
#endif
  }
  //Set the card bus to 4-bits
  cardCMD52_read(SD_FUNC_BUS, BUS_BI_CTRL_REG, &readResponse);
  cardCMD52_write(SD_FUNC_BUS, BUS_BI_CTRL_REG, (readResponse & ~3) | 2); // Zero set to 0x42
  //Enable I/O 
  if(SDIOEnableFunction(SD_FUNC_BAK_EN) == false) {
    return false;
  }
  //Verify I/O is ready
  cardCMD52_read(SD_FUNC_BUS, BUS_IORDY_REG, &readResponse);
  if (readResponse & 0x02) {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "BUS_IORDY_REG (Ready indication) returned OK\n" SER_RESET);
#endif
  } else {
    printf(SER_RED "BUS_IORDY_REG (Ready indication) returned %d\n" SER_RESET, readResponse);
    return false;
  }

  //Set SDHC FIFO read/write water mark levels
  m_psdhc->WTMK_LVL = SDHC_WML_RDWML(FIFO_WML) | SDHC_WML_WRWML(FIFO_WML);
  //Set SDHC bus to 4-bits
  m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_DTW_MASK;
  m_psdhc->PROT_CTRL |= SDHC_PROCTL_DTW(SDHC_PROCTL_DTW_4BIT);
  //Set the SDHC SCK frequency (33'000 seems to be the maximum working rate).
  
  // NOTE: The CYW43439 and CYW4343W may have issues at 50'000 KHz clock
  //       speed if using long connecting wires.
  //       If so, reduce to 33'000 KHz in "misc_defs.h".
  kHzWiFiClk = KHZ_WIFI_CLK; //33'000 or 50'000.

  //Disable GPIO
  enableSDIO(false);
  //Set clock
  setWiFiclk(kHzWiFiClk);
  //Enable GPIO
  enableSDIO(true);
#if INIT_DEBUG_MODE == true
  printf(SER_TRACE "SDHC bus set to 4-bit, speed set to %ldMHz\n" SER_RESET, kHzWiFiClk / 1000);
#endif
  //////////////////////////////////////////
  //End Included from SdFat init post CMD7//
  //////////////////////////////////////////
  //Set backplane window
  setBackplaneWindow(BAK_BASE_ADDR);
  u32Data u32d;
  //Read chip ID 
  //This was 4 byte read in the Zero code, causes extra bytes in the buffer - only if first CMD53 executed. 
  //Changing size to < 4 "fixes" it. Debug code sdio.c 3909 formats value as %4x, so use size 2
  cardCMD53_read(SD_FUNC_BAK, SB_32BIT_WIN, u32d.bytes, 2);
#if INIT_DEBUG_MODE == true
  printf(SER_GREEN "*************\nCardID: %ld\n*************\n" SER_RESET, u32d.uint32 & 0xFFFF);
#endif
#if USE_CYW4343W == false // Do 'wifiSetup' here else do 'wifiSetup' in QNEthernet.
  //============================
  // Setup the WiFi card (1DX) =
  //============================
  wifiSetup();
#endif
  return true;
}
//==============================================================================

//==============================================================================
// Setup WiFi device  =
//==============================================================================
bool W4343WCard::wifiSetup(void) {
  uint8_t readResponse;
  u32Data u32d;
  uint8_t data[520];

  ////////////////////////
  //Set chip clock - Zero
  ////////////////////////
  //sdio.c line 3913
  //Force PLL off until brcmf_chip_attach() programs PLL control regs
  cardCMD52_write(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ); // Was 0x28
  //Validate ALP is available
  cardCMD52_read(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, &readResponse);
 if ((readResponse & ~(0x80 | 0x40)) == (SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ)) {
//    printf(SER_GREEN "BAK_CHIP_CLOCK_CSR_REG returned 0x%02X\n" SER_RESET, readResponse);
  } else {
//    printf(SER_RED "BAK_CHIP_CLOCK_CSR_REG returned 0x%02X\n" SER_RESET, readResponse);
    return false;
  }
  cardCMD52_read(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, &readResponse);
  cardCMD52_write(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP);  // Was 0x21
  ////////////////////////
  ////////////////////////
  //Get chip ID again, and config base addr 
  cardCMD53_read(SD_FUNC_BAK, SB_32BIT_WIN, u32d.bytes, 4); // TODO Unused
  cardCMD53_read(SD_FUNC_BAK, SB_32BIT_WIN + 0xFC, u32d.bytes, 4); // TODO Unused
  //++++++++++++++++++++++++++
  //Start of download firmware
  //++++++++++++++++++++++++++
  // Reset cores
  backplaneWindow_write32(ARM_IOCTRL_REG, 0x03);
  backplaneWindow_write32(MAC_IOCTRL_REG, 0x07);
  backplaneWindow_write32(MAC_RESETCTRL_REG, 0x00);
  backplaneWindow_write32(MAC_IOCTRL_REG, 0x05);
  //[18.032572]
  backplaneWindow_write32(SRAM_IOCTRL_REG, 0x03);
  backplaneWindow_write32(SRAM_RESETCTRL_REG, 0x00);
  backplaneWindow_write32(SRAM_IOCTRL_REG, 0x01);
  if(!backplaneWindow_read32(SRAM_IOCTRL_REG, &u32d.uint32) || u32d.uint8 != 1) {
    printf(SER_RED "Set SRAM_IOCTRL_REG issue\n" SER_RESET);
    return false;
  } else {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "Set SRAM_IOCTRL_REG validated\n" SER_RESET);
#endif
  }
  //[18.034039]
  // This is CYW4343x specific stuff: Disable remap for SRAM_3
  backplaneWindow_write32(SRAM_BANKX_IDX_REG, 0x03);
  backplaneWindow_write32(SRAM_BANKX_PDA_REG, 0x00);
  //[18.034733]
  if(!backplaneWindow_read32(SRAM_IOCTRL_REG, &u32d.uint32) || u32d.uint8 != 1) {
    printf(SER_RED "Set SRAM_IOCTRL_REG second issue\n" SER_RESET);
    return false;
  } else {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "Set SRAM_IOCTRL_REG second validation\n" SER_RESET);
#endif
  }
  if(!backplaneWindow_read32(SRAM_RESETCTRL_REG, &u32d.uint32) || u32d.uint8 != 0) {
    printf(SER_RED "Set SRAM_RESETCTRL_REG issue\n" SER_RESET);
    return false;
  } else {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "Set SRAM_RESETCTRL_REG validated\n" SER_RESET);
#endif
  }
  //[18.035416]
  backplaneWindow_read32(SRAM_BASE_ADDR, &u32d.uint32);
  backplaneWindow_write32(SRAM_BANKX_IDX_REG, 0);
  backplaneWindow_read32(SRAM_UNKNOWN_REG, &u32d.uint32);
  backplaneWindow_write32(SRAM_BANKX_IDX_REG, 1);
  backplaneWindow_read32(SRAM_UNKNOWN_REG, &u32d.uint32);
  backplaneWindow_write32(SRAM_BANKX_IDX_REG, 2);
  backplaneWindow_read32(SRAM_UNKNOWN_REG, &u32d.uint32);
  backplaneWindow_write32(SRAM_BANKX_IDX_REG, 3);
  //[18.037502]
  // Verify value at BUS_BRCM_CARDCTRL
  // sdio.c line 3991
  // Set card control so an SDIO card reset does a WLAN backplane reset
  cardCMD52_read(SD_FUNC_BUS, BUS_BRCM_CARDCTRL, &readResponse);
  if(readResponse == 1) {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "BUS_BRCM_CARDCTRL returned %d\n" SER_RESET, readResponse);
#endif
  } else {
    printf(SER_RED "BUS_BRCM_CARDCTRL returned %d\n" SER_RESET, readResponse);
    return false;
  }
  readResponse |= SDIO_CCCR_BRCM_CARDCTRL_WLANRESET;
  cardCMD52_write(SD_FUNC_BUS, BUS_BRCM_CARDCTRL, readResponse);  // Was 3
  // sdio.c line 4002
  // Set PMUControl so a backplane reset does PMU state reload
  cardCMD53_read(SD_FUNC_BAK, 0x8600, u32d.bytes, 4);
  u32d.bytes[1] |= 0x40;
  cardCMD53_write(SD_FUNC_BAK, 0x8600, u32d.bytes, 4);
  // [18.052762]
  if(SDIOEnableFunction(SD_FUNC_BAK_EN) == false) {
    return false;
  }
  cardCMD52_write(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 0);
  delay(45);
  cardCMD52_write(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, SBSDIO_ALP_AVAIL_REQ);
  // Validate BAK_CHIP_CLOCK_CSR_REG
  cardCMD52_read(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, &readResponse);
  if (readResponse == 0x48) {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "BAK_CHIP_CLOCK_CSR_REG returned 0x%02X\n" SER_RESET, readResponse);
#endif
  } else {
    printf(SER_RED "BAK_CHIP_CLOCK_CSR_REG returned 0x%02X\n" SER_RESET, readResponse);
    return false;
  }
//==============================================================================
// Validate and upload Firmware 
//==============================================================================
  setBackplaneWindow(0x58000);
  cardCMD53_read(SD_FUNC_BAK, 0xEE80, data, 4);
//==============================================================================

//==============================================================================
// Validate and upload NVRAM 
//==============================================================================
#if WIFI_DEVICE == CYW43439
  uploadFirmware(FIRMWARE_LEN_9, (uintptr_t)firmware_bin_9);
  size_t wifi_nvram_len = sizeof(wifi_nvram_9);
  uintptr_t clm_p = reinterpret_cast<uintptr_t>(wifi_nvram_9); 
  uploadNVRAM(wifi_nvram_len, clm_p);
#else
  uploadFirmware(FIRMWARE_LEN_W, (uintptr_t)firmware_4343W);
  size_t wifi_nvram_len = sizeof(wifi_nvram_w);
  uintptr_t clm_p = reinterpret_cast<uintptr_t>(wifi_nvram_w); 
  uploadNVRAM(wifi_nvram_len, clm_p);
#endif
//==============================================================================

  // This prints the last 44 bytes of NVRAM upload. Comment out for now.
   cardCMD53_read(SD_FUNC_BAK, 0xFFD4, data, 44);
  //[19.146150]
  backplaneWindow_read32(SRAM_IOCTRL_REG, &u32d.uint32); 
  backplaneWindow_read32(SRAM_RESETCTRL_REG, &u32d.uint32);
  backplaneWindow_write32(SB_INT_STATUS_REG, 0xffffffff);
  //[19.147404]
  backplaneWindow_write32(ARM_IOCTRL_REG, 0x03);
  backplaneWindow_write32(ARM_RESETCTRL_REG, 0x00);
  backplaneWindow_write32(ARM_IOCTRL_REG, 0x01);
  backplaneWindow_read32(ARM_IOCTRL_REG, &u32d.uint32);
  setBackplaneWindow(BAK_BASE_ADDR);
  cardCMD52_write(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 0);
  //Request HT Avail
  cardCMD52_write(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, SBSDIO_HT_AVAIL_REQ);
  delay(50);
  //Validate HT Avail
  bool res = cardCMD52_read(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, &readResponse);
  if((res == false) || readResponse != 0xD0) {
    printf(SER_RED "Set BAK_CHIP_CLOCK_CSR_REG issue, response: 0x%02X\n" SER_RESET, readResponse);
    return false;
  } else {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "Set BAK_CHIP_CLOCK_CSR_REG validated\n" SER_RESET);
#endif
  }
  //[19.190728]
  cardCMD52_write(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 0xD2);
  backplaneWindow_write32(SB_TO_SB_MBOX_DATA_REG, 0x40000);
  if(SDIOEnableFunction(SD_FUNC_BAK_EN | SD_FUNC_RAD_EN) == false) {
    return false;
  }
  cardCMD52_read(SD_FUNC_BUS, BUS_IORDY_REG, &readResponse);
  delay(100);
  if(!cardCMD52_read(SD_FUNC_BUS, BUS_IORDY_REG, &readResponse) || readResponse != 0x06) {
    printf(SER_RED "Set BUS_IORDY_REG issue, response: 0x%02X\n" SER_RESET, readResponse);
    return false;
  } else {
#if INIT_DEBUG_MODE == true
    printf(SER_GREEN "Set BUS_IORDY_REG validated\n" SER_RESET);
#endif
  }
  //Disable pullups 
  cardCMD52_write(SD_FUNC_BAK, BAK_PULLUP_REG, 0);
  cardCMD52_read(SD_FUNC_BAK, BAK_PULLUP_REG, &readResponse);
  backplaneWindow_write32(SB_INT_HOST_MASK_REG, 0x200000F0);
  backplaneWindow_read32(SR_CONTROL1, &u32d.uint32);
  //[19.282972]
  setBackplaneWindow(BAK_BASE_ADDR);
  cardCMD52_write(SD_FUNC_BAK, BAK_WAKEUP_REG, 2);
  cardCMD52_write(SD_FUNC_BUS, BUS_BRCM_CARDCAP, 6);
  configureOOBInterrupt();
  cardCMD52_read(SD_FUNC_BUS, BUS_INTPEND_REG, &readResponse);
  //[19.284023]
  backplaneWindow_read32(SB_INT_STATUS_REG, &u32d.uint32);
  backplaneWindow_write32(SB_INT_STATUS_REG, 0x200000c0);
  backplaneWindow_read32(SB_TO_HOST_MBOX_DATA_REG, &u32d.uint32);
  backplaneWindow_write32(SB_TO_SB_MBOX_REG, 0x02);
  backplaneWindow_read32(SR_CONTROL1, &u32d.uint32);
  //[19.285708]
  backplaneWindow_read32(0x68000 | 0x7ffc, &u32d.uint32);
  setBackplaneWindow(0x38000);
  //TODO is this necessary? Comment out for now
  //cardCMD53_read(SD_FUNC_BAK, 0x70d4, data, 64);
  //[19.286520]
  backplaneWindow_read32(SB_INT_STATUS_REG, &u32d.uint32);
  backplaneWindow_write32(SB_INT_STATUS_REG, 0x80);
  cardCMD53_read(SD_FUNC_RAD, SB_32BIT_WIN, data, 64, false);
  dataISRReceived = false;
#if INIT_DEBUG_MODE == true
  printf("==============================\nEnd W4343WCard::begin: %s\n==============================\n", fUseSDIO2 ? "SDIO2" : "SDIO");
#endif
  return true;	
}
//==============================================================================

//----------------------------------
// IOCTL methods
//----------------------------------
//==============================================================================
// ????????????
//==============================================================================
int W4343WCard::set_iovar_mpc(uint8_t val) {
  printf(SER_TRACE "\n%s MPC\n" SER_RESET, (val == 0 ? "Disable" : "Enable"));
  int res = ioctl_set_uint32("mpc", 20, 1);
  return res;
}
//==============================================================================

//==============================================================================
// Get an unsigned integer IOCTL variable
//==============================================================================
int W4343WCard::ioctl_get_uint32(const char * name, int wait_msec,  uint8_t *data) {
  return ioctl_cmd(WLC_GET_VAR, name, wait_msec, 0, data, 4);
}

//==============================================================================
// Set an unsigned integer IOCTL variable
//==============================================================================
int W4343WCard::ioctl_set_uint32(const char * name, int wait_msec, uint32_t val) {
  u32Data u32 = {.uint32=val};
  return ioctl_cmd(WLC_SET_VAR, name, wait_msec, 1, u32.bytes, 4);
}
//==============================================================================

//==============================================================================
// IOCTL write with integer parameter
//==============================================================================
int W4343WCard::ioctl_wr_int32(int cmd, int wait_msec, int val) {
  u32Data u32 = {.uint32=(uint32_t)val};
  return ioctl_cmd(cmd, 0, wait_msec, 1, u32.bytes, 4);
}

//==============================================================================
// Get data block from IOCTL variable
//==============================================================================
int W4343WCard::ioctl_get_data(const char *name, int wait_msec, uint8_t *data, int dlen, bool logOutput) {
  return ioctl_cmd(WLC_GET_VAR, name, wait_msec, 0, data, dlen, logOutput);
}

//==============================================================================
// Set data block in IOCTL variable
//==============================================================================
int W4343WCard::ioctl_set_data(const char *name, int wait_msec, void *data, int len, bool logOutput) {
  return ioctl_cmd(WLC_SET_VAR, name, wait_msec, 1, data, len, logOutput);
}

//==============================================================================
// IOCTL write data - added 02-20-25 WW
//==============================================================================
int W4343WCard::ioctl_wr_data(int cmd, int wait_msec, void *data, int len) {
  return ioctl_cmd(cmd, 0, wait_msec, 1, data, len);
}

//==============================================================================
// IOCTL read data - added 02-20-25 WW
//==============================================================================
int W4343WCard::ioctl_rd_data(int cmd, int wait_msec, void *data, int len) {
    return(ioctl_cmd(cmd, 0, wait_msec, 0, data, len));
}

//==============================================================================

bool had_successful_packet = false;

//==============================================================================
// Do an IOCTL transaction, get response, optionally waiting for it
//==============================================================================
int W4343WCard::ioctl_cmd(int cmd, const char *name, int wait_msec, int wr, void *data, int dlen, bool logOutput) {
  static uint8_t txseq = 1;

  IOCTL_MSG_T4 *msgp = &ioctl_txmsg;
  IOCTL_CMD_T4 *cmdp = &msgp->cmd;
  int namelen = name ? strlen(name)+1 : 0;
  int txdlen = wr ? namelen + dlen : MAX(namelen, dlen);
  int hdrlen = cmdp->data - (uint8_t *)&ioctl_txmsg;
  int txlen = hdrlen + txdlen; //((hdrlen + txdlen + 3) / 4) * 4; //, rxlen;
  // Prepare IOCTL command
  memset(msgp, 0, sizeof(ioctl_txmsg));
  msgp->len = txlen;
  msgp->notlen = ~txlen & 0xffff;
  cmdp->sw_header.seq = txseq++;
  cmdp->sw_header.hdrlen = 12;
  cmdp->cmd = cmd;
  cmdp->outlen = txdlen;
  cmdp->flags = (((uint32_t)++ioctl_reqid << 16) & 0xFFFF0000) | (wr ? 2 : 0);
  if(namelen > 0) memcpy(cmdp->data, name, namelen);
  if(wr) memcpy(&cmdp->data[namelen], data, dlen);
  // Send IOCTL command
  cardCMD53_write(SD_FUNC_RAD, SB_32BIT_WIN, (uint8_t *)msgp, CYW43_WRITE_BYTES_PAD(txlen), logOutput);
  return ioctl_cmd_poll_device(wait_msec, wr, data, dlen, logOutput);
}
//==============================================================================


//==============================================================================
// Poll CYW4343x for a response to a command.
//==============================================================================
int W4343WCard::ioctl_cmd_poll_device(int wait_msec, int wr, void *data, int dlen, bool logOutput) {
  IOCTL_MSG_T4 *rsp = &ioctl_rxmsg;
  int ret = 0;
  uint32_t val = 0;
  
  memset(rsp, 0, sizeof(ioctl_rxmsg));
  //TODO consider code in cyw43_ll_sdpcm_poll_device(), cyw43_ll.c, line 948
//  printf(SER_TRACE "Waiting for response" SER_RESET);
//  uint32_t start = micros();
  while (dataISRReceived == false) {}
//  printf(SER_MAGENTA " - IRQ response in %lduS\n", micros()-start);
  dataISRReceived = false;
  ioctl_wait(IOCTL_WAIT_USEC);
/*
  if (had_successful_packet == false) {
    // Clear interrupt status so that HOST_WAKE/SDIO line is cleared
    uint32_t int_status = backplaneWindow_read32(SB_INT_STATUS_REG, &val);
    printf("val: 0x%02X\n", val);
    if (val & 0x000000f0) {
      printf(SER_WARN "Clearing SDIO_INT_STATUS 0x%lx\n" SER_RESET, (int)(val & 0xf0));
      backplaneWindow_write32(SB_INT_STATUS_REG, val & 0xf0);
    }
  }
*/
  while (wait_msec >= 0 && ret == 0) {
    // Wait for response to be available
    wait_msec -= IOCTL_POLL_MSEC;
    backplaneWindow_read32(SB_INT_STATUS_REG, &val);
    // If response is waiting..
    if(val & 0xff) {
      // ..request response
      backplaneWindow_write32(SB_INT_STATUS_REG, val);
      // Fetch response
      //TODO - Temporarily break into header/data to compare lengths, help separate send/receive
      uint16_t hdr[2];
      ret = cardCMD53_read(SD_FUNC_RAD, SB_32BIT_WIN, (uint8_t *)hdr, 4, logOutput);
      if(hdr[0] == 0 && hdr[1] == 0) {
        // no packets
#if USE_DEBUG_MODE == true
        printf(SER_ERROR "No packets\n" SER_RESET);
#endif
        had_successful_packet = false;
        return 1;
      }
      had_successful_packet = true;
      //Validate header size = not header size
      if((hdr[0] ^ hdr[1]) != 0xffff) {
        printf(SER_ERROR "Header mismatch 0x%04x ^ 0x%04x\n" SER_RESET, hdr[0], hdr[1]);
        return 0;
      }
      memcpy(rsp, hdr, 4);
      ret = cardCMD53_read(SD_FUNC_RAD, SB_32BIT_WIN, (uint8_t *)rsp + 4, hdr[0] - 4, logOutput);
      // Discard response if not matching request
      if((rsp->cmd.flags >> 16) != ioctl_reqid) {
#if DEBUG_WARNINGS == true
        printf(SER_WARN "None matching request: cmd.flags: %ld, ioctl_reqid: %d\n" SER_RESET, (rsp->cmd.flags >> 16), ioctl_reqid);
#endif
        ret = 0;
      }
      // Exit if error response
      if(ret && (rsp->cmd.flags & 1)) {
        ret = 0;
        printf("Error flag set\n");
        break;
      }
      // If OK, copy data to buffer
      if(ret && !wr && data && dlen) {
        memcpy(data, rsp->cmd.data, dlen);
      }
    } else { // If no response, wait
#if USE_DEBUG_MODE == true
      printf("No respsone, wait....\n");
#endif
      delay(IOCTL_POLL_MSEC);
    }
  }
  return ret;
}
//==============================================================================

//==============================================================================
// Wait until IOCTL command has been processed. Pins need to be in GPIO mode for
// this read to work.
//==============================================================================
int W4343WCard::ioctl_wait(int usec) {
  int ready = 0;
//  uint32_t startMicros = micros();
  //TODO restore wait time before timeout, or switch to interrupt - currently takes significantly longer than the requested timeout
  while(ready == false) { // && !(micros() > startMicros + (usec * 2))) {
    ready = fUseSDIO2 == false ? (GPIO3_PSR & (1 << 15)) ? false : true : (GPIO1_PSR & (1 << 21)) ? false : true;
  }
//  printf("%s Ready in %lduS, expected %lduS\n" SER_RESET, ready == true ? SER_MAGENTA "Is": SER_RED "Not", micros()- startMicros, usec);
  return ready;
}
//==============================================================================

//==============================================================================
// Display last IOCTL if error ******** FIX ME *************************
//==============================================================================
void W4343WCard::ioctl_err_display(int retval) {
/*
    IOCTL_MSG_T4 *msgp = &ioctl_txmsg;
    IOCTL_HDR *iohp = (IOCTL_HDR *)&msgp->data[msgp->cmd.sdpcm.hdrlen];
    char *cmds = iohp->cmd==WLC_GET_VAR ? (char *)"GET" : 
                 iohp->cmd==WLC_SET_VAR ? (char *)"SET" : (char *)"";
    char *data, *name;
*/   
    if(retval <= 0) {
//        data = (char *)&msgp->data[msgp->cmd.sdpcm.hdrlen+sizeof(IOCTL_HDR)];
//        name = iohp->cmd==WLC_GET_VAR || iohp->cmd==WLC_SET_VAR ? data : (char *)"";
//        printf("IOCTL error: cmd %lu %s %s\n", iohp->cmd, cmds, name);
      printf("IOCTL error: %d\n",retval);
    }
}
//==============================================================================

//==============================================================================
// Return last error.
//==============================================================================
uint8_t W4343WCard::errorCode() const {
  return m_errorCode;
}
//==============================================================================

//==============================================================================
// Return IRQ error status.
//==============================================================================
uint32_t W4343WCard::errorData() const {
  return m_irqstat;
}
//==============================================================================

//==============================================================================
// Return line number of error.
//==============================================================================
uint32_t W4343WCard::errorLine() const {
  return m_errorLine;
}
//==============================================================================

//==============================================================================
// Return WIFI clock Frquency.
//==============================================================================
uint32_t W4343WCard::kHzWiFiClk() {
  return m_WiFiClkKhz;
}
//==============================================================================

//==============================================================================
// print reponse return value.
//==============================================================================
void W4343WCard::printResponse(bool return_value) {
  printf("RSP: 0x%4.4lX  0x%4.4lX  0x%4.4lX  0x%4.4lX   RET: 0x%02X\n",
          m_psdhc->CMD_RSP0, m_psdhc->CMD_RSP1, m_psdhc->CMD_RSP2,
          m_psdhc->CMD_RSP3, return_value);
}
//==============================================================================

//==============================================================================
// Dump data as byte values Added 02-21-25
//==============================================================================
void W4343WCard::disp_bytes(uint8_t *data, int len) {
  while(len--) printf(SER_GREEN "%02x" SER_RESET, *data++);
}
//==============================================================================

//==============================================================================
// Dump data as hex
//==============================================================================
void W4343WCard::disp_bytes(int mask, uint8_t *data, int len) {
  if(mask == 0 || (mask & display_mode)) {
    while (len--) printf(SER_GREEN "%02X " SER_GREEN, *data++);
  }
}

//==============================================================================
// Display diagnostic data
//==============================================================================
void W4343WCard::display(int mask, const char* fmt, ...) {
  va_list args;

  if(mask == 0 || (mask & display_mode)) {
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}
//==============================================================================

//==============================================================================
// Set display mode
//==============================================================================
void W4343WCard::set_display_mode(int mask) {
  display_mode = mask;
}
//==============================================================================

//==============================================================================
// Return non-zero if timeout
//==============================================================================
bool W4343WCard::ustimeout(uint32_t *tickp, uint32_t usec) {
  uint32_t t = micros(), dt = t - *tickp;

  if(usec == 0 || dt >= usec) {
    *tickp = t;
    return (1);
  }
  return (0);
}
//==============================================================================

//==============================================================================
void waitInput();
void waitInput() {
  Serial.println("Press anykey to continue...");
  while (Serial.read() == -1) ;
  while (Serial.read() != -1) ;
}
//==============================================================================

//==============================================================================
void cwydump(unsigned char *memory, unsigned int len) {
  unsigned int	i=0, j=0;
  unsigned char	c=0;

  printf("\n\rADDR          00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
  printf("\n\r-------------------------------------------------------------\n\r");
  for(i = 0; i <= (len-1); i+=16) {
	printf("%8.8x",(unsigned int)(i + memory));
	printf("      ");
	for(j = 0; j < 16; j++) {
      c = memory[i+j];
      printf("%2.2x",c);
      printf(" ");
	}
	printf("  ");
	for(j = 0; j < 16; j++) {
      c = memory[i+j];
      if(c > 31 && c < 127)	printf("%c",c);
      else
        printf(".");
	}
	printf("\n");
  }
}
//==============================================================================

// END

#endif  // QNETHERNET_INTERNAL_DRIVER_CYW4343W
