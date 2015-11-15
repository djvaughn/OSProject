/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 */

#include <phase1.h>
#include <phase2.h>
#include <libuser.h>
#include <usyscall.h>
#include <usloss.h>

#define CHECKMODE {						\
	if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { 				\
	    USLOSS_Console("Trying to invoke syscall from kernel\n");	\
	    USLOSS_Halt(1);						\
	}							\
}

/*
 *  Routine:  Spawn
 *
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *		  PFV func      -- pointer to the function to fork
 *		  void *arg	-- argument to function
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                int  *pid      -- pointer to output value
 *                (output value: process id of the forked process)
 *
 *  Return Value: 0 means success, -1 means error occurs
 */
int Spawn(char *name, int (*func)(char *), char *arg, int stack_size,
	int priority, int *pid)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SPAWN;
    sysArg.arg1 = (void *) func;
    sysArg.arg2 = arg;
    sysArg.arg3 = (void *) ( (long) stack_size);
    sysArg.arg4 = (void *) ( (long) priority);
    sysArg.arg5 = name;
    USLOSS_Syscall(&sysArg);
    *pid = (long) sysArg.arg1;
    return (long) sysArg.arg4;
} /* end of Spawn */


/*
 *  Routine:  Wait
 *
 *  Description: This is the call entry to wait for a child completion
 *
 *  Arguments:    int *pid -- pointer to output value 1
 *                (output value 1: process id of the completing child)
 *                int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 */
int Wait(int *pid, int *status)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_WAIT;
    USLOSS_Syscall(&sysArg);
    *pid = (long) sysArg.arg1;
    *status = (long) sysArg.arg2;
    return (long) sysArg.arg4;

} /* End of Wait */


/*
 *  Routine:  Terminate
 *
 *  Description: This is the call entry to terminate
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Return Value: 0 means success, -1 means error occurs
 */
void Terminate(int status)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_TERMINATE;
    sysArg.arg1 = (void *) ( (long) status);
    USLOSS_Syscall(&sysArg);
    return;

} /* End of Terminate */


/*
 *  Routine:  SemCreate
 *
 *  Description: Create a semaphore.
 *
 *  Arguments:    int value -- initial semaphore value
 *		  int *semaphore -- semaphore handle
 *                (output value: completion status)
 */
int SemCreate(int value, int *semaphore)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMCREATE;
    sysArg.arg1 = (void *) ( (long) value);
    USLOSS_Syscall(&sysArg);
    *semaphore = (long) sysArg.arg1;
    return (long) sysArg.arg4;
} /* end of SemCreate */


/*
 *  Routine:  SemP
 *
 *  Description: "P" a semaphore.
 *
 *
 *  Arguments:    int semaphore -- semaphore handle
 *                (output value: completion status)
 *
 */
int SemP(int semaphore)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMP;
    sysArg.arg1 = (void *) ( (long) semaphore);
    USLOSS_Syscall(&sysArg);
    return (long) sysArg.arg4;
} /* end of SemP */


/*
 *  Routine:  SemV
 *
 *  Description: "V" a semaphore.
 *
 *
 *  Arguments:    int semaphore -- semaphore handle
 *                (output value: completion status)
 *
 */
int SemV(int semaphore)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMV;
    sysArg.arg1 = (void *) ( (long) semaphore);
    USLOSS_Syscall(&sysArg);
    return (long) sysArg.arg4;
} /* end of SemV */


/*
 *  Routine:  SemFree
 *
 *  Description: Free a semaphore.
 *
 *
 *  Arguments:    int semaphore -- semaphore handle
 *                (output value: completion status)
 *
 */
int SemFree(int semaphore)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMFREE;
    sysArg.arg1 = (void *) ( (long) semaphore);
    USLOSS_Syscall(&sysArg);
    return (long) sysArg.arg4;
} /* end of SemFree */


/*
 *  Routine:  GetTimeofDay
 *
 *  Description: This is the call entry point for getting the time of day.
 *
 *  Arguments:    int *tod  -- pointer to output value
 *                (output value: the time of day)
 *
 */
void GetTimeofDay(int *tod)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_GETTIMEOFDAY;
    USLOSS_Syscall(&sysArg);
    *tod = (long) sysArg.arg1;
    return;
} /* end of GetTimeofDay */


/*
 *  Routine:  CPUTime
 *
 *  Description: This is the call entry point for the process' CPU time.
 *
 *
 *  Arguments:    int *cpu  -- pointer to output value
 *                (output value: the CPU time of the process)
 *
 */
void CPUTime(int *cpu)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_CPUTIME;
    USLOSS_Syscall(&sysArg);
    *cpu = (long) sysArg.arg1;
    return;
} /* end of CPUTime */


/*
 *  Routine:  GetPID
 *
 *  Description: This is the call entry point for the process' PID.
 *
 *
 *  Arguments:    int *pid  -- pointer to output value
 *                (output value: the PID)
 *
 */
void GetPID(int *pid)
{
    systemArgs sysArg;

    CHECKMODE;
    sysArg.number = SYS_GETPID;
    USLOSS_Syscall(&sysArg);
    *pid = (long) sysArg.arg1;
    return;
} /* end of GetPID */

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

/* end libuser.c */

