/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 */

#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <provided_prototypes.h>
#include <usyscall.h>
#include <usloss.h>

#define CHECKMODE {                     \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) {                \
        USLOSS_Console("Trying to invoke syscall from kernel\n");   \
        USLOSS_Halt(1);                     \
    }                           \
}

/* ------------------------------------------------------------------------
Name - Sleep
Purpose - user mode process to delay a process
Parameters - int sleep: length of sleep time in seconds
Returns - int, -1 for invalid inputs, 0 otherwise
Side Effects - none
----------------------------------------------------------------------- */
int Sleep(int sleep)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *)((long) sleep);
    USLOSS_Syscall(&sysArg);
    return (long) sysArg.arg4;
} /* end of Sleep */


/* ------------------------------------------------------------------------
Name - DiskRead
Purpose - User mode process to read from the disk
Parameters -    diskBuffer = pointer to buffer for read info
                unit = int, unit number of disk to read
                track = int, starting disk track number
                first = int, starting disk sector number
                sectors = int, number of sectors to read
Returns - int, -1 for invalid input, 0 otherwise
Side Effects - none
----------------------------------------------------------------------- */
int DiskRead (void *diskBuffer, int unit, int track, int first,
              int sectors, int *status)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = diskBuffer;
    sysArg.arg2 = (void *)((long) sectors);
    sysArg.arg3 = (void *)((long) track);
    sysArg.arg4 = (void *)((long) first);
    sysArg.arg5 = (void *)((long) unit);
    USLOSS_Syscall(&sysArg);
    *status = (long)sysArg.arg1;
    return (long)sysArg.arg4;

} /* End of Wait */

/* ------------------------------------------------------------------------
Name - DiskWrite
Purpose - User mode process to write to the disk
Parameters -    diskBuffer = int, pointer to buffer for read info
                unit = int, unit number of disk to read
                track = int, starting disk track number
                first = int, starting disk sector number
                sectors = int, number of sectors to read
Returns - int, -1 for invalid input, 0 otherwise
Side Effects - disk changed
----------------------------------------------------------------------- */
int DiskWrite(void *diskBuffer, int unit, int track, int first,
              int sectors, int *status)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = diskBuffer;
    sysArg.arg2 = (void *)((long) sectors);
    sysArg.arg3 = (void *)((long) track);
    sysArg.arg4 = (void *)((long) first);
    sysArg.arg5 = (void *)((long) unit);
    USLOSS_Syscall(&sysArg);
    *status = (long)sysArg.arg1;
    return (long)sysArg.arg4;

}

/* ------------------------------------------------------------------------
Name - DiskSize
Purpose - User mode process for getting the size of the disk
Parameters -    unit = int, unit number of disk
                sector = int pointer, for number of bytes in sector
                track = int pointer, for number of sectors in track
                disk = int pointer, for number of tracks in disk
Returns - int, -1 for invalid input, 0 otherwise
Side Effects - none
----------------------------------------------------------------------- */
int DiskSize (int unit, int *sector, int *track, int *disk)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) ( (long) unit);
    USLOSS_Syscall(&sysArg);
    *sector = (long)sysArg.arg1;
    *track = (long)sysArg.arg2;
    *disk = (long)sysArg.arg3;
    return (long)sysArg.arg4;

}


/* ------------------------------------------------------------------------
Name - TermRead
Purpose - user mode process to read from a terminal
Parameters -    buffer = char pointer for storing read chars
                bufferSize = max number of characters in buffer
                unitID = terminal unit number
                numCharsRead = int pointer for number of chars read
Returns - int, -1 for invalid input, 0 otherwise
Side Effects - none
----------------------------------------------------------------------- */
int TermRead(char *buffer, int bufferSize, int unitID,
             int *numCharsRead)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = (void *)((long)buffer);
    sysArg.arg2 = (void *)((long)bufferSize);
    sysArg.arg3 = (void *)((long)unitID);
    USLOSS_Syscall(&sysArg);
    *numCharsRead = (long) sysArg.arg2;
    return (long) sysArg.arg4;
}


/* ------------------------------------------------------------------------
Name - TermWrite
Purpose - user mode process to write to a terminal
Parameters -    buffer = char pointer for storing read chars
                bufferSize = max number of characters in buffer
                unitID = terminal unit number
                numCharsRead = int pointer for number of chars read
Returns - -1 for invalid input, 0 otherwise
Side Effects - terminal changed
----------------------------------------------------------------------- */
int TermWrite(char *buffer, int bufferSize, int unitID,
              int *numCharsRead)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_TERMWRITE;
    sysArg.arg1 = (void *)((long)buffer);
    sysArg.arg2 = (void *)((long)bufferSize);
    sysArg.arg3 = (void *)((long)unitID);
    USLOSS_Syscall(&sysArg);
    *numCharsRead = (long) sysArg.arg2;
    return (long) sysArg.arg4;
}