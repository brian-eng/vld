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
  /* 0x000C */ volatile uint32_t trigDelayWidth;
  /* 0x0010 */ uint32_t _BLANK[(0x20-0x10)>>2];
  /* 0x0020 */ volatile uint32_t trigSrc;
  /* 0x0024 */ uint32_t _BLANK[(0x2C-0x24)>>2];
  /* 0x002C */ volatile uint32_t clockSrc;
  /* 0x0040 */ ledControlRegs connector[5];
  /* 0x0068 */ volatile uint32_t bleachTime;
  /* 0x006C */ volatile uint32_t pulseLoad;
  /* 0x0070 */ volatile uint32_t calibrationWidth;
  /* 0x0074 */ volatile uint32_t analogCtrl;
  /* 0x0088 */ volatile uint32_t randomTrig;
  /* 0x008C */ volatile uint32_t periodicTrig;
  /* 0x00DC */ volatile uint32_t trigCnt;
  /* 0x0100 */ volatile uint32_t reset;
} vldRegs;

int32_t  vldCheckAddresses();
int32_t  vldInit(uint32_t vme_addr, uint32_t iFlag);
int32_t  vldStatus(int32_t pFlag);
