/*----------------------------------------------------------------------------*
 *  Copyright (c) 2022        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Firmware update for the Jefferson Lab VME LED Driver (VLD)
 *
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jvme.h"
#include "vldLib.h"


#ifdef VXWORKS
extern int sysBusToLocalAdrs(int, char *, char **);
extern STATUS vxMemProbe(char *, int, int, char *);
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
extern uint16_t vldFWVers[MAX_VME_SLOTS+1];
typedef volatile unsigned int * vuintptr_t;
vuintptr_t eJTAGLoad;
unsigned int firmwareInfo;
char *programName;

static void SwitchAM(int enable_modified);
static void FirmwareEMload(char *filename);
#ifndef VXWORKS
static void FirmwareUsage();
#endif

int
#ifdef VXWORKS
vldFirmwareUpdate(unsigned int arg_vmeAddr, char *arg_filename)
#else
main(int argc, char *argv[])
#endif
{
  int stat = 0, badInit = 0;
  char *filename;
  int inputchar = 10;
  unsigned int vme_addr = 0;
  unsigned long laddr = 0;
  int geo = 0;

  printf("\nVLD firmware update via VME\n");
  printf("----------------------------\n");

#ifdef VXWORKS
  programName = __FUNCTION__;

  vme_addr = arg_vmeAddr;
  filename = arg_filename;
#else
  programName = argv[0];

  if (argc < 3)
    {
      printf(" ERROR: Must specify two arguments\n");
      FirmwareUsage();
      return (-1);
    }
  else
    {
      vme_addr = (unsigned int) strtoll(argv[1], NULL, 16) & 0xffffffff;
      if (vme_addr <= 21)
	vme_addr = vme_addr << 19;
      filename = argv[2];
    }

  vmeSetQuietFlag(1);
  stat = vmeOpenDefaultWindows();
  if (stat != OK)
    goto CLOSE;

#endif

  stat = vldInit(vme_addr, 0, 1, 0);
  if (stat != OK)
    {
      printf("\n");
      printf("*** Failed to initialize VLD ***\nThis may indicate (either):\n");
      printf("   a) an incorrect VME Address provided\n");
      printf("   b) VLD is unresponsive\n");
      printf("   c) VLD has incorrect firmware loaded\n");
      printf("\n");
      printf("Proceed with the update with the provided VME address (0x%x)?\n",
	 vme_addr);
    REPEAT:
      printf(" (y/n): ");
      inputchar = getchar();

      if ((inputchar == 'n') || (inputchar == 'N'))
	{
	  printf("--- Exiting without update ---\n");
	  goto CLOSE;
	}
      else if ((inputchar == 'y') || (inputchar == 'Y'))
	{
	  printf("--- Continuing update, assuming VME address (0x%x) is correct ---\n",
	     vme_addr);
	  printf("\n");
	  badInit = 1;
	}
      else
	{
	  goto REPEAT;
	}
    }

  if (badInit == 0)
    {
      firmwareInfo = vldFWVers[vldSlot(0)];
      if (firmwareInfo > 0)
	{
	  printf("\n  Board Firmware = 0x%x\n", firmwareInfo);
	  printf("\n");
	}
      else
	{
	  printf("  Error reading Firmware Version\n");
	}
    }

  printf("Press y to load firmware (%s) to the TI via VME...\n", filename);
  printf("\t or n to quit without update\n");

REPEAT2:
  printf("(y/n): ");
  inputchar = getchar();

  if ((inputchar == 'n') || (inputchar == 'N'))
    {
      printf("--- Exiting without update ---\n");
      goto CLOSE;
    }
  else if ((inputchar == 'y') || (inputchar == 'Y'))
    {
    }
  else
    goto REPEAT2;

  /* Check to see if the vld is in a VME-64X crate or Trying to recover corrupted firmware */
  if (badInit == 0)
    geo = vldGetGeoAddress(0);
  else
    geo = -1;

  if (geo <= 0)
    {
      if (geo == 0)
	{
	  printf("  ...Detected non VME-64X crate...\n");

	  /* Need to reset the Address to 0 to communicate with the emergency loading AM */
	  vme_addr = 0;
	}

#ifdef VXWORKS
      stat = sysBusToLocalAdrs(0x39, (char *) vme_addr, (char **) &laddr);
      if (stat != 0)
	{
	  printf("%s: ERROR: Error in sysBusToLocalAdrs res=%d \n",
		 __FUNCTION__, stat);
	  goto CLOSE;
	}
#else
      stat =
	vmeBusToLocalAdrs(0x39, (char *) (unsigned long) vme_addr,
			  (char **) &laddr);
      if (stat != 0)
	{
	  printf("%s: ERROR: Error in vmeBusToLocalAdrs res=%d \n",
		 __FUNCTION__, stat);
	  goto CLOSE;
	}
#endif
    }
  else
    {
      laddr = (unsigned long)VLDp[vldSlot(0)];
    }
  eJTAGLoad = (vuintptr_t) (laddr + 0xFFFC);

  FirmwareEMload(filename);

CLOSE:

#ifndef VXWORKS
  vmeCloseDefaultWindows();
#endif
  printf("\n");

  return OK;
}

static void
SwitchAM(int enable_modified)
{
  if(enable_modified)
    {
#ifdef VXWORKS
#ifdef TEMPE
      sysTempeSetAM(2, 0x19);
#else /* Universe */
      sysUnivSetUserAM(0x19, 0);
      sysUnivSetLSI(2, 6);
#endif /*TEMPE*/
#else
      vmeBusLock();
      vmeSetA24AM(0x19);
#endif

#ifdef DEBUGFW
      printf("%s: A24 memory map is set to AM = 0x19 \n", __FUNCTION__);
#endif
    }
  else
    {
#ifdef VXWORKS
#ifdef TEMPE
      sysTempeSetAM(2, 0);
#else
      sysUnivSetLSI(2, 1);
#endif /*TEMPE*/
#else
      vmeSetA24AM(0);
      vmeBusUnlock();
#endif

#ifdef DEBUGFW
  printf("\n A24 memory map is set back to its default \n");
#endif
    }
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
      printf("Time base: %x , next call: %x , second call: %x \n", time_0,
	     time_1, time);
#endif
      diff = time - time_0;
    }
  while (diff < delays);
}
#endif

static int
Emergency(unsigned int jtagType, unsigned int numBits, unsigned int *jtagData)
{
  unsigned int iloop, iword, ibit;
  unsigned int shData;
  int rval = OK;

/* #define DEBUG */
#ifdef DEBUG
  static int times = 0;
  if((jtagType == 2) && (times < 20 ))
    {
      int numWord, i;
      times++;
      printf("type: %x, num of Bits: %x, data: \n", jtagType, numBits);
#define LOOK2
#ifdef LOOK1
      for (iloop = 0; iloop < numBits; iloop++)
	{
	  iword = iloop / 32;
	  ibit = iloop % 32;
	  shData = ((jtagData[iword] >> ibit) << 1) & 0x2;
	  if (iloop == numBits - 1)
	    shData = shData + 1;	//set the TMS high for last bit to exit Shift_DR
	  if(iloop < (32*40))
	    {
	      printf("%08x: %08x\n",
		     iloop, shData);
	    }
	}
#endif
#ifdef LOOK2
      numWord = numBits ? ((numBits - 1) / 32 + 1) : 0;
      for (i = 0; i < numWord; i++)
	{
	  if(i < 4)
	    {
	      printf("%d: %x\n", i, jtagData[numWord - i - 1]);
	      fflush(stdout);
	    }
	}
      printf("\n");
#endif
      return OK;
    }
  else
    {
      return OK;
    }
#endif

  if (jtagType == 0)		//JTAG reset, TMS high for 5 clcoks, and low for 1 clock;
    {
      for (iloop = 0; iloop < 5; iloop++)
	{
	  vmeWrite32((vuintptr_t)eJTAGLoad, 1);
	}

      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
    }
  else if (jtagType == 1)	// JTAG instruction shift
    {
      // Shift_IR header:
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);

      for (iloop = 0; iloop < numBits; iloop++)
	{
	  iword = iloop / 32;
	  ibit = iloop % 32;
	  shData = ((jtagData[iword] >> ibit) << 1) & 0x2;
	  if (iloop == numBits - 1)
	    shData = shData + 1;	//set the TMS high for last bit to exit Shift_IR
	  vmeWrite32((vuintptr_t)eJTAGLoad, shData);
	}

      // shift _IR tail
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
    }
  else if (jtagType == 2)	// JTAG data shift
    {
      //shift_DR header
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);

      for (iloop = 0; iloop < numBits; iloop++)
	{
	  iword = iloop / 32;
	  ibit = iloop % 32;
	  shData = ((jtagData[iword] >> ibit) << 1) & 0x2;
	  if (iloop == numBits - 1)
	    shData = shData + 1;	//set the TMS high for last bit to exit Shift_DR
	  vmeWrite32((vuintptr_t)eJTAGLoad, shData);
	}

      // shift _DR tail
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);	// update Data_Register
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);	// back to the Run_test/Idle
    }
  else if (jtagType == 3)	// JTAG instruction shift, stop at IR-PAUSE state, though, it started from IDLE
    {
      // Shift_IR header:
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);

      for (iloop = 0; iloop < numBits; iloop++)
	{
	  iword = iloop / 32;
	  ibit = iloop % 32;
	  shData = ((jtagData[iword] >> ibit) << 1) & 0x2;
	  if (iloop == numBits - 1)
	    shData = shData + 1;	//set the TMS high for last bit to exit Shift_IR
	  vmeWrite32((vuintptr_t)eJTAGLoad, shData);
	}

      // shift _IR tail
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);	// update instruction register
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);	// back to the Run_test/Idle
    }
  else if (jtagType == 4)	// JTAG data shift, start from IR-PAUSE, end at IDLE
    {
      //shift_DR header
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);	//to EXIT2_IR
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);	//to UPDATE_IR
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);	//to SELECT-DR_SCAN
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);

      for (iloop = 0; iloop < numBits; iloop++)
	{
	  iword = iloop / 32;
	  ibit = iloop % 32;
	  shData = ((jtagData[iword] >> ibit) << 1) & 0x2;
	  if (iloop == numBits - 1)
	    shData = shData + 1;	//set the TMS high for last bit to exit Shift_DR
	  vmeWrite32((vuintptr_t)eJTAGLoad, shData);
	}

      // shift _DR tail
      vmeWrite32((vuintptr_t)eJTAGLoad, 1);	// update Data_Register
      vmeWrite32((vuintptr_t)eJTAGLoad, 0);	// back to the Run_test/Idle
    }
  else if (jtagType == 5)  // JTAG RUNTEST
    {
      //      printf(" real RUNTEST delay %d \n", numBits);
      for (iloop =0; iloop <numBits; iloop++)
	{
	  vmeWrite32((vuintptr_t)eJTAGLoad, 0); // Shift TMS=0, TDI=0
	  //	  cpuDelay(100);
	}
    }
  else
    {
      printf("\n JTAG type %d unrecognized \n", jtagType);
    }

  return rval;
}

static void
Parse(char *buf, unsigned int *Count, char **Word)
{
  *Word = buf;
  *Count = 0;
  while (*buf != '\0')
    {
      while ((*buf == ' ') || (*buf == '\t') || (*buf == '\n')
	     || (*buf == '"'))
	*(buf++) = '\0';
      if ((*buf != '\n') && (*buf != '\0'))
	{
	  Word[(*Count)++] = buf;
	}
      while ((*buf != ' ') && (*buf != '\0') && (*buf != '\n')
	     && (*buf != '\t') && (*buf != '"'))
	{
	  buf++;
	}
    }
  *buf = '\0';
}

static void
FirmwareEMload(char *filename)
{
  unsigned int lineRead;
  FILE *svfFile;
  char bufRead[256];
  unsigned int sndData[256];
  char *Word[16], *lastn;
  unsigned int nbits, nMiddle, nbytes, extrType, i, j, Count, nWords, nlines = 0;
  unsigned int nWordsP, nWordsF;	// nWords: number of bytes per line without SDR etc
  // nWordsP: number of bytes for first line;
  // nWordsF: number of bytes for last line;
  // nWordsA: number of total bytes
  int shiftData_size = 64;
  unsigned int *ShiftData;
  int shiftChar_size = 10000000;
#ifdef VXWORKS
  unsigned int nWordsA;
  unsigned int RegAdd;
  char tempchar;
#endif
  char *ShiftChar;
  unsigned int nlongwait = 0, ilongwait = 0;
  unsigned int longwait_threshold = 100000;
#ifdef DEBUG
  static int done1=0, done2=0;
#endif

  ShiftData =
    (unsigned int *) malloc(shiftData_size * sizeof(unsigned int));

  if (ShiftData <= 0)
    {
      printf(" Error Allocating memory for ShiftData \n");
      return;
    }

  //A24 Address modifier redefined
  SwitchAM(1);

  //open the file:
  svfFile = fopen(filename, "r");
  if (svfFile == NULL)
    {
      perror("fopen");
      printf("%s: ERROR: Unable to open file %s\n", __FUNCTION__, filename);

      // A24 address modifier reset
      SwitchAM(0);
      return;
    }

#ifdef DEBUGFW
  printf("\n File is open \n");
#endif

  //PROM JTAG reset/Idle
  Emergency(0, 0, ShiftData);
#ifdef DEBUGFW
  printf("%s: Emergency PROM JTAG reset IDLE \n", __FUNCTION__);
#endif
  taskDelay(1);

  //Another PROM JTAG reset/Idle
  Emergency(0, 0, ShiftData);
#ifdef DEBUGFW
  printf("%s: Emergency PROM JTAG reset IDLE \n", __FUNCTION__);
#endif
  taskDelay(1);


  //initialization
  extrType = 0;
  lineRead = 0;

  printf("\n");
  fflush(stdout);

  /* Count the total number of lines, and number of RUNTEST 6000000 */
  while (fgets(bufRead, 256, svfFile) != NULL)
    {
      nlines++;

      Parse(bufRead, &Count, &(Word[0]));
      if(strcmp(Word[0], "RUNTEST") == 0)
  	{
  	  sscanf(Word[1], "%d", &nbits);
  	  if(nbits > longwait_threshold)
  	    nlongwait++;
  	}
    }

  rewind(svfFile);

  while (fgets(bufRead, 256, svfFile) != NULL)
    {
      lineRead += 1;

      if ((lineRead % ((int) (nlines / 40))) == 0)
	{
#ifdef VXWORKS
	  /* This is pretty filthy... but at least shows some output when it's updating */
	  printf("     ");
	  printf("\b\b\b\b\b");
#endif
	  printf(".");
	  fflush(stdout);
	}

      if (((bufRead[0] == '/') && (bufRead[1] == '/')) || (bufRead[0] == '!'))
	{
	  //    printf(" comment lines: %c%c \n",bufRead[0],bufRead[1]);
	}
      else
	{
	  // begin to parse the data bufRead
	  Parse(bufRead, &Count, &(Word[0]));
	  if (strcmp(Word[0], "SDR") == 0)
	    {
	      sscanf(Word[1], "%d", &nbits);
	      nbytes = (nbits - 1) / 8 + 1;
	      if (strcmp(Word[2], "TDI") == 0)
		{
		  if (nbytes < 120)
		    {
		      for (i = 0; i < nbytes; i++)
			{
			  sscanf(&Word[3][2 * (nbytes - i) - 1], "%2x",
			  	 &sndData[i]);
#ifdef DEBUG
			  if ((i < 4) && (done1 == 0) && (sndData[i] != 0))
			    {
			      printf("%d: Word: %c%c, data: %x \n",
				     lineRead,
			  	     Word[3][2*(nWordsP-i)-1],
			  	     Word[3][2*(nWordsP-i)],
			  	     sndData[i]);
			      done1++;
			    }
#endif
			}
		      nWords = (nbits - 1) / 32 + 1;
		      for (i = 0; i < nWords; i++)
			{
			  ShiftData[i] =
			    ((sndData[i * 4 + 3] << 24) & 0xff000000) +
			    ((sndData[i * 4 + 2] << 16) & 0xff0000) +
			    ((sndData[i * 4 + 1] << 8) & 0xff00) +
			    (sndData[i * 4] & 0xff);
			}

		      Emergency(2 + extrType, nbits, ShiftData);
		    }
		  else		// deal with the FPGA loading size more than one line: 77845248, 77845248 = 304083*4*8*8
		    {
		      ShiftChar =
			(char *) malloc(shiftChar_size * sizeof(char));
		      if (ShiftChar <= 0)
			{
			  printf(" Error Allocating memory for ShiftChar \n");

			  SwitchAM(0);
			  return;
			}

		      if (nbytes < 160)
			{
			  nWordsP = 116;
			  nWordsF = 40;
			}
		      else if (nbytes < 300)
			{
			  nWordsP = 116;
			  nWordsF = 35;
			}
		      else if (nbytes < 1200000)	// for the special firmware by v14.7
			{
			  nWordsP = 115;
			  nWordsF = 6;
			}
		      else if (nbytes < 1300000)	// for the special firmware by v14.5
			{
			  nWordsP = 115;
			  nWordsF = 67;
			}
		      else	// for the standard FPGA direct loading
			{
			  nWordsP = 114;
			  nWordsF = 12;
			}

		      for (i = 0; i < nWordsP; i++)
			{
			  sscanf(&Word[3][2 * (nWordsP - i) - 1], "%2x",
				 &sndData[i]);
#ifdef DEBUG
			  if ((i < 4) && (done2 == 0) && (sndData[i] != 0))
			    {
			      printf("%d: Word: %c%c, data: %x \n",
				     lineRead,
			  	     Word[3][2*(nWordsP-i)-1],
			  	     Word[3][2*(nWordsP-i)],
			  	     sndData[i]);
			      done2++;
			    }
#endif
			}

		      for (i = 0; i < nWordsP; i++)
			{
			  ShiftChar[nbytes - 1 - i] =
			    (sndData[nWordsP - 1 - i] & 0xff);
			}

		      nWords = 123;
		      nMiddle = (nbytes - nWordsP) / nWords;
		      if (nMiddle > 0)
			{
			  for (i = 0; i < nMiddle; i++)
			    {

			      if (fgets(bufRead, 256, svfFile) == NULL)
				{
				  printf
				    ("\n \n  !!! End of file Reached !!! \n \n");

				  // A24 address modifier reset
				  SwitchAM(0);
				  return;
				}
			      for (j = 0; j < nWords; j++)
				{
				  sscanf(&bufRead[j * 2], "%2x",
					 &sndData[nWords - 1 - j]);

				}

			      for (j = 0; j < nWords; j++)
				{
				  ShiftChar[nbytes - nWordsP - 1 -
					    i * nWords - j] =
				    (sndData[nWords - 1 - j] & 0xff);

				}
			      fflush(stdout);
			      lastn = strrchr(bufRead, '\n');
			      if (lastn != 0)
				lastn[0] = '\0';
			    }
			}
		      // Read the remainng bytes
		      if (fgets(bufRead, 256, svfFile) == NULL)
			{
			  printf("\n \n  !!! End of file Reached !!! \n \n");

			  // A24 address modifier reset
			  SwitchAM(0);
			  return;
			}

		      for (j = 0; j < nWordsF; j++)
			{
			  sscanf(&bufRead[2 * j], "%2x",
				 &sndData[nWordsF - 1 - j]);

			}

		      for (j = 0; j < nWordsF; j++)
			{
			  ShiftChar[nWordsF - 1 - j] =
			    (sndData[nWordsF - 1 - j] & 0xff);

			}

		      fflush(stdout);
		      // re-arrange the endian to call Emergency()
#ifdef VXWORKS
		      nWordsA = (nbits - 1) / 32 + 1;
		      for (j = 0; j < nWordsA; j++)
			{
			  tempchar = ShiftChar[4 * j];
			  ShiftChar[4 * j] = ShiftChar[4 * j + 3];
			  ShiftChar[4 * j + 3] = tempchar;
			  tempchar = ShiftChar[4 * j + 1];
			  ShiftChar[4 * j + 1] = ShiftChar[4 * j + 2];
			  ShiftChar[4 * j + 2] = tempchar;
			}
#endif
		      Emergency(2 + extrType, nbits, (unsigned int *)ShiftChar);

		      free(ShiftChar);

		    }
		}
	    }
	  else if (strcmp(Word[0], "SIR") == 0)
	    {
	      sscanf(Word[1], "%d", &nbits);
	      nbytes = (nbits - 1) / 8 + 1;
	      if (strcmp(Word[2], "TDI") == 0)
		{
		  for (i = 0; i < nbytes; i++)
		    {
		      sscanf(&Word[3][2 * (nbytes - i) - 1], "%2x",
			     &sndData[i]);

		    }
		  nWords = (nbits - 1) / 32 + 1;
		  for (i = 0; i < nWords; i++)
		    {
		      ShiftData[i] =
			((sndData[i * 4 + 3] << 24) & 0xff000000) +
			((sndData[i * 4 + 2] << 16) & 0xff0000) +
			((sndData[i * 4 + 1] << 8) & 0xff00) +
			(sndData[i * 4] & 0xff);
		    }

		  Emergency(1 + extrType, nbits, ShiftData);
		}
	    }
	  else if (strcmp(Word[0], "RUNTEST") == 0)
	    {
	      sscanf(Word[1], "%d", &nbits);

	      if (nbits > longwait_threshold)
		{
		  if(ilongwait == 0)
		    {
		      printf("          ");
		      for(i = 0; i < nlongwait; i++)
			{
			  if((i % ((int) ( nlongwait / 40))) == 0)
			    {
			      printf("-");
			    }
			}

		      printf("\nErasing:  ");
		      fflush(stdout);
		    }
		  else
		    {
		      if((ilongwait % ((int) ( nlongwait / 40))) == 0)
			{
			  printf(".");
			  fflush(stdout);
			}
		    }

		  ilongwait++;

		  if(ilongwait==nlongwait)
		    {
		      printf(".Done\n\n");
		      fflush(stdout);
		      printf("          ----------------------------------------\n");
		      printf("Updating: ");
		      fflush(stdout);
		    }
		}
	      Emergency(5, nbits * 2, ShiftData);	// Jtag clock at RUNTEST/IDLE

	    }
	  else if (strcmp(Word[0], "STATE") == 0)
	    {
	      if (strcmp(Word[1], "RESET") == 0)
		{
		  Emergency(0, 0, ShiftData);
		  sleep(1);
		}
	    }
	  else if (strcmp(Word[0], "ENDIR") == 0)
	    {
	      if (strncmp(Word[1], "IDLE", 4) == 0)
		{
		  extrType = 0;
#ifdef DEBUGFW
		  printf(" ExtraType: %d \n", extrType);
#endif
		}
	      else if (strncmp(Word[1], "IRPAUSE", 7) == 0)
		{
		  extrType = 2;
#ifdef DEBUGFW
		  printf(" ExtraType: %d \n", extrType);
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
	      printf(" Command type ignored: %s \n", Word[0]);
#endif
	    }

	}			//end of if (comment statement)
    }				//end of while

  printf("Done\n");

  printf("**************************************\n");
  printf("**     Firmware Update Complete     **\n");
  printf("** Power Cycle to load new firmware **\n");
  printf("**************************************\n\n");

  //close the file
  fclose(svfFile);

  // A24 address modifier reset
  SwitchAM(0);
}


#ifndef VXWORKS
static void
FirmwareUsage()
{
  printf("\n");
  printf("%s <VME Address (A24)> <firmware svf file>\n", programName);
  printf("\n");

}
#endif
