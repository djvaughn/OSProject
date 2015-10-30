#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include "message.h"

extern int debugflag2;

/* an error method to handle invalid syscalls */
void nullsys(sysargs *args)
{
  USLOSS_Console("nullsys(): Invalid syscall. Halting...\n");
  USLOSS_Halt(1);
} /* nullsys */


void clockHandler2(int dev, void *arg)
{

  if (DEBUG2 && debugflag2)
    USLOSS_Console("clockHandler2(): called\n");
  int status = -1;
  if (USLOSS_DeviceInput(dev, unit, &status) == USLOSS_DEV_OK)
  {
    MboxCondSend(clockMailBox[unit], &status, 1);
  }
  timeSlice();
} /* clockHandler */


void diskHandler(int dev, void *arg)
{

  long unit = (long) arg;
  if (DEBUG2 && debugflag2)
    USLOSS_Console("diskHandler(): called\n");
  int status = -1;
  if (USLOSS_DeviceInput(dev, unit, &status) == USLOSS_DEV_OK)
  {
    MboxCondSend(diskMailBox[unit], &status, 1);
  }

} /* diskHandler */


void termHandler(int dev, void *arg)
{
  long unit = (long) arg;
  if (DEBUG2 && debugflag2)
    USLOSS_Console("termHandler(): called\n");
  int status = -1;
  if (USLOSS_DeviceInput(dev, unit, &status) == USLOSS_DEV_OK)
  {
    MboxCondSend(termMailBox[unit], &status, 1);
  }

} /* termHandler */


void syscallHandler(int dev, void *arg)
{

  long unit = (long) arg;
  if (DEBUG2 && debugflag2)
    USLOSS_Console("syscallHandler(): called\n");

  systemArgs *sysPtr;
  sysPtr = (systemArgs *) unit;
  if (dev != USLOSS_SYSCALL_INT)
  {
    USLOSS_Console("syscall_handler(): called w/ bad device number %d. Halting...\n",
                   dev);
    USLOSS_Halt(1);
  }
  if (sysPtr->number < 0 || sysPtr->MAXSYSCALLS)
  {
    USLOSS_Console("syscall_handler(): sys number %d is wrong.  Halting...\n",
                   sys_ptr->number);
    USLOSS_Halt(1);
  }
  USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

  sys_vec[sysPtr->number](sysPtr);

} /* syscallHandler */
