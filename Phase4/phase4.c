/*
Phase 4.c
@authors: Daniel Vaughn and Meg Dever Hanson
*/
#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase4.h>
#include <provided_prototypes.h>
#include <stdlib.h> /* needed for atoi() */
#include <stdio.h>
#include <string.h>
#include "driver.h"

/* ------------------------- Prototypes ----------------------------------- */
static int  ClockDriver(char *);
static int  DiskDriver(char *);
static int  TermDriver(char *);
void start3(void);
int sleepReal(int seconds);
int  diskReadReal (int unit, int track, int first,
                   int sectors, void *diskBuffer);
int  diskWriteReal(int unit, int track, int first,
                   int sectors, void *diskBuffer);
int  diskSizeReal (int unit, int *sector, int *track, int *disk);
int  termReadReal (int unitID, int bufferSize, char *buffer);
int  termWriteReal(int unitID, int bufferSize, char *buffer);
driverProcPtr removeProcess(void);
void intializeProcStruct(void);
void initializeSysVec(void);
void sleep(systemArgs *sysArg);
void diskread(systemArgs *sysArg);
void diskwrite(systemArgs *sysArg);
void disksize(systemArgs *sysArg);
void termread(systemArgs *sysArg);
void termwrite(systemArgs *sysArg);
void insertAfter(driverProcPtr currentProcess, driverProcPtr newProcess);
driverProcPtr removeDiskQ(int unit);
void addDiskQ (int unit, driverProcPtr newProcess);
static int  TermWriter(char *arg);
static int  TermReader(char *arg);


/* -------------------------- Globals ------------------------------------- */
semaphore running;
semaphore diskSem[USLOSS_DISK_UNITS];
semaphore termSem[USLOSS_TERM_UNITS];
semaphore termWriteRealSem[USLOSS_TERM_UNITS];
/* Mailboxes for the the Terminal */
//The Kill Boxes
int     termKillerReader[USLOSS_TERM_UNITS];
int     termKillerWriter[USLOSS_TERM_UNITS];
int     termKillerDriver[USLOSS_TERM_UNITS];
//The Send Boxes
int     termReaderMBoxs[USLOSS_TERM_UNITS];
int     termWriterMBboxs[USLOSS_TERM_UNITS];
int     termWriteRealMBoxs[USLOSS_TERM_UNITS];
int     termReadRealMBox[USLOSS_TERM_UNITS];
int     termDriverMBox[USLOSS_TERM_UNITS];
/* Process Table */
driverProcStruct driverProcTable[MAXPROC];


driverProcPtr diskUnit[USLOSS_DISK_UNITS];
int termMBoxs[20];

/* TermReader Interrupts Array */
int termReaderInts[USLOSS_TERM_UNITS];

/* Queue of processes asleep */
driverProcPtr sleepingProcessQueue;
int isRunning;

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
Name -  start3 *TODO*
Purpose - initialize all structures, create first user level process, quit
Parameters - none
Returns - none
Side Effects - none
----------------------------------------------------------------------- */

void start3(void)
{
    char    name[128];
    char    buffer[10];
    int     i;
    int     clockPID;
    int     diskPID[USLOSS_DISK_UNITS];
    int     termDriverPID[USLOSS_TERM_UNITS];
    int     termReaderPID[USLOSS_TERM_UNITS];
    int     termWriterPID[USLOSS_TERM_UNITS];
    int     status;
    /*
     * Check kernel mode here.
     */
    CHECKMODE("start3");

    isRunning = 1;


    /*initialize PID list*/
    clockPID = NOTUSEDPID;
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        termDriverPID[i] = NOTUSEDPID;
        termReaderPID[i] = NOTUSEDPID;
        termWriterPID[i] = NOTUSEDPID;


    }

    for (int i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        diskPID[i] = NOTUSEDPID;
    }
    /*Initialize the system vectors*/
    initializeSysVec();

    /*Initialize the Process Table*/
    intializeProcStruct();

    /*Initialize the sleepingProcessQueue*/
    sleepingProcessQueue = NULL;
    /*
     * Create clock device driver
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
        USLOSS_Console("start3(): Can't create clock driver\n");
        USLOSS_Halt(1);
    }
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */
    sempReal(running);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */
    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(buffer, "%d", i);
        sprintf(name, "DiskDriver %d", i);
        diskSem[i] = semcreateReal(0);
        diskPID[i] = fork1(name, DiskDriver, buffer, USLOSS_MIN_STACK, 2);
        if (diskPID[i] < 0) {
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }
    }
    ;
    /*Block start3 till all the diskDrivers have started up and blocked*/
    for (i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        sempReal(running);
    }

    /*
     * Create terminal device drivers. how many terminals are there
     */
    for (i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        termReaderMBoxs[i] = MboxCreate(10, MAX_MESSAGE);
        termWriterMBboxs[i] = MboxCreate(10, MAX_MESSAGE);
        termWriteRealMBoxs[i] = MboxCreate(10, MAX_MESSAGE);
        termReadRealMBox[i] = MboxCreate(10, MAX_MESSAGE);
        termDriverMBox[i] = MboxCreate(10, MAX_MESSAGE);


    }
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        termKillerReader[i] = MboxCreate(1, MAX_MESSAGE);
        termKillerWriter[i] = MboxCreate(1, MAX_MESSAGE);
        termKillerDriver[i] = MboxCreate(1, MAX_MESSAGE);
        termWriteRealSem[i] = semcreateReal(0);
        sprintf(buffer, "%d", i);
        termSem[i] = semcreateReal(0);
        sprintf(name, "TermDriver %d", i);
        termDriverPID[i] = fork1(name, TermDriver, buffer, USLOSS_MIN_STACK, 2);
        if (termDriverPID[i] < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        sempReal(running);

        sprintf(buffer, "%d", i);
        sprintf(name, "TermReader %d", i);
        termReaderPID[i] = fork1(name, TermReader, buffer, USLOSS_MIN_STACK, 2);
        if (termReaderPID[i] < 0) {
            USLOSS_Console("start3(): Can't create TermReader %d\n", i);
            USLOSS_Halt(1);
        }
        sempReal(running);

        sprintf(buffer, "%d", i);
        sprintf(name, "TermWriter %d", i);
        termWriterPID[i] = fork1(name, TermWriter, buffer, USLOSS_MIN_STACK, 2);
        if (termWriterPID[i] < 0) {
            USLOSS_Console("start3(): Can't create TermWriter %d\n", i);
            USLOSS_Halt(1);
        }
        sempReal(running);



    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters.
     */
    spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    waitReal(&status);
    /*Zap all the device drivers*/
    isRunning = 0;
    zap(clockPID);
    join(&status);
    for (int i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        semvReal(diskSem[i]);

    }
    int control = 0;
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {


        MboxSend(termKillerReader[i], NULL, 0);
        MboxSend(termReaderMBoxs[i], NULL, 0);
        zap(termReaderPID[i]);

        MboxSend(termKillerWriter[i], NULL, 0);
        MboxSend(termWriterMBboxs[i], NULL, 0);
        zap(termWriterPID[i]);


        MboxSend(termKillerDriver[i], NULL, 0);
        control = 0;
        control = USLOSS_TERM_CTRL_XMIT_INT(control);
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void*)(long)control);
        zap(termDriverPID[i]);
    }

    // eventually, at the end:
    quit(0);

}

/* ------------------------------------------------------------------------
Name - ClockDriver
Purpose - puts a process to sleep for specified period
Parameters - char pointer arg = not used
Returns - int, 0 for failure, 1 for success
Side Effects - none
----------------------------------------------------------------------- */
static int ClockDriver(char *arg)
{
    int result;
    int status;
    int time = 0;
    driverProcPtr temp;
    driverProcPtr sleepingProcess;

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // Infinite loop until we are zap'd
    while (!isZapped() && isRunning) {
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
            return 0;
        }
        /*
         * Compute the current time and wake up any processes
         * whose time has come.
         */
        gettimeofdayReal(&time);
        if (sleepingProcessQueue != NULL)
        {
            temp = sleepingProcessQueue;
            while (temp != NULL && (time >= (temp->sleepTime))) {
                sleepingProcess = removeProcess();
                MboxSend(sleepingProcess->privateMbox, NULL, 0);
                temp = temp->nextProcessAsleep;
            }
        }
    }
    return 1;
}
/* ------------------------------------------------------------------------
Name -  DiskDriver
Purpose - reading and writing sectors to disk
Parameters - char pointer arg = disk unit
Returns - int -1 for failure, 0 for success
Side Effects -
----------------------------------------------------------------------- */
static int DiskDriver(char *arg)
{
    int result;
    int status;

    int unit = atoi( (char *) arg);     // Unit is passed as arg.

    int disk;
    int sector;
    int track;

    diskSizeReal(unit, &sector, &track, &disk);


    diskUnit[unit] = NULL;
    static int previousSpot = -1;
    semvReal(running);

    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    while (!isZapped() && isRunning) {

        sempReal(diskSem[unit]);

        driverProcPtr nextRW = removeDiskQ(unit);

        if (isZapped() || !isRunning)
        {
            quit(-1);
        }

        if (nextRW != NULL)
        {
            int i;
            int j;
            for ( i = (nextRW->start), j = 0; i <= (nextRW->end); i++, j++)
            {
                int currentTrack = previousSpot / track;


                int requestedTrack = i / track;


                if (currentTrack != requestedTrack)
                {
                    USLOSS_DeviceRequest myRequest;
                    myRequest.opr = USLOSS_DISK_SEEK;
                    myRequest.reg1 =  (void *)(long)requestedTrack;

                    if (USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &myRequest) == USLOSS_DEV_OK)
                    {
                        result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                        if (result != 0) {
                            return -1;
                        }
                    }
                }

                int requestedSector = i % track;

                USLOSS_DeviceRequest myRequest;
                int isRead = nextRW->isRead;
                if (isRead == 1)
                {
                    myRequest.opr = USLOSS_DISK_READ;
                } else if (isRead == 0) {
                    myRequest.opr = USLOSS_DISK_WRITE;
                } else {
                    USLOSS_Console("DiskDriver: Invalid disk operation.  Terminatiing...\n");
                    USLOSS_Halt(1);
                }
                myRequest.reg1 = (void *)(long)requestedSector;
                myRequest.reg2 = (nextRW->buffer + (j * sector));

                if (USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &myRequest) == USLOSS_DEV_OK)
                {
                    result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                    if (result != 0) {
                        return -1;
                    }
                }
            }

            MboxSend(nextRW->privateMbox, NULL, 0);
        }

    }
    return 0;
}

/* ------------------------------------------------------------------------
Name -  TermDriver
Purpose - manages terminals: buffer input ad output
Parameters - char pointer arg = terminal unit
Returns - int -1 for failure, 0 for success
Side Effects - none
----------------------------------------------------------------------- */
static int  TermDriver(char *arg) {

    int result;
    int status;
    int unit = atoi( (char *) arg);
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    while (!isZapped()) {
        result = waitDevice(USLOSS_TERM_DEV, unit, &status);
        if (MboxCondReceive(termKillerDriver[unit], NULL, 0) >= 0)
        {
            quit(0);
        }
        if (result != 0)
        {
            quit(0);
        }
        if (USLOSS_TERM_STAT_RECV(status) == USLOSS_DEV_BUSY)
        {
            MboxCondSend(termReaderMBoxs[unit], &status, 2);
            //unblock TermReader Semaphore
            //semvReal(termReaderBlock[unit]);

        }
        if (USLOSS_TERM_STAT_XMIT(status) == USLOSS_DEV_READY)
        {
            MboxCondSend(termDriverMBox[unit], &status, 2);
        }

    }

    return 0;
}

/* ------------------------------------------------------------------------
Name -  TermReader
Purpose - manages terminals: buffer input ad output
Parameters - char pointer arg = terminal unit
Returns - int -1 for failure, 0 for success
Side Effects - none
----------------------------------------------------------------------- */
static int  TermReader(char *arg) {
    char lineBuffer[MAXLINE + 1];
    memset(lineBuffer, '\0', MAXLINE + 1);
    int count = 0;
    int unit = atoi( (char *) arg);
    int statusCharacter = 0;
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    while (!isZapped()) {
        MboxReceive(termReaderMBoxs[unit], &statusCharacter, 2);
        if (MboxCondReceive(termKillerReader[unit], NULL, 0) >= 0)
        {
            quit(0);
        }
        char temp = USLOSS_TERM_STAT_CHAR(statusCharacter);
        lineBuffer[count] = temp;
        count += 1;
        if (count == MAXLINE || temp == '\n')
        {
            if (count == MAXLINE)
            {
                lineBuffer[count] = '\0';
            }
            MboxCondSend(termReadRealMBox[unit], lineBuffer, MAXLINE + 1);
            count = 0;
        }

    }

    return 0;
}

/* ------------------------------------------------------------------------
Name -  TermWriter
Purpose - manages terminals: buffer input ad output
Parameters - char pointer arg = terminal unit
Returns - int -1 for failure, 0 for success
Side Effects - none
----------------------------------------------------------------------- */
static int  TermWriter(char *arg) {
    char tempBuffer[MAXLINE + 1];
    memset(tempBuffer, '\0', MAXLINE + 1);
    int numChars = 0;
    int unit = atoi( (char *) arg);
    int status = 0;
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    while (!isZapped()) {
        numChars = MboxReceive(termWriterMBboxs[unit], tempBuffer, MAXLINE + 1);
        if (MboxCondReceive(termKillerWriter[unit], NULL, 0) >= 0)
        {
            quit(0);
        }
        int iChar = 0;
        char lastChar;
        while (iChar < numChars) {
            MboxReceive(termDriverMBox[unit], &status, 2);

            if (USLOSS_TERM_STAT_XMIT(status) == USLOSS_DEV_READY) {
                int control = 0;
                int mask = 0;
                mask += tempBuffer[iChar];
                lastChar = tempBuffer[iChar];
                control = USLOSS_TERM_CTRL_CHAR(control, mask);
                control = USLOSS_TERM_CTRL_XMIT_CHAR(control);
                if (termReaderInts[unit] == 1)
                {
                    control = USLOSS_TERM_CTRL_RECV_INT(control);
                }
                USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control);
                iChar++;
            }
        }
        if (lastChar != '\n')
        {
            int control = 0;
            int mask = 0;
            mask += '\n';

            control = USLOSS_TERM_CTRL_CHAR(control, mask);
            control = USLOSS_TERM_CTRL_XMIT_CHAR(control);
            if (termReaderInts[unit] == 1)
            {
                control = USLOSS_TERM_CTRL_RECV_INT(control);
            }
            USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control);
        }

        int control = 0;
        if (termReaderInts[unit] == 1)
        {
            control = USLOSS_TERM_CTRL_RECV_INT(control);
            USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control);
            termReaderInts[unit] = 1;
        }
        int pid;
        MboxReceive(termWriteRealMBoxs[unit], &pid, sizeof(int));
        semvReal(termWriteRealSem[unit]);
        MboxCondSend(driverProcTable[pid].privateMbox, tempBuffer, strlen(tempBuffer + 1));

    }

    return 0;
}


/* ------------------------------------------------------------------------
Name - Sleep
Purpose - delays process for specified time
Parameters - int seconds = amount of time to delay
Returns - int -1 for failure, 0 for success
Side Effects - none
----------------------------------------------------------------------- */
int sleepReal(int seconds) {
    int time = -1;
    int returnValue = -1;
    driverProcPtr current = &driverProcTable[getpid() % MAXPROC];
    driverProcPtr temp = sleepingProcessQueue;

    if (!isZapped())
    {
        gettimeofdayReal(&time);
        time += (seconds * 1000000);
        MboxSend(current->mutexMbox, NULL, 0);
        current->sleepTime = time;

        if (temp != NULL)
        {
            if (temp->sleepTime > time) {
                current->nextProcessAsleep = temp;
                sleepingProcessQueue = current;
            } else {
                while (temp->nextProcessAsleep != NULL && temp->nextProcessAsleep->sleepTime < time)
                {
                    temp = temp->nextProcessAsleep;
                }
                current->nextProcessAsleep = temp->nextProcessAsleep;
                temp->nextProcessAsleep = current;
            }
        } else {
            sleepingProcessQueue = current;
        }
        MboxReceive(current->mutexMbox, NULL, 0);
        MboxReceive(current->privateMbox, NULL, 0);
        returnValue = 0;
    }
    return returnValue;
}

/* ------------------------------------------------------------------------
Name - diskReadReal *TODO*
Purpose - reads one or more sectors from disk
Parameters -
                unit = int, unit number of disk to read
                track = int, starting disk track number
                first = int, starting disk sector number
                sectors = int, number of sectors to read
                diskBuffer = pointer to buffer for read info
Returns - int, -1 for invalid parameters, 0 for success, >0 for disk's status register
Side Effects - none
----------------------------------------------------------------------- */

int  diskReadReal (int unit, int track, int first,
                   int sectors, void *diskBuffer) {
    int returnValue = 0;

    int diskTrackSize = USLOSS_DISK_TRACK_SIZE;
    int thisFirst = (track * diskTrackSize) + first;
    int thisEnd = (track * diskTrackSize) + first + sectors - 1;

    driverProcPtr current = &driverProcTable[(getpid() % MAXPROC)];

    current->isRead = 1;
    current->buffer = diskBuffer;
    current->start = thisFirst;
    current->end = thisEnd;

    addDiskQ(unit, current);

    semvReal(diskSem[unit]);

    MboxReceive(current->privateMbox, NULL, 0);


    return returnValue;

}

/* ------------------------------------------------------------------------
Name -diskWriteReal *TODO*
Purpose - interface to disk driver, write to the disk
Parameters -    diskBuffer = pointer to buffer with written info
                unit = int, unit number of disk to read
                track = int, starting disk track number
                first = int, starting disk sector number
                sectors = int, number of sectors to read
                int pointer status
Returns - int, -1 for invalid parameters, 0 for success, >0 for disk's status register
Side Effects - none
----------------------------------------------------------------------- */
int  diskWriteReal(int unit, int track, int first,
                   int sectors, void *diskBuffer) {
    int returnValue = 0;

    int diskTrackSize = USLOSS_DISK_TRACK_SIZE;
    int thisFirst = (track * diskTrackSize) + first;
    int thisEnd = (track * diskTrackSize) + first + sectors - 1;

    driverProcPtr current = &driverProcTable[(getpid() % MAXPROC)];

    current->isRead = 0;
    current->buffer = (char *)diskBuffer;
    current->start = thisFirst;
    current->end = thisEnd;

    addDiskQ(unit, current);

    semvReal(diskSem[unit]);

    MboxReceive(current->privateMbox, NULL, 0);


    return returnValue;
}

/* ------------------------------------------------------------------------
Name -diskSizeReal *TODO*
Purpose - returns information about size of disk unit
Parameters -    int unit = int, unit number of disk to read
                int pointer sector for number of bytes in sector
                int pointer track for number of sectors in track
                int pointer disk for number of tracks in disk
Returns - int, -1 for invalid parameters, 0 for success
Side Effects - none
----------------------------------------------------------------------- */
int  diskSizeReal (int unit, int *sector, int *track, int *disk) {
    static int diskUnit[USLOSS_DISK_UNITS];

    *sector = USLOSS_DISK_SECTOR_SIZE;
    *track = USLOSS_DISK_TRACK_SIZE;
    if (!(diskUnit[unit]))
    {
        int result;
        int status;
        USLOSS_DeviceRequest myRequest;
        myRequest.opr = USLOSS_DISK_TRACKS;
        myRequest.reg1 = &(diskUnit[unit]);

        if (USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &myRequest) == USLOSS_DEV_OK)
        {
            result = waitDevice(USLOSS_DISK_DEV, unit, &status);
            if (result != 0)
            {
                return -1;
            }
        } else {
            return -1;
        }
    }
    *disk = diskUnit[unit];
    return 0;
}

/* ------------------------------------------------------------------------
Name -termReadReal *TODO*
Purpose -  reads line of test from terminal into buffer
Parameters -    char pointer buffer for characters read
                int bufferSize = max number of characters accepted by buffer
                int unitID = indicates terminal unit
                int pointer for the number of chars read in
Returns - int, -1 for invalid parameters, >0 number of characters read
Side Effects - none
----------------------------------------------------------------------- */
int  termReadReal (int unitID, int bufferSize, char *buffer) {
    //TODO readInterrupts array here
    int control;
    char localBuffer[MAXLINE + 1];
    if (termReaderInts[unitID] == 0)
    {
        for (int i = 0; i < USLOSS_TERM_UNITS; i++)
        {
            control = USLOSS_TERM_CTRL_RECV_INT(0);
            USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void *)(long)control);
            termReaderInts[i] = 1;

        }
    }
    MboxReceive(termReadRealMBox[unitID], localBuffer, MAXLINE + 1);
    strncpy(buffer, localBuffer, bufferSize);
    return strlen(buffer);
}

/* ------------------------------------------------------------------------
Name -termWriteReal *TODO*
Purpose - writes characters to terminal
Parameters -    char pointer buffer = line of text to write
                int bufferSize = number of character to write
                int unitID = terminal unit
                int pointer for number of chars read?????
Returns -
Side Effects -
----------------------------------------------------------------------- */
int  termWriteReal(int unitID, int bufferSize, char *buffer) {
    int pid = getpid();
    MboxSend(termWriterMBboxs[unitID], buffer, bufferSize);
    MboxSend(termWriteRealMBoxs[unitID], &pid, sizeof(int));
    sempReal(termWriteRealSem[unitID]);
    return bufferSize;
}

/* ------------------------------------------------------------------------
Name - sleep*TODO*
Purpose - checks arguments, calls sleepReal to delay process
Parameters - systemArgs pointer sysArg:
                arg1 = number of seconds to delay
Returns - none
Side Effects - sysArg:
                arg4 = -1 for invalid input, 0 otherwise
----------------------------------------------------------------------- */
void sleep(systemArgs *sysArg) {
    int status = -1;
    //TODO bring in matching sysNumbers  && isArgsNotNull from Phase 3
    if (sysArg->number == SYS_SLEEP && sysArg->arg1 != NULL) {
        int sleep = (long)sysArg->arg1;
        status = sleepReal(sleep);
    }
    (sysArg->arg4) = (void *)((long)status);
    //TODO Define USERMODE
    USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
}

/* ------------------------------------------------------------------------
Name - diskRead
Purpose - checks arguments, calls diskReadReal to read from the disk
Parameters - systemArgs pointer sysArg:
                arg1 = memory address to transfer to
                arg2 = number of sectors to read
                arg3 = starting disk track number
                arg4 = starting disk sector number
                arg5 = unit number of disk to read from
Returns - none
Side Effects - sysArg:
                arg1 = 0 if transfer successful, disk status register otherwise
                arg4 = -1 for invalid parameters, 0 otherwise
----------------------------------------------------------------------- */
void diskread(systemArgs *sysArg) {
    void *diskBuffer;
    int sectors;
    int track;
    int first;
    int unit;
    int retStatus = -1;
    int argStatus = -1;

    if (sysArg->arg1 != NULL && sysArg->arg2 != NULL && sysArg->number == SYS_DISKREAD)
    {
        diskBuffer = (void *)sysArg->arg1;
        sectors = (long)sysArg->arg2;
        track = (long)sysArg->arg3;
        first = (long)sysArg->arg4;
        unit = (long)sysArg->arg5;

        if (sectors >= 0 && track >= 0 && track < 16 && first >= 0 && first < 16 && unit >= 0 && unit < USLOSS_DISK_UNITS) {
            retStatus = diskReadReal(unit, track, first, sectors, diskBuffer);
            if (retStatus != -1)
            {
                argStatus = 0;
            }

        }
    }
    (sysArg->arg1) = (void *)((long)retStatus);
    (sysArg->arg4) = (void *)((long)argStatus);
    //TODO set_usermode
    USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);

}

/* ------------------------------------------------------------------------
Name - diskWrite
Purpose - checks arguments, calls diskWriteReal to write sectors to disk
Parameters - systemArgs pointer sysArg:
                arg1 = memory address to write from
                arg2 = number of sectors to read
                arg3 = starting disk track number
                arg4 = starting disk sector number
                arg5 = unit number of disk to write to
Returns - none
Side Effects - sysArg:
                arg1 = 0 if transfer successful, disk status register otherwise
                arg4 = -1 for invalid parameters, 0 otherwise
----------------------------------------------------------------------- */
void diskwrite(systemArgs *sysArg) {
    void *diskBuffer;
    int sectors;
    int track;
    int first;
    int unit;
    int retStatus = -1;
    int argStatus = -1;

    if (sysArg->arg1 != NULL && sysArg->arg2 != NULL && sysArg->number == SYS_DISKWRITE)
    {
        diskBuffer = (void *)sysArg->arg1;
        sectors = (long)sysArg->arg2;
        track = (long)sysArg->arg3;
        first = (long)sysArg->arg4;
        unit = (long)sysArg->arg5;

        if (sectors >= 0 && track >= 0 && track < 16 && first >= 0 && first < 16 && unit >= 0 && unit < USLOSS_DISK_UNITS) {
            retStatus = diskWriteReal(unit, track, first, sectors, diskBuffer);

            if (retStatus != -1)
            {
                argStatus = 0;
            }

        }
    }
    (sysArg->arg1) = (void *)((long)retStatus);
    (sysArg->arg4) = (void *)((long)argStatus);
    //TODO set_usermode
    USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);

}

/* ------------------------------------------------------------------------
Name - disksize
Purpose - checks arguments, calls diskSizeReal to get disk size info
Parameters - systemArgs pointer sysArg:
                arg1 = unit number of disk
Returns - none
Side Effects - sysArgs:
                arg1 = size of sector in bytes
                arg2 = number of sectors in track
                arg3 = number of tracks in disk
                arg4 = -1 for invalid parameters, 0 otherwise
----------------------------------------------------------------------- */
void disksize(systemArgs *sysArg) {
    int unit;
    int disk;
    int sector;
    int track;
    int argStatus = -1;

    if (sysArg->number == SYS_DISKSIZE)
    {
        unit = (long)sysArg->arg1;
        if (unit >= 0 && unit < USLOSS_DISK_UNITS)
        {
            argStatus = diskSizeReal(unit, &sector, &track, &disk);
        }
    }
    (sysArg->arg1) = (void *)((long)sector);
    (sysArg->arg2) = (void *)((long)track);
    (sysArg->arg3) = (void *)((long)disk);
    (sysArg->arg4) = (void *)((long)argStatus);
    //TODO set_usermode
    USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
}

/* ------------------------------------------------------------------------
Name - termRead
Purpose - checks arguments, calls termReadReal to read from terminal
Parameters - systemArgs pointer sysArg
                arg1 = address of user's line buffer
                arg2 = max size of buffer
                arg3 = unit number of terminal to read from
Returns - none
Side Effects - sysArg:
                arg2 = number of characters read
                arg4 = -1 for invalid parameters, 0 otherwise
----------------------------------------------------------------------- */
void termread(systemArgs *sysArg) {
    char *buffer;
    int bufferSize;
    int unitID;
    int numberCharRead = 0;
    int argStatus = -1;

    if (sysArg->arg1 != NULL && sysArg->number == SYS_TERMREAD)
    {
        buffer = (char *)sysArg->arg1;
        bufferSize = (long)sysArg->arg2;
        unitID = (long)sysArg->arg3;

        if (bufferSize >= 0  && bufferSize <= MAXLINE && unitID >= 0 && unitID < USLOSS_TERM_UNITS)
        {
            //Make sure this is correct
            numberCharRead = termReadReal(unitID, bufferSize, buffer);
            if (numberCharRead != -1)
            {
                argStatus = 0;
            }
        }
    }
    (sysArg->arg2) = (void *)((long)numberCharRead);
    (sysArg->arg4) = (void *)((long)argStatus);
}

/* ------------------------------------------------------------------------
Name - termWrite
Purpose - checks arguments, calls termWriteReal to write to terminal
Parameters - systemArgs pointer sysArg
                arg1: address of user's line buffer
                arg2: number of characters to write
                arg3: unit number of terminal to write to
Returns - none
Side Effects - sysArg:
                arg2: number of characters written
                arg4: -1 for invalid parameters, 0 otherwise
----------------------------------------------------------------------- */
void termwrite(systemArgs *sysArg) {
    char *buffer;
    int bufferSize;
    int unitID;
    int numberCharRead = 0;
    int argStatus = -1;

    if (sysArg->arg1 != NULL && sysArg->number == SYS_TERMWRITE)
    {
        buffer = (char *)sysArg->arg1;
        bufferSize = (long)sysArg->arg2;
        unitID = (long)sysArg->arg3;

        if (bufferSize >= 0  && bufferSize <= MAXLINE && unitID >= 0 && unitID < USLOSS_TERM_UNITS)
        {
            //Make sure this is correct
            numberCharRead = termWriteReal(unitID, bufferSize, buffer);
            if (numberCharRead != -1)
            {
                argStatus = 0;
            }
        }
    }
    (sysArg->arg2) = (void *)((long)numberCharRead);
    (sysArg->arg4) = (void *)((long)argStatus);
}

/* ------------------------------------------------------------------------
Name - addDiskQ *TODO*
Purpose -
Parameters -
Returns -
Side Effects -
----------------------------------------------------------------------- */
void addDiskQ (int unit, driverProcPtr newProcess) {
    static int pushCount = 0;
    pushCount += 1;

    driverProcPtr head = diskUnit[unit];

    if (head == NULL)
    {
        (newProcess->prevReadWrite) = newProcess;
        (newProcess->nextReadWrite) = newProcess;
        diskUnit[unit] = newProcess;
    } else {
        driverProcPtr next = head;
        if ((newProcess->start) < (head->start))
        {
            do
            {
                next = (next->prevReadWrite);
            } while ((newProcess->start) < (next->start)
                     && (next->start) >= ((next->nextReadWrite)->start)
                     && (head != next));
            insertAfter(next, newProcess);
        } else {
            do
            {
                next = (next->nextReadWrite);
            } while ((newProcess->start) >= (next->start)
                     && (next->start) >= ((next->prevReadWrite)->start)
                     && (head != next));
            insertAfter(next->prevReadWrite, newProcess);
        }
    }
}

/* ------------------------------------------------------------------------
Name - removeDiskQ *TODO*
Purpose -
Parameters -
Returns -
Side Effects -
----------------------------------------------------------------------- */
driverProcPtr removeDiskQ(int unit) {
    static int popCount = 0;
    popCount += 1;

    driverProcPtr head = diskUnit[unit];

    if (head == NULL)
    {
        return NULL;
    }
    if (head->nextReadWrite == head || head->prevReadWrite == head)
    {
        diskUnit[unit] = NULL;
    } else {
        driverProcPtr next = head->nextReadWrite;
        driverProcPtr previous = head->prevReadWrite;

        next->prevReadWrite = previous;
        previous->nextReadWrite = next;

        diskUnit[unit] = next;
    }

    head->nextReadWrite = NULL;
    head->prevReadWrite = NULL;

    return head;
}



/* ------------------------------------------------------------------------
Name - insertAfter *TODO*
Purpose -
Parameters -
Returns -
Side Effects -
----------------------------------------------------------------------- */
void insertAfter(driverProcPtr currentProcess, driverProcPtr newProcess) {
    (newProcess->nextReadWrite) = (currentProcess->nextReadWrite);
    (newProcess->prevReadWrite) = currentProcess;
    ((currentProcess->nextReadWrite)->prevReadWrite) = newProcess;
    (currentProcess->nextReadWrite) = newProcess;
}

/* ------------------------------------------------------------------------
Name - removeProcess TODO
Purpose - remove a process from head of wake up list
Parameters - none
Returns - driverProcPtr temp = former head of wake up list
Side Effects - sleepingProcessQueue modified
----------------------------------------------------------------------- */

driverProcPtr removeProcess(void) {
    driverProcPtr temp = NULL;

    if (sleepingProcessQueue != NULL)
    {
        temp = sleepingProcessQueue;
        sleepingProcessQueue = temp->nextProcessAsleep;
        temp->nextProcessAsleep = NULL;
    }

    return temp;
}

/* ------------------------------------------------------------------------
Name -  intializeProcStruct TODO
Purpose - initializes all procStruct values to initial values
Parameters - none
Returns - none
Side Effects - none
----------------------------------------------------------------------- */
void intializeProcStruct(void) {
    for (int i = 0; i < MAXPROC; i++)
    {
        driverProcTable[i].mutexMbox = MboxCreate(1, MAX_MESSAGE);
        driverProcTable[i].nextProcessAsleep = NULL;
        driverProcTable[i].privateMbox = MboxCreate(0, MAX_MESSAGE);
        driverProcTable[i].sleepTime = -1;
        driverProcTable[i].isRead = -1;
        driverProcTable[i].prevReadWrite = NULL;
        driverProcTable[i].nextReadWrite = NULL;
        driverProcTable[i].buffer = NULL;
        driverProcTable[i].start = -1;
        driverProcTable[i].end = -1;

    }
}

/* ------------------------------------------------------------------------
Name - initializeSysVec TODO
Purpose -   fills sysVec with proper functions
Parameters - none
Returns - none
Side Effects - none
----------------------------------------------------------------------- */
void initializeSysVec(void) {
    systemCallVec[SYS_SLEEP] = sleep;
    systemCallVec[SYS_DISKREAD] = diskread;
    systemCallVec[SYS_DISKWRITE] = diskwrite;
    systemCallVec[SYS_DISKSIZE] = disksize;
    systemCallVec[SYS_TERMREAD] = termread;
    systemCallVec[SYS_TERMWRITE] = termwrite;

}


/* ------------------------------------------------------------------------
Name -
Purpose -
Parameters -
Returns -
Side Effects -
----------------------------------------------------------------------- */

/*






*/
