/*
 * File:
 *    vldStatus
 *
 * Description:
 *    show status of VME LED Driver module and library
 *
 *
 */


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "jvme.h"
#include "vldLib.h"

int32_t
main(int32_t argc, char *argv[])
{

  int32_t stat;
  uint32_t address=0;

  if (argc > 1)
    {
      address = (uint32_t) strtoll(argv[1],NULL,16)&0xffffffff;
    }
  else
    {
      address = 7; // my test module
    }

  printf("\n %s: address = 0x%08x\n", argv[0], address);
  printf("----------------------------\n");

  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    goto CLOSE;

  vmeCheckMutexHealth(1);
  vmeBusLock();

  vldInit(address<<19, 0, 1, 0);
  vldGStatus(1);

 CLOSE:

  vmeBusUnlock();

  vmeClearException(1);

  stat = vmeCloseDefaultWindows();
  if (stat != OK)
    {
      printf("vmeCloseDefaultWindows failed: code 0x%08x\n",stat);
      return -1;
    }

  exit(0);
}

/*
  Local Variables:
  compile-command: "make -k vldStatus"
  End:
 */
