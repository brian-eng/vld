/**
 * @copyright Copyright 2022, Jefferson Science Associates, LLC.
 *            Subject to the terms in the LICENSE file found in the
 *            top-level directory.
 *
 * @author    Bryan Moffit
 *            moffit@jlab.org                   Jefferson Lab, MS-12B3
 *            Phone: (757) 269-5660             12000 Jefferson Ave.
 *            Fax:   (757) 269-5800             Newport News, VA 23606
 *
 * @file      vldFirmwareUpdate.c
 * @brief     Firmware update program for the JLab VME LED Driver
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jvme.h"
#include "vldLib.h"


#ifdef VXWORKS
extern  int sysBusToLocalAdrs(int, char *, char **);
extern STATUS vxMemProbe(char *, int, int, char*);
extern UINT32 sysTimeBaseLGet();
extern STATUS taskDelay(int);
#ifdef TEMPE
extern unsigned int sysTempeSetAM(unsigned int, unsigned int);
#else
extern unsigned int sysUnivSetUserAM(int, int);
extern unsigned int sysUnivSetLSI(unsigned short, unsigned short);
#endif /*TEMPE*/
#endif

extern volatile vldRegs *VLDp[MAX_VME_SLOTS+1];
int32_t vld_slot = 0;
unsigned int firmwareInfo;
char *programName;

void vldFirmwareEMload(char *filename);
#ifndef VXWORKS
static void vldFirmwareUsage();
#endif

int
#ifdef VXWORKS
vldFirmwareUpdate(unsigned int arg_vmeSlot, char *arg_filename)
#else
main(int argc, char *argv[])
#endif
{
  int stat = 0, badInit = 0;
  char *filename;
  int inputchar=10;
  unsigned long laddr=0;
  int geo = 0;

  printf("\nVLD firmware update via VME\n");
  printf("----------------------------\n");

#ifdef VXWORKS
  programName = __FUNCTION__;

  vld_slot = arg_vmeAddr;
  filename = arg_filename;
#else
  programName = argv[0];

  if(argc<3)
    {
      printf(" ERROR: Must specify two arguments\n");
      vldFirmwareUsage();
      return(-1);
    }
  else
    {
      vld_slot = (unsigned int) strtoll(argv[1],NULL,10);
      filename = argv[2];
    }

  vmeSetQuietFlag(1);
  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    goto CLOSE;
#endif

  stat = vldInit(vld_slot, 0, 1, VLD_INIT_SKIP_FIRMWARE_CHECK);
  if(stat != OK)
    {
      printf("\n");
      printf("*** Failed to initialize VLD ***\nThis may indicate (either):\n");
      printf("   a) an incorrect VME slot provided\n");
      printf("   b) VLD is unresponsive and needs firmware reloaded\n");
      printf("\n");
      printf("Proceed with the update with the provided VME slot (%d)?\n", vld_slot);
    REPEAT:
      printf(" (y/n): ");
      inputchar = getchar();

      if((inputchar == 'n') || (inputchar == 'N'))
	{
	  printf("--- Exiting without update ---\n");
	  goto CLOSE;
	}
      else if((inputchar == 'y') || (inputchar == 'Y'))
	{
	  printf("--- Continuing update, assuming VME slot (%d) is correct ---\n", vld_slot);
	  printf("\n");
	  badInit = 1;
	}
      else
	{
	  goto REPEAT;
	}
    }

  printf("Press y to load firmware (%s) to the VLD via VME...\n",
	 filename);
  printf("\t or n to quit without update\n");

 REPEAT2:
  printf("(y/n): ");
  inputchar = getchar();

  if((inputchar == 'n') ||
     (inputchar == 'N'))
    {
      printf("--- Exiting without update ---\n");
      goto CLOSE;
    }
  else if((inputchar == 'y') ||
     (inputchar == 'Y'))
    {
    }
  else
    goto REPEAT2;

  /* Check to see if the VLD is in a VME-64X crate or Trying to recover corrupted firmware */
  if(badInit == 0)
    geo = vldGetGeoAddress(vld_slot);
  else
    geo = -1;

  if(geo <= 0)
    {
      if(geo == 0)
	{
	  printf("  ...Detected non VME-64X crate...\n");

	  /* Need to reset the Address to 0 to communicate with the emergency loading AM */
	  vld_slot = 0;
	}

#ifdef VXWORKS
      stat = sysBusToLocalAdrs(0x39,(char *)vme_addr,(char **)&laddr);
      if (stat != 0)
	{
	  printf("%s: ERROR: Error in sysBusToLocalAdrs res=%d \n",__FUNCTION__,stat);
	  goto CLOSE;
	}
#else
      stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)(vld_slot<<19),(char **)&laddr);
      if (stat != 0)
	{
	  printf("%s: ERROR: Error in vmeBusToLocalAdrs res=%d \n",__FUNCTION__,stat);
	  goto CLOSE;
	}
#endif
      VLDp[vld_slot] = (vldRegs *)laddr;
    }


  vldFirmwareEMload(filename);

 CLOSE:

#ifndef VXWORKS
  vmeCloseDefaultWindows();
#endif
  printf("\n");

  return OK;
}


#ifdef VXWORKS
static void
cpuDelay(unsigned int delays)
{
  unsigned int time_0, time_1, time, diff;
  time_0 = sysTimeBaseLGet();
  do
    {
      time_1 = sysTimeBaseLGet();
      time = sysTimeBaseLGet();
#ifdef DEBUG
      printf("Time base: %x , next call: %x , second call: %x \n",time_0,time_1, time);
#endif
      diff = time-time_0;
    } while (diff <delays);
}
#endif

static int
Emergency(unsigned int jtagType, unsigned int numBits, unsigned int *jtagData)
{
/*   unsigned long *laddr; */
  unsigned int iloop, iword, ibit;
  unsigned int shData;
  int rval=OK;
  uint32_t *eJTAGLoad = (uint32_t *)VLDp[vld_slot] + 0xFFFC;

/* #define DEBUG */
#ifdef DEBUG
  int numWord, i;
  printf("type: %x, num of Bits: %x, data: \n",jtagType, numBits);
  numWord = numBits ? ((numBits-1)/32 + 1 ) : 0;
  for (i=0; i<numWord; i++)
    {
      printf("%lx",jtagData[numWord-i-1]);
    }
  printf("\n");

  return OK;
#endif

  if (jtagType == 0) //JTAG reset, TMS high for 5 clcoks, and low for 1 clock;
    {
      for (iloop=0; iloop<5; iloop++)
	{
	  vmeWrite32(eJTAGLoad,1);
	}

      vmeWrite32(eJTAGLoad,0);
    }
  else if (jtagType == 1) // JTAG instruction shift
    {
      // Shift_IR header:
      vmeWrite32(eJTAGLoad,0);
      vmeWrite32(eJTAGLoad,1);
      vmeWrite32(eJTAGLoad,1);
      vmeWrite32(eJTAGLoad,0);
      vmeWrite32(eJTAGLoad,0);

      for (iloop =0; iloop <numBits; iloop++)
	{
	  iword = iloop/32;
	  ibit = iloop%32;
	  shData = ((jtagData[iword] >> ibit )<<1) &0x2;
	  if (iloop == numBits -1) shData = shData +1;  //set the TMS high for last bit to exit Shift_IR
	  vmeWrite32(eJTAGLoad, shData);
	}

      // shift _IR tail
      vmeWrite32(eJTAGLoad,1);
      vmeWrite32(eJTAGLoad,0);
    }
  else if (jtagType == 2)  // JTAG data shift
    {
      //shift_DR header
      vmeWrite32(eJTAGLoad,0);
      vmeWrite32(eJTAGLoad,1);
      vmeWrite32(eJTAGLoad,0);
      vmeWrite32(eJTAGLoad,0);

      for (iloop =0; iloop <numBits; iloop++)
	{
	  iword = iloop/32;
	  ibit = iloop%32;
	  shData = ((jtagData[iword] >> ibit )<<1) &0x2;
	  if (iloop == numBits -1) shData = shData +1;  //set the TMS high for last bit to exit Shift_DR
	  vmeWrite32(eJTAGLoad, shData);
	}

      // shift _DR tail
      vmeWrite32(eJTAGLoad,1);  // update Data_Register
      vmeWrite32(eJTAGLoad,0);  // back to the Run_test/Idle
    }
  else if (jtagType == 3) // JTAG instruction shift, stop at IR-PAUSE state, though, it started from IDLE
    {
      // Shift_IR header:
      vmeWrite32(eJTAGLoad,0);
      vmeWrite32(eJTAGLoad,1);
      vmeWrite32(eJTAGLoad,1);
      vmeWrite32(eJTAGLoad,0);
      vmeWrite32(eJTAGLoad,0);

      for (iloop =0; iloop <numBits; iloop++)
	{
	  iword = iloop/32;
	  ibit = iloop%32;
	  shData = ((jtagData[iword] >> ibit )<<1) &0x2;
	  if (iloop == numBits -1) shData = shData +1;  //set the TMS high for last bit to exit Shift_IR
	  vmeWrite32(eJTAGLoad, shData);
	}

      // shift _IR tail
      vmeWrite32(eJTAGLoad,0);  // update instruction register
      vmeWrite32(eJTAGLoad,0);  // back to the Run_test/Idle
    }
  else if (jtagType == 4)  // JTAG data shift, start from IR-PAUSE, end at IDLE
    {
      //shift_DR header
      vmeWrite32(eJTAGLoad,1);  //to EXIT2_IR
      vmeWrite32(eJTAGLoad,1);  //to UPDATE_IR
      vmeWrite32(eJTAGLoad,1);  //to SELECT-DR_SCAN
      vmeWrite32(eJTAGLoad,0);
      vmeWrite32(eJTAGLoad,0);

      for (iloop =0; iloop <numBits; iloop++)
	{
	  iword = iloop/32;
	  ibit = iloop%32;
	  shData = ((jtagData[iword] >> ibit )<<1) &0x2;
	  if (iloop == numBits -1) shData = shData +1;  //set the TMS high for last bit to exit Shift_DR
	  vmeWrite32(eJTAGLoad, shData);
	}

      // shift _DR tail
      vmeWrite32(eJTAGLoad,1);  // update Data_Register
      vmeWrite32(eJTAGLoad,0);  // back to the Run_test/Idle
    }
  else
    {
      printf( "\n JTAG type %d unrecognized \n",jtagType);
    }

  return rval;
}

static void
Parse(char *buf,unsigned int *Count,char **Word)
{
  *Word = buf;
  *Count = 0;
  while(*buf != '\0')
    {
      while ((*buf==' ') || (*buf=='\t') || (*buf=='\n') || (*buf=='"')) *(buf++)='\0';
      if ((*buf != '\n') && (*buf != '\0'))
	{
	  Word[(*Count)++] = buf;
	}
      while ((*buf!=' ')&&(*buf!='\0')&&(*buf!='\n')&&(*buf!='\t')&&(*buf!='"'))
	{
	  buf++;
	}
    }
  *buf = '\0';
}

void
vldFirmwareEMload(char *filename)
{
  unsigned int ShiftData[64], lineRead;
/*   unsigned int jtagType, jtagBit, iloop; */
  FILE *svfFile;
/*   int byteRead; */
  char bufRead[1024],bufRead2[256];
  unsigned int sndData[256];
  char *Word[16], *lastn;
  unsigned int nbits, nbytes, extrType, i, Count, nWords, nlines=0;
#ifdef CHECKREAD
  unsigned int rval=0;
  int stat=0;
#endif

  //A24 Address modifier redefined
#ifdef VXWORKS
#ifdef TEMPE
  printf("Set A24 mod\n");
  sysTempeSetAM(2,0x19);
#else /* Universe */
  sysUnivSetUserAM(0x19,0);
  sysUnivSetLSI(2,6);
#endif /*TEMPE*/
#else
  printf("\n");
  vmeBusLock();
  vmeSetA24AM(0x19);
#endif

#ifdef DEBUGFW
  printf("%s: A24 memory map is set to AM = 0x19 \n",__FUNCTION__);
#endif

  /* Check if TI board is readable */
#ifdef CHECKREAD
#ifdef VXWORKS
  stat = vxMemProbe((char *)(&VLDp[vld_slot]->boardID),0,4,(char *)&rval);
#else
  stat = vmeMemProbe((char *)(&VLDp[vld_slot]->boardID),4,(char *)&rval);
#endif
  if (stat != 0)
    {
      printf("%s: ERROR: TI card not addressable\n",__FUNCTION__);
      VLDp[vld_slot]=NULL;
      // A24 address modifier reset
#ifdef VXWORKS
#ifdef TEMPE
      sysTempeSetAM(2,0);
#else
      sysUnivSetLSI(2,1);
#endif /*TEMPE*/
#else
      vmeSetA24AM(0);
      vmeBusUnlock();
#endif
      return;
    }
#endif

  //open the file:
  svfFile = fopen(filename,"r");
  if(svfFile==NULL)
    {
      perror("fopen");
      printf("%s: ERROR: Unable to open file %s\n",__FUNCTION__,filename);

      // A24 address modifier reset
#ifdef VXWORKS
#ifdef TEMPE
      sysTempeSetAM(2,0);
#else
      sysUnivSetLSI(2,1);
#endif /*TEMPE*/
#else
      vmeSetA24AM(0);
      vmeBusUnlock();
#endif
      return;
    }

#ifdef DEBUGFW
  printf("\n File is open \n");
#endif

  //PROM JTAG reset/Idle
  Emergency(0,0,ShiftData);
#ifdef DEBUGFW
  printf("%s: Emergency PROM JTAG reset IDLE \n",__FUNCTION__);
#endif
  taskDelay(1);

  //Another PROM JTAG reset/Idle
  Emergency(0,0,ShiftData);
#ifdef DEBUGFW
  printf("%s: Emergency PROM JTAG reset IDLE \n",__FUNCTION__);
#endif
  taskDelay(1);


  //initialization
  extrType = 0;
  lineRead=0;

  printf("\n");
  fflush(stdout);

  /* Count the total number of lines */
  while (fgets(bufRead,256,svfFile) != NULL)
    {
      nlines++;
    }

  rewind(svfFile);

  while (fgets(bufRead,256,svfFile) != NULL)
    {
      lineRead +=1;
      if((lineRead%((int)(nlines/40))) ==0)
	{
#ifdef VXWORKS
	  /* This is pretty filthy... but at least shows some output when it's updating */
	  printf("     ");
	  printf("\b\b\b\b\b");
#endif
	  printf(".");
	  fflush(stdout);
	}
      //    fgets(bufRead,256,svfFile);
      if (((bufRead[0] == '/')&&(bufRead[1] == '/')) || (bufRead[0] == '!'))
	{
	  //	printf(" comment lines: %c%c \n",bufRead[0],bufRead[1]);
	}
      else
	{
	  if (strrchr(bufRead,';') ==0)
	    {
	      do
		{
		  lastn =strrchr(bufRead,'\n');
		  if (lastn !=0) lastn[0]='\0';
		  if (fgets(bufRead2,256,svfFile) != NULL)
		    {
		      strcat(bufRead,bufRead2);
		    }
		  else
		    {
		      printf("\n \n  !!! End of file Reached !!! \n \n");

		      // A24 address modifier reset
#ifdef VXWORKS
#ifdef TEMPE
		      sysTempeSetAM(2,0);
#else
		      sysUnivSetLSI(2,1);
#endif /*TEMPE*/
#else
		      vmeSetA24AM(0);
		      vmeBusUnlock();
#endif
		      return;
		    }
		}
	      while (strrchr(bufRead,';') == 0);  //do while loop
	    }  //end of if

	  // begin to parse the data bufRead
	  Parse(bufRead,&Count,&(Word[0]));
	  if (strcmp(Word[0],"SDR") == 0)
	    {
	      sscanf(Word[1],"%d",&nbits);
	      nbytes = (nbits-1)/8+1;
	      if (strcmp(Word[2],"TDI") == 0)
		{
		  for (i=0; i<nbytes; i++)
		    {
		      sscanf (&Word[3][2*(nbytes-i-1)+1],"%2x",&sndData[i]);
		      //  printf("Word: %c%c, data: %x \n",Word[3][2*(nbytes-i)-1],Word[3][2*(nbytes-i)],sndData[i]);
		    }
		  nWords = (nbits-1)/32+1;
		  for (i=0; i<nWords; i++)
		    {
		      ShiftData[i] = ((sndData[i*4+3]<<24)&0xff000000) + ((sndData[i*4+2]<<16)&0xff0000) + ((sndData[i*4+1]<<8)&0xff00) + (sndData[i*4]&0xff);
		    }
#ifdef SERIALNUMBER
		  // hijacking the PROM usercode:
		  if ((nbits == 32) && (ShiftData[0] == 0x71d55948)) {ShiftData[0] = BoardSerialNumber;}
#endif
		  //printf("Word[3]: %s \n",Word[3]);
		  //printf("sndData: %2x %2x %2x %2x, ShiftData: %08x \n",sndData[3],sndData[2],sndData[1],sndData[0], ShiftData[0]);
		  Emergency(2+extrType,nbits,ShiftData);
		}
	    }
	  else if (strcmp(Word[0],"SIR") == 0)
	    {
	      sscanf(Word[1],"%d",&nbits);
	      nbytes = (nbits-1)/8+1;
	      if (strcmp(Word[2],"TDI") == 0)
		{
		  for (i=0; i<nbytes; i++)
		    {
		      sscanf (&Word[3][2*(nbytes-i)-1],"%2x",&sndData[i]);
		      //  printf("Word: %c%c, data: %x \n",Word[3][2*(nbytes-i)-1],Word[3][2*(nbytes-i)],sndData[i]);
		    }
		  nWords = (nbits-1)/32+1;
		  for (i=0; i<nWords; i++)
		    {
		      ShiftData[i] = ((sndData[i*4+3]<<24)&0xff000000) + ((sndData[i*4+2]<<16)&0xff0000) + ((sndData[i*4+1]<<8)&0xff00) + (sndData[i*4]&0xff);
		    }
		  //printf("Word[3]: %s \n",Word[3]);
		  //printf("sndData: %2x %2x %2x %2x, ShiftData: %08x \n",sndData[3],sndData[2],sndData[1],sndData[0], ShiftData[0]);
		  Emergency(1+extrType,nbits,ShiftData);
		}
	    }
	  else if (strcmp(Word[0],"RUNTEST") == 0)
	    {
	      sscanf(Word[1],"%d",&nbits);
	      //	    printf("RUNTEST delay: %d \n",nbits);
	      if(nbits>100000)
		{
		  printf("Erasing (%.1f seconds): ..",((float)nbits)/2./1000000.);
		  fflush(stdout);
		}
#ifdef VXWORKS
	      cpuDelay(nbits*45);   //delay, assuming that the CPU is at 45 MHz
#else
	      usleep(nbits/2);
#endif
	      if(nbits>100000)
		{
		  printf("Done\n");
		  fflush(stdout);
		  printf("          ----------------------------------------\n");
		  printf("Updating: ");
		  fflush(stdout);
		}
/* 	      int time = (nbits/1000)+1; */
/* 	      taskDelay(time);   //delay, assuming that the CPU is at 45 MHz */
	    }
	  else if (strcmp(Word[0],"STATE") == 0)
	    {
	      if (strcmp(Word[1],"RESET") == 0) Emergency(0,0,ShiftData);
	    }
	  else if (strcmp(Word[0],"ENDIR") == 0)
	    {
	      if (strncmp(Word[1], "IDLE", 4) == 0)
		{
		  extrType = 0;
#ifdef DEBUGFW
		  printf(" ExtraType: %d \n",extrType);
#endif
		}
	      else if (strncmp(Word[1], "IRPAUSE", 7) == 0)
		{
		  extrType = 2;
#ifdef DEBUGFW
		  printf(" ExtraType: %d \n",extrType);
#endif
		}
	      else
		{
		  printf(" Unknown ENDIR type %s\n", Word[1]);
		}
	    }
	  else
	    {
#ifdef DEBUGFW
	      printf(" Command type ignored: %s \n",Word[0]);
#endif
	    }

	}  //end of if (comment statement)
    } //end of while

  printf("Done\n");

  printf("** Firmware Update Complete **\n");

  //close the file
  fclose(svfFile);

  // A24 address modifier reset
#ifdef VXWORKS
#ifdef TEMPE
  sysTempeSetAM(2,0);
#else
  sysUnivSetLSI(2,1);
#endif /*TEMPE*/
#else
  vmeSetA24AM(0);
  vmeBusUnlock();
#endif

#ifdef DEBUGFW
  printf("\n A24 memory map is set back to its default \n");
#endif
}


#ifndef VXWORKS
static void
vldFirmwareUsage()
{
  printf("\n");
  printf("%s <VME Address (A24)> <firmware svf file>\n",programName);
  printf("\n");

}
#endif
