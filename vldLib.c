/*
 * Copyright 2022, Jefferson Science Associates, LLC.
 * Subject to the terms in the LICENSE file found in the top-level directory.
 *
 *     Authors: Bryan Moffit
 *              moffit@jlab.org                   Jefferson Lab, MS-12B3
 *              Phone: (757) 269-5660             12000 Jefferson Ave.
 *              Fax:   (757) 269-5800             Newport News, VA 23606
 *
 * Description: Library for the JLab VME LED Driver
 *
 */

#include <pthread.h>
#include <stdio.h>
#include "jvme.h"
#include "vldLib.h"

/* Mutex to guard HV read/writes */
pthread_mutex_t vldMutex = PTHREAD_MUTEX_INITIALIZER;
#define VLOCK   if(pthread_mutex_lock(&vldMutex)<0) perror("pthread_mutex_lock");
#define VUNLOCK if(pthread_mutex_unlock(&vldMutex)<0) perror("pthread_mutex_unlock");

#define CHECKID(id)							\
  if((id<0) || (id>=MAX_VME_SLOTS) || (VLDp[id] == NULL))		\
    {									\
      printf("%s: ERROR : VLD id %d is not initialized \n",		\
	     __func__, id);						\
      return ERROR;							\
    }

/* Globals */
int32_t nVLD = 0;                        /* Number of initialized modules */
volatile vldRegs *VLDp[MAX_VME_SLOTS+1];  /* pointer to VLD memory maps, index = slotID */
volatile vldSerialRegs *VLDJTAGp[MAX_VME_SLOTS+1];
volatile vldSerialRegs *VLDI2Cp[MAX_VME_SLOTS+1];
int32_t vldID[MAX_VME_SLOTS+1];                    /**< array of slot numbers for TDs */
unsigned long vldA24Offset = 0;          /* Difference in CPU A24 Base and VME A24 Base */
uint32_t vldAddrList[MAX_VME_SLOTS+1];     /**< array of a24 addresses for TDs */


/*
 *  DEBUG Routine to check the register map with the vldRegs structure
 */

int32_t
vldCheckAddresses()
{
  int32_t rval = OK;
  uintptr_t offset = 0, expected = 0, base = 0;
  vldRegs fbase;

  printf("%s:\n\t ---------- Checking VLD memory map ---------- \n",
	 __func__);

  base = (uintptr_t) &fbase;
#ifndef CHECKOFFSET
#define CHECKOFFSET(_x, _y)						\
  offset = ((uintptr_t) &fbase._y) - base;				\
  expected = _x;							\
  if(offset != expected)						\
  {									\
  printf("%s: ERROR ->%s not at offset = 0x%lx (@ 0x%lx)\n",		\
	 __func__, #_y ,expected, offset);				\
  rval = ERROR;								\
  }
#endif

  CHECKOFFSET(0x0C, trigDelay);
  CHECKOFFSET(0x68, bleachTime);
  CHECKOFFSET(0x100, reset);

  return rval;
}

/*
 *    Initialize a bunch of VLD.
 *       Locate them by checking at addr and increment by addr_inc, nfind times.
 */

int32_t
vldInit(uint32_t addr, uint32_t addr_inc, uint32_t nfind, uint32_t iFlag)
{
  int32_t useList=0, noBoardInit=0, noFirmwareCheck=0;
  int32_t islot, ivld;
  int32_t res;
  uint32_t rdata, boardID;
  uintptr_t laddr, laddr_inc;
  uint32_t firmwareInfo=0, vldVersion=0, vldType=0;
  volatile vldRegs *vld;


  /* Check if we're skipping initialization, and just mapping the structure pointer */
  if(iFlag&VLD_INIT_NO_INIT)
    {
      noBoardInit = 1;
    }
  /* Check if we're skipping the firmware check */
  if(iFlag&VLD_INIT_SKIP_FIRMWARE_CHECK)
    {
      noFirmwareCheck=1;
    }
  /* Check if we're initializing using a list */
  if(iFlag&VLD_INIT_USE_ADDR_LIST)
    {
      useList=1;
    }

  /* Check for valid address */
  if( (addr==0) && (useList==0) )
    { /* Scan through valid slot -> A24 address */
      useList=1;
      nfind=16;

      /* Loop through JLab Standard GEOADDR to VME addresses to make a list */
      for(islot=3; islot<11; islot++) /* First 8 */
	vldAddrList[islot-3] = (islot<<19);

      /* Skip Switch Slots */

      for(islot=13; islot<21; islot++) /* Last 8 */
	vldAddrList[islot-5] = (islot<<19);

    }
  else if(addr > 0x00ffffff)
    { /* A32 Addressing */
      printf("%s: ERROR: A32 Addressing not allowed for VLD configuration space\n",
	     __FUNCTION__);
      return(ERROR);
    }
  else
    { /* A24 Addressing */
      if(addr < 22)
	{ /* First argument is a slot number, instead of VME address */
	  printf("%s: Initializing using slot number %d (VME address 0x%x)\n",
		 __FUNCTION__,addr,addr<<19);
	  addr = addr<<19; // Shift to VME A24 address;

	  /* If addr_inc is also in slot number form, shift it */
	  if((addr_inc<22) && (addr_inc>0))
	    addr_inc = addr_inc<<19;

	  /* Check and shift the address list, if it's used */
	  if(useList==1)
	    {
	      for(ivld=0; ivld<nfind; ivld++)
		{
		  if(vldAddrList[ivld] < 22)
		    {
		      vldAddrList[ivld] = vldAddrList[ivld]<<19;
		    }
		}
	    }
	}

      if( ((addr_inc==0)||(nfind==0)) && (useList==0) )
	{ /* assume only one VLD to initialize */
	  nfind = 1;
	}
    }

  /* get the VLD address */
#ifdef VXWORKS
  res = sysBusToLocalAdrs(0x39,(char *)(unsigned long)addr,(char **)&laddr);
#else
  res = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)addr,(char **)&laddr);
#endif

#ifndef SHOWERROR
#ifndef VXWORKS
  vmeSetQuietFlag(1);
#endif
#endif

  if (res != 0)
    {
#ifdef VXWORKS
      printf("%s: ERROR in sysBusToLocalAdrs(0x39,0x%x,&laddr) \n",
	     __FUNCTION__,addr);
#else
      printf("%s: ERROR in vmeBusToLocalAdrs(0x39,0x%x,&laddr) \n",
	     __FUNCTION__,addr);
#endif
      return(ERROR);
    }
  vldA24Offset = laddr - addr;

  for (ivld=0;ivld<nfind;ivld++)
    {
      if(useList==1)
	{
	  laddr_inc = vldAddrList[ivld] + vldA24Offset;
	}
      else
	{
	  laddr_inc = laddr +ivld*addr_inc;
	}

      vld = (vldRegs *)laddr_inc;
      /* Check if Board exists at that address */
#ifdef VXWORKS
      res = vxMemProbe((char *) &(vld->boardID),VX_READ,4,(char *)&rdata);
#else
      res = vmeMemProbe((char *) &(vld->boardID),4,(char *)&rdata);
#endif

      if(res < 0)
	{
#ifdef SHOWERRORS
#ifdef VXWORKS
	  printf("%s: ERROR: No addressable board at addr=0x%x\n",
		 __FUNCTION__,(UINT32) vld);
#else
	  printf("%s: ERROR: No addressable board at VME (Local) addr=0x%x (0x%x)\n",
		 __FUNCTION__,
		 (UINT32) laddr_inc-vldA24Offset, (UINT32) vld);
#endif
#endif /* SUPPRESSERRORSES */
	}
      else
	{
	  /* Check that it is a VLD */
	  if(((rdata&VLD_BOARDID_TYPE_MASK)>>24) != VLD_BOARDID_TYPE_VLD)
	    {
	      printf(" WARN: For board at VME addr=0x%x, Invalid Board ID: 0x%x\n",
		     (UINT32)(laddr_inc - vldA24Offset), rdata);
	      continue;
	    }
	  else
	    {
	      /* Check if this is board has a valid slot number */
	      boardID =  (rdata&VLD_BOARDID_GEOADR_MASK)>>8;
	      if((boardID <= 0)||(boardID >21))
		{
		  printf(" WARN: Board Slot ID is not in range: %d (this module ignored)\n"
			 ,boardID);
		  continue;
		}
	      else
		{
		  VLDp[boardID] = (vldRegs *)(laddr_inc);
		  vldID[nVLD] = boardID;

		  /* Get the Firmware Information and print out some details */
		  firmwareInfo = vldGetFirmwareVersion(vldID[nVLD]);

		  if(firmwareInfo>0)
		    {
		      printf("  User ID: 0x%x \tFirmware (type - revision): %X - %x.%x\n",
			     (firmwareInfo&VLD_FIRMWARE_ID_MASK)>>16,
			     (firmwareInfo&VLD_FIRMWARE_TYPE_MASK)>>12,
			     (firmwareInfo&VLD_FIRMWARE_MAJOR_VERSION_MASK)>>4,
			     firmwareInfo&VLD_FIRWMARE_MINOR_VERSION_MASK);

		      vldVersion = firmwareInfo&0xFFF;
		      vldType    = (firmwareInfo&VLD_FIRMWARE_TYPE_MASK)>>12;
		      if((vldVersion < VLD_SUPPORTED_FIRMWARE) || (vldType!=VLD_SUPPORTED_TYPE))
			{
			  if(noFirmwareCheck)
			    {
			      printf("%s: WARN: Type %x Firmware version (0x%x) not supported by this driver.\n  Supported: Type %x version 0x%x (IGNORED)\n",
				     __FUNCTION__,
				     vldType,vldVersion,VLD_SUPPORTED_TYPE,VLD_SUPPORTED_FIRMWARE);
			    }
			  else
			    {
			      printf("%s: ERROR: Type %x Firmware version (0x%x) not supported by this driver.\n  Supported Type %x version 0x%x\n",
				     __FUNCTION__,
				     vldType,vldVersion,VLD_SUPPORTED_TYPE,VLD_SUPPORTED_FIRMWARE);
			      VLDp[boardID]=NULL;
			      continue;
			    }
			}
		    }
		  else
		    {
		      printf("%s:  ERROR: Invalid firmware 0x%08x\n",
			     __FUNCTION__,firmwareInfo);
		      return ERROR;
		    }

		  printf("Initialized VLD %2d  Slot # %2d at address 0x%08lx (0x%08x) \n",
			 nVLD, vldID[nVLD],(unsigned long) VLDp[(vldID[nVLD])],
			 (UINT32)((unsigned long)VLDp[(vldID[nVLD])]-vldA24Offset));
		}
	    }
	  nVLD++;
	}
    }

#ifndef SHOWERROR
#ifndef VXWORKS
  vmeSetQuietFlag(0);
#endif
#endif

  if(noBoardInit)
    {
      if(nVLD>0)
	{
	  printf("%s: %d VLD(s) successfully mapped (not initialized)\n",
		 __FUNCTION__,nVLD);
	  return OK;
	}
    }

  if(nVLD==0)
    {
      printf("%s: ERROR: Unable to initialize any VLD modules\n",
	     __FUNCTION__);
      return ERROR;
    }

  return OK;

}


/*
 *  Show the settings and status of the initialized VLD
 *
 */

int32_t
vldGStatus(int32_t pFlag)
{

  return OK;
}

int32_t
vldGetFirmwareVersion(int32_t id)
{
  unsigned int rval=0;

  if(id==0) id=vldID[0];

  if(VLDp[id]==NULL)
    {
      printf("%s: ERROR: VLD in slot %d not initialized\n",__FUNCTION__,id);
      return ERROR;
    }

  VLOCK;
  /* reset the VME_to_JTAG engine logic */
  vmeWrite32(&VLDp[id]->reset,VLD_RESET_JTAG);

  /* Reset FPGA JTAG to "reset_idle" state */
  vmeWrite32(&VLDJTAGp[id]->data[(0x003C)>>2],0);

  /* enable the user_code readback */
  vmeWrite32(&VLDJTAGp[id]->data[(0x092C)>>2],0x3c8);

  /* shift in 32-bit to FPGA JTAG */
  vmeWrite32(&VLDJTAGp[id]->data[(0x1F1C)>>2],0);

  /* Readback the firmware version */
  rval = vmeRead32(&VLDJTAGp[id]->data[(0x1F1C)>>2]);
  VUNLOCK;

  return rval;
}

int32_t
vldSetTriggerDelayWidth(int32_t id, int32_t delay, int32_t delaystep, int32_t width)
{
  uint32_t wval=0;
  CHECKID(id);

  if((delay<0) || (delay>0x7F))
    {
      printf("%s: ERROR: Invalid delay (%d).  Must be <= %d\n",
	     __FUNCTION__,delay, 0x7f);
      return ERROR;
    }
  if((width<0) || (width> 0x1F))
    {
      printf("%s: ERROR: Invalid width (%d).  Must be <= %d\n",
	     __FUNCTION__,width, 0x1F);
    }

  delaystep = (delaystep ? VLD_TRIGDELAY_16NS_STEP_ENABLE : 0);

  VLOCK;
  wval = (delay) | (delaystep) | (width << 8);

  vmeWrite32(&VLDp[id]->trigDelay, wval);
  VUNLOCK;

  return OK;
}

int32_t
vldGetTriggerDelayWidth(int32_t id, int32_t *delay, int32_t *delaystep, int32_t *width)
{
  uint32_t rval=0;
  CHECKID(id);

  VLOCK;
  rval = vmeRead32(&VLDp[id]->trigDelay);

  *delay = rval & VLD_TRIGDELAY_DELAY_MASK;
  *delaystep = (rval & VLD_TRIGDELAY_16NS_STEP_ENABLE) ? 1 : 0;
  *width = (rval & VLD_TRIGDELAY_WIDTH_MASK) >> 8;

  VUNLOCK;

  return OK;
}

int32_t
vldSetTriggerSourceMask(int32_t id, uint32_t trigSrc)
{
  CHECKID(id);

  if((trigSrc & ~VLD_TRIGSRC_MASK) != 0)
    {
      printf("%s(%d): ERROR: Invalid trigSrc Mask (0x%x).  Allowed bits in mask 0x%x\n",
	     __func__, id, trigSrc, VLD_TRIGSRC_MASK);
      return ERROR;
    }

  VLOCK;
  vmeWrite32(&VLDp[id]->trigSrc, trigSrc);
  VUNLOCK;

  return OK;
}

int32_t
vldGetTriggerSourceMask(int32_t id, uint32_t *trigSrc)
{
  CHECKID(id);

  VLOCK;
  *trigSrc = vmeRead32(&VLDp[id]->trigSrc) & VLD_TRIGSRC_MASK;
  VUNLOCK;

  return OK;
}


int32_t
vldSetClockSource(int32_t id, uint32_t clkSrc)
{
  CHECKID(id);

  VLOCK;

  VUNLOCK;

  return OK;
}

/*
  if timer == 0, use the current register value
 */
int32_t
vldSetBleachTime(int32_t id, uint32_t timer, uint32_t enable)
{
  uint32_t wval = 0;
  CHECKID(id);

  if(timer > VLD_BLEACHTIME_TIMER_MASK)
    {
      printf("%s: WARN: Invalid Bleach time %u (0x%x).  Setting to Max (0x%x)\n",
	     __func__, timer, timer, VLD_BLEACHTIME_TIMER_MASK);
      timer = VLD_BLEACHTIME_TIMER_MASK;
    }

  enable = enable ? VLD_BLEACHTIME_ENABLE : 0;

  VLOCK;
  if(timer == 0)
    timer = vmeRead32(&VLDp[id]->bleachTime) & VLD_BLEACHTIME_TIMER_MASK;

  wval = timer | enable;

  vmeWrite32(&VLDp[id]->bleachTime, wval);
  VUNLOCK;

  return OK;
}

int32_t
vldGetBleachTime(int32_t id, uint32_t *timer, uint32_t *enable)
{
  uint32_t rval = 0;
  CHECKID(id);

  VLOCK;
  rval = vmeRead32(&VLDp[id]->bleachTime);

  *timer = rval & VLD_BLEACHTIME_TIMER_MASK;

  *enable = ((rval & VLD_BLEACHTIME_ENABLE_MASK) == VLD_BLEACHTIME_ENABLE );
  VUNLOCK;

  return OK;
}



int32_t
vldSetCalibrationPulseWidth(int32_t id, uint32_t width)
{
  CHECKID(id);

  if(width > VLD_CALIBRATIONWIDTH_MASK)
    {
      printf("%s: ERROR: Invalid Calibration Pulse Width (0x%x).  Max = 0x%x\n",
	     __func__, width, VLD_CALIBRATIONWIDTH_MASK);
      return ERROR;
    }

  VLOCK;
  vmeWrite32(&VLDp[id]->calibrationWidth, width);
  VUNLOCK;

  return OK;
}

int32_t
vldGetCalibrationPulseWidth(int32_t id, uint32_t *width)
{
  CHECKID(id);

  VLOCK;
  *width = vmeRead32(&VLDp[id]->calibrationWidth) & VLD_CALIBRATIONWIDTH_MASK;
  VUNLOCK;

  return OK;
}

int32_t
vldSetAnalogSwitchControl(int32_t id, uint32_t enableDelay, uint32_t enableWidth)
{
  uint32_t maxDelay = 0xFF, maxWidth = 0x7F;
  CHECKID(id);

  if(enableDelay > maxDelay)
    {
      printf("%s: ERROR: Invalid enableDelay (0%x).  Max = 0x%x\n",
	     __func__, enableDelay, maxDelay);
      return ERROR;
    }

  if(enableWidth > maxWidth)
    {
      printf("%s: ERROR: Invalid enableWidth (0%x).  Max = 0x%x\n",
	     __func__, enableWidth, maxWidth);
      return ERROR;
    }

  VLOCK;
  vmeWrite32(&VLDp[id]->analogCtrl, enableDelay | (enableWidth << 9));
  VUNLOCK;

  return OK;
}

int32_t
vldGetAnalogSwitchControl(int32_t id, uint32_t *enableDelay, uint32_t *enableWidth)
{
  uint32_t rval = 0;
  CHECKID(id);

  VLOCK;
  rval = vmeRead32(&VLDp[id]->analogCtrl);

  *enableDelay = rval & VLD_ANALOGCTRL_DELAY_MASK;
  *enableWidth = (rval & VLD_ANALOGCTRL_WIDTH_MASK) >> 9;
  VUNLOCK;

  return OK;
}

int32_t
vldSetRandomPulser(int32_t id, uint32_t prescale, uint32_t enable)
{
  uint32_t maxPrescale = 0x7;
  CHECKID(id);

  if(prescale > maxPrescale)
    {
      printf("%s: ERROR: Invalid prescale 0x%x. Max = %d\n",
	     __func__, prescale, maxPrescale);
    }

  enable = enable ? VLD_RANDOMTRIG_ENABLE : 0;

  VLOCK;
  if(prescale == 0)
    prescale = vmeRead32(&VLDp[id]->randomTrig) & VLD_RANDOMTRIG_PRESCALE_MASK;

  vmeWrite32(&VLDp[id]->randomTrig, prescale | (prescale << 4) | enable);
  VUNLOCK;

  return OK;
}

int32_t
vldGetRandomPulser(int32_t id, uint32_t *prescale, uint32_t *enable)
{
  uint32_t rval = 0;
  CHECKID(id);

  VLOCK;
  rval = vmeRead32(&VLDp[id]->randomTrig);

  *prescale = rval & VLD_RANDOMTRIG_PRESCALE_MASK;
  *enable = (rval & VLD_RANDOMTRIG_ENABLE) ? 1 : 0;
  VUNLOCK;

  return OK;
}


int32_t
vldSetPeriodicPulser(int32_t id, uint32_t period, uint32_t npulses)
{
  uint32_t maxPeriod = 0xFFFF, maxNpulses = 0xFFFF;
  CHECKID(id);

  if(period > maxPeriod)
    {
      printf("%s: ERROR: Invalid period (%u).  Max = %u.\n",
	     __func__, period, maxPeriod);
      return ERROR;
    }

  if(npulses > maxNpulses)
    {
      printf("%s: WARN: Invalid npulses (%u).  Set to = %u.\n",
	     __func__, npulses, maxNpulses);
      npulses = maxNpulses;
    }

  VLOCK;
  if(period == 0)
    period = (vmeRead32(&VLDp[id]->periodicTrig) & VLD_PERIODICTRIG_PERIOD_MASK) >> 16;

  vmeWrite32(&VLDp[id]->periodicTrig, npulses | (period << 16));
  VUNLOCK;

  return OK;
}

int32_t
vldGetPeriodicPulser(int32_t id, uint32_t *period, uint32_t *npulses)
{
  uint32_t rval = 0;
  CHECKID(id);

  VLOCK;
  rval = vmeRead32(&VLDp[id]->periodicTrig);

  *period = rval & VLD_PERIODICTRIG_NPULSES_MASK;
  *npulses = (rval & VLD_PERIODICTRIG_PERIOD_MASK) >> 16;
  VUNLOCK;

  return OK;
}


int32_t
vldGetTriggerCount(int32_t id, uint32_t *trigCnt)
{
  CHECKID(id);

  VLOCK;
  *trigCnt = vmeRead32(&VLDp[id]->trigCnt);
  VUNLOCK;

  return OK;
}

int32_t
vldResetMask(int32_t id, uint32_t resetMask)
{
  CHECKID(id);

  if((resetMask & ~VLD_RESET_MASK) != 0)
    {
      printf("%s: WARN: Of resetMask (0x%x), only these are defined (0x%x)\n",
	     __func__, resetMask, resetMask & VLD_RESET_MASK);
    }

  VLOCK;
  vmeWrite32(&VLDp[id]->reset, resetMask & VLD_RESET_MASK);
  VUNLOCK;

  return OK;
}

int32_t
vldResetI2C(int32_t id)
{
  CHECKID(id);

  VLOCK;
  vmeWrite32(&VLDp[id]->reset, VLD_RESET_I2C);
  VUNLOCK;

  return OK;
}

int32_t
vldResetJTAG(int32_t id)
{
  CHECKID(id);

  VLOCK;
  vmeWrite32(&VLDp[id]->reset, VLD_RESET_JTAG);
  VUNLOCK;

  return OK;
}

int32_t
vldSoftReset(int32_t id)
  {
  CHECKID(id);

  VLOCK;
  vmeWrite32(&VLDp[id]->reset, VLD_RESET_SOFT);
  VUNLOCK;

  return OK;
}

int32_t
vldResetClockDCM(int32_t id)
{
  CHECKID(id);

  VLOCK;
  vmeWrite32(&VLDp[id]->reset, VLD_RESET_CLK);
  VUNLOCK;

  return OK;
}

int32_t
vldResetMGT(int32_t id)
{
  CHECKID(id);

  VLOCK;
  vmeWrite32(&VLDp[id]->reset, VLD_RESET_MGT);
  VUNLOCK;

  return OK;
}

int32_t
vldHardClockReset(int32_t id)
{
  CHECKID(id);

  VLOCK;
  vmeWrite32(&VLDp[id]->reset, VLD_RESET_HARD_CLK);
  VUNLOCK;

  return OK;
}
