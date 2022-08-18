#pragma once
/*
 * Copyright 2022, Jefferson Science Associates, LLC.
 * Subject to the terms in the LICENSE file found in the top-level directory.
 *
 *     Authors: Bryan Moffit
 *              moffit@jlab.org                   Jefferson Lab, MS-12B3
 *              Phone: (757) 269-5660             12000 Jefferson Ave.
 *              Fax:   (757) 269-5800             Newport News, VA 23606
 *
 * Description: Header for VME LED Driver library
 *
 */

#include <stdint.h>


/* Automatically generate unique blank register names */
#ifdef __COUNTER__
#define _BLANK JOIN(_blank, __COUNTER__)
#else
#define _BLANK JOIN(_blank, __LINE__)
#endif
#define JOIN(x,y) _DO_JOIN(x,y)
#define _DO_JOIN(x,y) x##y


typedef struct
{
  /* 0x0000 */ volatile uint32_t low_ctrl;
  /* 0x0004 */ volatile uint32_t high;
} ledControlRegs;

#define LED_CONTROL_CALIBRATION_ENABLED   (1 << 0)
#define LED_CONTROL_CH_ENABLE_MASK        0x0007FFFE
#define LED_CONTROL_BLEACH_CTRL_MASK      0x07000000
#define LED_CONTROL_BLEACH_REG_ENABLE     (1 << 27)
#define LED_CONTROL_BLEACH_ENABLE_MASK    0xF0000000
#define LED_CONTROL_BLEACH_ENABLE         0xB0000000

typedef struct
{
  /* 0x0000 */ volatile uint32_t boardID;
  /* 0x0004 */ uint32_t _BLANK[(0xC-0x4)>>2];
  /* 0x000C */ volatile uint32_t trigDelay;
  /* 0x0010 */ uint32_t _BLANK[(0x20-0x10)>>2];
  /* 0x0020 */ volatile uint32_t trigSrc;
  /* 0x0024 */ uint32_t _BLANK[(0x2C-0x24)>>2];
  /* 0x002C */ volatile uint32_t clockSrc;
  /* 0x0030 */ uint32_t _BLANK[(0x40-0x30)>>2];
  /* 0x0040 */ ledControlRegs output[5];
  /* 0x0068 */ volatile uint32_t bleachTime;
  /* 0x006C */ volatile uint32_t pulseLoad;
  /* 0x0070 */ volatile uint32_t calibrationWidth;
  /* 0x0074 */ volatile uint32_t analogCtrl;
  /* 0x0078 */ uint32_t _BLANK[(0x88-0x78)>>2];
  /* 0x0088 */ volatile uint32_t randomTrig;
  /* 0x008C */ volatile uint32_t periodicTrig;
  /* 0x0090 */ uint32_t _BLANK[(0xDC-0x90)>>2];
  /* 0x00DC */ volatile uint32_t trigCnt;
  /* 0x00E0 */ uint32_t _BLANK[(0x100-0xE0)>>2];
  /* 0x0100 */ volatile uint32_t reset;
} vldRegs;

typedef struct
{
  /* 0x20000 */ volatile uint32_t data[(0x10000)>>2];
} vldSerialRegs;

/* Firmware Masks */
#define VLD_FIRMWARE_ID_MASK              0xFFFF0000
#define VLD_FIRMWARE_TYPE_MASK            0x0000F000
#define VLD_FIRMWARE_TYPE_PROD            1
#define VLD_FIRMWARE_TYPE_P               3
#define VLD_FIRMWARE_MAJOR_VERSION_MASK   0x00000FF0
#define VLD_FIRWMARE_MINOR_VERSION_MASK   0x0000000F

#define VLD_SUPPORTED_FIRMWARE 0x81
#define VLD_SUPPORTED_TYPE     VLD_FIRMWARE_TYPE_P

/* 0x0 boardID bits and masks */
#define VLD_BOARDID_TYPE_VLD            0x1D
#define VLD_BOARDID_TYPE_MASK     0xFF000000
#define VLD_BOARDID_VME64X        (1 << 13)
#define VLD_BOARDID_PROD_MASK     0x00FF0000
#define VLD_BOARDID_GEOADR_MASK   0x00001F00
#define VLD_BOARDID_CRATEID_MASK  0x000000FF

/* 0xC trigDelay */
#define VLD_TRIGDELAY_DELAY_MASK       0x0000007F
#define VLD_TRIGDELAY_16NS_STEP_ENABLE (1 << 7)
#define VLD_TRIGDELAY_WIDTH_MASK       0x00001F00

/* 0x20 trigSrc */
#define VLD_TRIGSRC_MASK                      0x17
#define VLD_TRIGSRC_INTERNAL_PERIODIC_ENABLE  (1 << 0)
#define VLD_TRIGSRC_INTERNAL_RANDOM_ENABLE    (1 << 1)
#define VLD_TRIGSRC_INTERNAL_SEQUENCE_ENABLE  (1 << 2)
#define VLD_TRIGSRC_EXTERNAL_ENABLE           (1 << 4)


/* 0x100 reset bits and masks */
#define VLD_RESET_I2C                  (1<<1)
#define VLD_RESET_JTAG                 (1<<2)
#define VLD_RESET_SOFT                 (1<<4)
#define VLD_RESET_CLK                  (1<<8)
#define VLD_RESET_MGT                  (1<<10)
#define VLD_RESET_HARD_CLK             (1<<21)


/* vldInit initialization flag bits */
#define VLD_INIT_NO_INIT                 (1<<0)
#define VLD_INIT_SKIP_FIRMWARE_CHECK     (1<<2)
#define VLD_INIT_USE_ADDR_LIST           (1<<3)

#ifndef MAX_VME_SLOTS
/** This is either 20 or 21 */
#define MAX_VME_SLOTS 21
#endif

int32_t  vldCheckAddresses();
int32_t  vldInit(uint32_t vme_addr, uint32_t vme_incr, uint32_t nincr, uint32_t iFlag);
int32_t  vldGStatus(int32_t pFlag);
int32_t  vldGetFirmwareVersion(int32_t id);
int32_t  vldSetTriggerDelayWidth(int32_t id, int32_t delay, int32_t delaystep, int32_t width);
int32_t  vldGetTriggerDelayWidth(int32_t id, int32_t *delay, int32_t *delaystep, int32_t *width);
int32_t  vldSetTriggerSource(int32_t id, uint32_t trigSrc);
int32_t  vldGetTriggerSource(int32_t id, uint32_t *trigSrc);
