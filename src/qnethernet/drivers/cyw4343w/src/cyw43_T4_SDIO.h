//cyw43_T4_SDIO.h

#ifndef CYW43_T4_SDIO_H
#define CYW43_T4_SDIO_H

#include <stddef.h>
#include "WiFiCardInfo.h"
#include "misc_defs.h"
#include "ioctl_T4.h"
#include "event.h"

// Set true if using with QNethernet library.
#define USE_CYW4343W true

/**
 *  maximum initialization clock rate.
 */
#ifndef WIFI_MAX_INIT_RATE_KHZ
#define WIFI_MAX_INIT_RATE_KHZ 400
#endif  // WIFI_MAX_INIT_RATE_KHZ
/**/
/*
#define CHECK(f, a, ...) {if (f(a, __VA_ARGS__) == false) {\
                          printf(SER_RED "\nError: %s(%s ...)\n" SER_RESET, #f, #a);}\
                        else {\
                          printf(SER_TRACE "\nSuccess: %s(%s ...)\n" SER_RESET, #f, #a);}\
                        }
*/
#define CHECK(f, a, ...) /*{if (f(a, __VA_ARGS__) == false) {\*/
                          /*printf(SER_RED "\nError: %s(%s ...)\n" SER_RESET, #f, #a);}\*/
                        /*else {\*/
                          /*printf(SER_TRACE "\nSuccess: %s(%s ...)\n" SER_RESET, #f, #a);}\*/
                        /*}*/
class W4343WCard;
//------------------------------------------------------------------------------
/**
 * \class W4343WCard
 * \brief Raw SDIO access to SD and SDHC flash memory cards.
 */
class W4343WCard {
 public:

  // Whether an ioctl is in progress. While true, nothing else may access the
  // bus: the ioctl wait calls delay(), which may call yield() and run the
  // network stack.
  static volatile bool ioctlBusy;

  bool begin(bool SDIOused, int8_t wlOnPin, int8_t wlIrqPin, int8_t extLPOPin = -1);

  uint8_t errorCode() const;
  /** \return error data for last error. */
  uint32_t errorData() const;
  /** \return error line for last error. Tmp function for debug. */
  uint32_t errorLine() const;
  /** \return the WiFi clock frequency in kHz. */
  uint32_t kHzWiFiClk();
  ///////////////////////////
  // IRW new public functions
  ///////////////////////////
  int getMACAddress(uint8_t * mac);
  char *getFirmwareVersion();
  char *getCLMVersion(void);
  void postInitSettings();
  void join_network_poll(void);
  bool ustimeout(uint32_t *tickp, uint32_t usec);

  ///////////////////////////////
  // End IRW new public functions
  ///////////////////////////////
  
  bool wifiSetup(void);
  void disp_bytes(uint8_t *data, int len);
  void disp_bytes(int mask, uint8_t *data, int len);
  void display(int mask, const char* fmt, ...);
  void set_display_mode(int mask);
  
  //-----------------------------------------------------------------------------------------------------------------
  // Moved from private.  
  //-----------------------------------------------------------------------------------------------------------------
  bool cardCMD52_read(uint32_t functionNumber, uint32_t registerAddress, uint8_t * response, bool logOutput = false);
  bool cardCMD52_write(uint32_t functionNumber, uint32_t registerAddress, uint8_t data, bool logOutput = false);
  bool cardCMD52(uint32_t functionNumber, uint32_t registerAddress, uint8_t data, bool write, bool readAfterWriteFlag, uint8_t * response, bool logOutput);
  
  void setBlockCountSize(bool blockMode, uint32_t functionNumber, uint32_t size);
  bool cardCMD53_read(uint32_t functionNumber, uint32_t registerAddress, uint8_t * buffer, uint32_t size, bool logOutput = false);
  bool cardCMD53_write(uint32_t functionNumber, uint32_t registerAddress, uint8_t * buffer, uint32_t size, bool logOutput = false);
  bool cardCMD53(uint32_t functionNumber, uint32_t registerAddress, uint8_t * buffer, uint32_t size, bool write, bool logOutput);

  int ioctl_get_uint32(const char * name, int wait_msec,  uint8_t *data);
  int ioctl_set_uint32(const char *name, int wait_msec, uint32_t val);
  int ioctl_wr_int32(int cmd, int wait_msec, int val);
  int ioctl_get_data(const char *name, int wait_msec, uint8_t *data, int dlen, bool logOutput = false);
  int ioctl_set_data(const char *name, int wait_msec, void *data, int len, bool logOutput = false);
  int ioctl_wr_data(int cmd, int wait_msec, void *data, int len);
  int ioctl_rd_data(int cmd, int wait_msec, void *data, int len);
  int ioctl_cmd(int cmd, const char *name, int wait_msec, int wr, void *data, int dlen, bool logOutput = false);
  int ioctl_cmd_poll_device(int wait_msec, int wr, void *data, int dlen, bool logOutput = false);
  int ioctl_wait(int usec);
  int set_iovar_mpc(uint8_t val);

  void setBackplaneWindow(uint32_t addr);
  uint32_t setBackplaneWindow_retOffset(uint32_t addr);
  uint32_t backplaneWindow_read32(uint32_t addr, uint32_t *valp);
  uint32_t backplaneWindow_write32(uint32_t addr, uint32_t val);

  void printResponse(bool return_value);
  void ioctl_err_display(int retval);
  IMXRT_USDHC_t *m_psdhc;

  //-----------------------------------------------------------------------------------------------------------------

private:
  static const uint8_t IDLE_STATE = 0;
  static const uint8_t READ_STATE = 1;
  static const uint8_t WRITE_STATE = 2;
  uint32_t m_curSector;
  uint8_t m_curState = IDLE_STATE;
  static volatile bool fUseSDIO2;
  int8_t m_wlIrqPin = -1;

  // move helper functions into class.
  typedef bool (W4343WCard::*pcheckfcn)();
  bool cardCommand(uint32_t xfertyp, uint32_t arg);
  void enableSDIO(bool enable);
  void enableDmaIrs();
  void initSDHC();
  bool isBusyCommandComplete();
  bool isBusyCommandInhibit();
  void setWiFiclk(uint32_t kHzMax);
  bool waitTimeout(pcheckfcn fcn);
  inline bool setWifiErrorCode(uint8_t code, uint32_t line);
    
  /////////////////////////////////////////////
  static void wifiISR();
  static void wifiISR2(); // one for second SDIO
  static W4343WCard *s_pSdioCards[2];
  void gpioMux(uint8_t mode);
  void initClock();
  uint32_t baseClock();
    
  ////////////////////
  // IRW new functions
  ////////////////////
  void makeSDIO_DAT1();
  void makeGPIO_DAT1();
  bool SDIOEnableFunction(uint8_t functionEnable);
  bool SDIODisableFunction(uint8_t functionEnable);
  bool configureOOBInterrupt();
  static volatile bool dataISRReceived;
  static void onWLIRQInterruptHandler();
  static void onDataInterruptHandler();
  bool prepareDataTransfer(uint8_t *buffer, uint16_t length) ;
  bool completeDataTransfer() ;

  bool checkValidFirmware(size_t len, uintptr_t source);
  bool uploadFirmware(size_t firmwareSize, uintptr_t source);
  bool uploadNVRAM(size_t nvRAMSize, uintptr_t source);
  bool uploadCLM();

  bool isBusyTransferComplete();

  /////////////////////////////////////////////////////
  // lets move global (static) variables into class instance.
  pcheckfcn m_busyFcn = nullptr;
  bool m_initDone = false;
  bool m_version2;
  bool m_highCapacity;
  bool m_transferActive = false;
  uint8_t m_errorCode = WIFI_CARD_ERROR_INIT_NOT_CALLED;
  uint32_t m_errorLine = 0;
  uint32_t m_rca;
  volatile bool m_dmaBusy = false;
  volatile uint32_t m_irqstat;
  uint32_t m_WiFiClkKhz = 0;
  uint32_t m_ocr;
};

extern W4343WCard WIFIcard;

#endif  // CYW43_T4_SDIO_H
