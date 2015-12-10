/*
 * Phase5.c
 * Daniel Vaughn and Meg Dever Hanson
 */


#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <provided_prototypes.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>


/* ------------------------- Prototypes ----------------------------------- */
extern void mbox_create(systemArgs *args_ptr);
extern void mbox_release(systemArgs *args_ptr);
extern void mbox_send(systemArgs *args_ptr);
extern void mbox_receive(systemArgs *args_ptr);
extern void mbox_condsend(systemArgs *args_ptr);
extern void mbox_condreceive(systemArgs *args_ptr);
extern int diskReadReal(int a, int b, int c, int d, void* e);
extern int diskWriteReal(int a, int b, int c, int d, void* e);
extern int start5(char* arg);
extern void* vmRegion;
static void
FaultHandler(int  type,  // USLOSS_MMU_INT
             void *arg); // Offset within VM region

static int Pager(char* buf);
void* vmInitReal(int mappings, int pages, int frames, int pagers);
static void vmInit(systemArgs *systemArgsPtr);
static void vmDestroy(systemArgs *systemArgsPtr);
void vmDestroyReal();
void initializeProcTable();
int nextFreeFrame(int pid, int page);
void clearTheFrame(int currentPid, int pageIndex);
void readFrameFromDisk(int pid, int page, int frame);
void writeFrameToDisk(int pid, int page, int frame);
int vmStarted();
void PrintStats(void);




/* -------------------------- Globals ------------------------------------- */
static Process processes[MAXPROC];
PTE** PTETable[MAXPROC]; // pointer to a table of PTEs
FTE* FTETable;
int* pagersTable; //pointer to table of all the pagers
int faultBox;
FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;
int vmInitialization = 0;
int totalPages = 1;
int totalFrames;
int totalMapping;
int totalDiskBlocks;
int pageSize;
int startingProcess;
int numPagers;
int killBox;
int clockSemaphore;
int clockHand = 0;
int nextFreeDiskBlock = 0;

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
Name -  start4 *DONE*
Purpose -Initializes the VM system call handlers.
Parameters - arg
Returns -MMU return status
Side Effects - The MMU is initialized
----------------------------------------------------------------------- */
int
start4(char *arg)
{
  int pid;
  int result;
  int status;
  int sectorsInDisk;
  int tracksInSector;
  int bytesInTrack;
  initializeProcTable();
  SemCreate(1, &clockSemaphore);

  /* to get user-process access to mailbox functions */
  systemCallVec[SYS_MBOXCREATE]      = mbox_create;
  systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
  systemCallVec[SYS_MBOXSEND]        = mbox_send;
  systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
  systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
  systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

  /* user-process access to VM functions */
  systemCallVec[SYS_VMINIT]    = vmInit;
  systemCallVec[SYS_VMDESTROY] = vmDestroy;

  /*Calculate the diskSize and then Calculate the diskblock*/
  DiskSize(0, &bytesInTrack, &tracksInSector, &sectorsInDisk);
  totalDiskBlocks = bytesInTrack * tracksInSector * sectorsInDisk;


  result = Spawn("Start5", start5, NULL, 8 * USLOSS_MIN_STACK, 2, &pid);
  if (result != 0) {
    USLOSS_Console("start4(): Error spawning start5\n");
    Terminate(1);
  }

  result = Wait(&pid, &status);
  if (result != 0) {
    USLOSS_Console("start4(): Error waiting for start5\n");
    Terminate(1);
  }
  Terminate(0);
  return 0; // not reached

} /* start4 */



/* ------------------------------------------------------------------------
Name -  vmInit *DONE*
Purpose -Initializes the VM system
Parameters - arg
Returns -none
Side Effects - VM system is initialized.
----------------------------------------------------------------------- */
static void
vmInit(systemArgs *sysargPtr)
{
  CheckMode();
  int numMappings;
  int numPages;
  int numFrames;
  int numPagers;
  void* returnRegion;

  numMappings = (long)((void*)sysargPtr->arg1);
  numPages = (long)((void*)sysargPtr->arg2);
  numFrames = (long)((void*)sysargPtr->arg3);
  numPagers = (long)((void*)sysargPtr->arg4);
  if (numMappings != numPages || numPagers > MAXPAGERS) {
    sysargPtr->arg4 = (void*)((long) - 1);
    return;
  }
  returnRegion = vmInitReal(numMappings, numPages, numFrames, numPagers);
  sysargPtr->arg1 = returnRegion;
  sysargPtr->arg4 = (void*)((long)0);
  return;

} /* vmInit */


/* ------------------------------------------------------------------------
Name -  vmDestroy *DONE*
Purpose -VM system is cleaned up.
Parameters - None
Returns -none
Side Effects - VM system is cleaned up.
----------------------------------------------------------------------- */

static void
vmDestroy(systemArgs *systemArgsPtr)
{
  CheckMode();
  vmDestroyReal();
} /* vmDestroy */

/* ------------------------------------------------------------------------
Name -  vmInitReal *DONE*
Purpose -Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables
Parameters - None
Returns -Address of the VM region.
Side Effects -  The MMU is initialized.
----------------------------------------------------------------------- */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
  int status;
  int i;
  int dummy;
  char buf[10];
  char name[10];
  void* addr;

  killBox = MboxCreate(4, 0);
  totalPages = pages;
  totalMapping = mappings;
  totalFrames = frames;
  numPagers = pagers;

  strcpy(name, "Pager 1");
  CheckMode();

  status = USLOSS_MmuInit(mappings, pages, frames);
  pageSize = USLOSS_MmuPageSize();
  totalDiskBlocks = totalDiskBlocks / pageSize;


  if (status != USLOSS_MMU_OK) {
    USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
    abort();
  }
  USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
  vmInitialization = 1;


  for (i = 0; i < MAXPROC; i++) {
    processes[i].numPages = pages;
    processes[i].mappings = 0;
  }
  FTETable = malloc(frames * sizeof(FTE));
  for ( i = 0; i < frames; i++) {
    FTETable[i].state = UNUSED;
    FTETable[i].page = -1;
    FTETable[i].pid = -1;
    FTETable[i].cleanBit = -1;

  }


  faultBox = MboxCreate(1, sizeof(FaultMsg));


  startingProcess = 9;
  pagersTable = malloc(pagers * sizeof(int));
  for (i = 0; i < pagers; i++) {
    sprintf(buf, "%d", i);
    pagersTable[i] = fork1(name, Pager, buf, USLOSS_MIN_STACK, 2);
    startingProcess++;
  }


  memset((char *) &vmStats, 0, sizeof(VmStats));
  vmStats.pages = pages;
  vmStats.frames = frames;
  vmStats.freeFrames = frames;
  vmStats.freeDiskBlocks = totalDiskBlocks;
  vmStats.diskBlocks = totalDiskBlocks;


  addr = USLOSS_MmuRegion(&dummy);
  if (dummy != pages) {
    USLOSS_Console("vmInitReal: vmInit failed dummy %d not equal to pages %d\n", dummy, pages);
    abort();
  }
  return addr;
} /* vmInitReal */

/* ------------------------------------------------------------------------
Name -  vmDestroyReal *DONE*
Purpose -Called by vmDestroy.
 * Frees all of the global data structures
Parameters - None
Returns -None
Side Effects -  The MMU is turned off.
----------------------------------------------------------------------- */

void
vmDestroyReal()
{
  int i;
  CheckMode();
  if (vmStarted()) {
    USLOSS_MmuDone();

    vmInitialization = 0;

    free(FTETable);
    for ( i = 0; i < numPagers; i++) {
      MboxCondSend(killBox, NULL, 0);
      MboxSend(faultBox, NULL, 0);
      zap(pagersTable[i]);
    }

    PrintStats();
  }

} /* vmDestroyReal */

/* ------------------------------------------------------------------------
Name -  FaultHandler *DONE*
Purpose -Handles an MMU interrupt. Simply stores information about the
* fault in a queue, wakes a waiting pager, and blocks until
* the fault has been handled.
Parameters - None
Returns -None
Side Effects -  The current process is blocked until the fault is handled.
----------------------------------------------------------------------- */
static void
FaultHandler(int  type /* USLOSS_MMU_INT */,
             void *arg  /* Offset within VM region */)
{

  int cause;
  int pid;
  int pageIndex;
  int size;
  int offset = (int) (long) arg;

  size = USLOSS_MmuPageSize();
  pageIndex = offset / size;

  assert(type == USLOSS_MMU_INT);
  cause = USLOSS_MmuGetCause();
  assert(cause == USLOSS_MMU_FAULT);
  vmStats.faults++;
  getPID_real(&pid);

  if (processes[pid % MAXPROC].pid == -1) {
    processes[pid % MAXPROC].pid = pid;
  }

  if (processes[pid % MAXPROC].mailbox == -1) {
    processes[pid % MAXPROC].mailbox = MboxCreate(1, sizeof(FaultMsg));
  }

  faults[pid % MAXPROC].pid = pid;
  faults[pid % MAXPROC].addr = vmRegion + offset;
  faults[pid % MAXPROC].replyMbox = processes[pid % MAXPROC].mailbox;
  faults[pid % MAXPROC].offset = pageIndex;

  MboxSend(faultBox, &faults[pid % MAXPROC], sizeof(FaultMsg));
  MboxReceive(processes[pid % MAXPROC].mailbox, &faults[pid % MAXPROC], sizeof(FaultMsg));

  USLOSS_MmuMap(TAG, pageIndex, faults[pid % MAXPROC].frame, USLOSS_MMU_PROT_RW);
  processes[pid % MAXPROC].mappings ++;
  vmStats.freeFrames--;


  if (processes[pid % MAXPROC].pageTable[pageIndex].state == UNUSED) {
    vmStats.new++;
  }


  processes[pid % MAXPROC].pageTable[pageIndex].state = INCORE;
  processes[pid % MAXPROC].pageTable[pageIndex].frame = faults[pid % MAXPROC].frame;


  FTETable[faults[pid % MAXPROC].frame].state = INCORE;
  FTETable[faults[pid % MAXPROC].frame].pid = pid;
  FTETable[faults[pid % MAXPROC].frame].page = pageIndex;

} /* FaultHandler */


/* ------------------------------------------------------------------------
Name -  Pager *DONE*
Purpose -Kernel process that handles page faults and does page replacement.
Parameters - None
Returns -None
Side Effects -  None
----------------------------------------------------------------------- */

static int
Pager(char *buf)
{
  FaultMsg fault;
  int i;
  int currentPid;
  int pid;
  int frame;
  int blockedPID;
  int access;
  int state;
  int fromDisk;
  getPID_real(&pid);
  currentPid = pid % MAXPROC;

  processes[currentPid].mailbox = MboxCreate(0, 0);

  while (1) {

    currentPid = -1;
    fromDisk = 0;

    MboxReceive(faultBox, &fault, sizeof(FaultMsg));

    if (!MboxCondReceive(killBox, NULL, 0)) {

      quit(0);
    }

    blockedPID = fault.pid % MAXPROC;

    state = processes[blockedPID].pageTable[fault.offset].state;

    if (state != UNUSED) {

      frame = processes[blockedPID].pageTable[fault.offset].frame;
      USLOSS_MmuGetAccess(fault.offset, &access);

      if (access != USLOSS_MMU_DIRTY && frame == fault.offset) {

        fault.frame = frame;
        MboxSend(fault.replyMbox, &fault, sizeof(FaultMsg));
        continue;
      }
    }

    for (i = 0; i < totalFrames; i++) {

      if (FTETable[i].state == UNUSED) {

        currentPid = i;
        i = totalFrames;
      }
    }
    if ( currentPid == -1) {

      currentPid = nextFreeFrame(blockedPID, fault.offset);
    }

    clearTheFrame(currentPid, fault.offset);
    if (state == ONDISK) {

      readFrameFromDisk(blockedPID, fault.offset, currentPid);

      if (fromDisk) {

        vmStats.replaced++;
      }

    }
    fault.frame = currentPid;

    MboxSend(fault.replyMbox, &fault, sizeof(FaultMsg));
  }
  return 0;
} /* Pager */




/* ------------------------------------------------------------------------
Name -  tableSwap *DONE*
Purpose -takes off the old mappings and puts on the new mappings
Parameters - None
Returns -None
Side Effects -  None
----------------------------------------------------------------------- */
void tableSwap(int old, int new) {

  int i;
  int oldSlot;
  int newSlot;
  int protection;
  int frame;
  int access;
  int state;
  int status;

  oldSlot = old % MAXPROC;
  newSlot = new % MAXPROC;
  i = processes[oldSlot].mappings;

  for (i = 0; i < totalPages; i++) {

    if (old < 10) {

      i = totalPages;
      continue;
    }
    if (processes[oldSlot].pageTable[i].state != UNUSED) {

      USLOSS_MmuGetAccess(i, &access);

      if (processes[oldSlot].pageTable[i].previousAccess == -1) {

        processes[oldSlot].pageTable[i].previousAccess = access;
        processes[oldSlot].pageTable[i].clean = access;
        processes[oldSlot].pageTable[i].ref = access;

      }
      if (access !=  0 && processes[oldSlot].pageTable[i].previousAccess != access) {

        processes[oldSlot].pageTable[i].previousAccess = access;
        processes[oldSlot].pageTable[i].clean = access;
        processes[oldSlot].pageTable[i].ref = access;
      }

      USLOSS_MmuGetMap(TAG, i, &frame, &protection);
      status = USLOSS_MmuUnmap(TAG, i);

      if ( status == USLOSS_MMU_OK) {
        vmStats.freeFrames++;
      }
    }
  }

  for ( i = 0; i < totalPages; i++) {

    if ( new < 10) {

      i = totalPages;
      continue;
    }

    state = processes[newSlot].pageTable[i].state;

    if (state != UNUSED) {

      access = 0;
      frame = processes[newSlot].pageTable[i].frame;
      USLOSS_MmuGetAccess(i, &access);


      if (frame == i) {

        status = USLOSS_MmuMap(TAG, i, frame, USLOSS_MMU_PROT_RW);
        if (status == 0) {

          vmStats.freeFrames--;
          processes[newSlot].pageTable[i].frame = i;
          FTETable[frame].page = i;
          FTETable[frame].pid = new;
        }

      }
    }
  }


}

/* ------------------------------------------------------------------------
Name -  nextFreeFrame *DONE*
Purpose - finds the next free frame by using the clock algorithm
Parameters - None
Returns -None
Side Effects -  None
----------------------------------------------------------------------- */
int nextFreeFrame(int pid, int page) {

  int count;
  int clean;
  int i;
  int isFound;

  count = 0;
  isFound = 0;

  sempReal(clockSemaphore);

  clockHand = clockHand % totalFrames;

  while (!isFound) {
    for (i = 0; i < totalFrames; i++, clockHand++) {
      clean = 0;

      USLOSS_MmuGetAccess(clockHand % totalFrames, &clean);

      if ((clean & USLOSS_MMU_REF) == 1) {

        if (( clean & USLOSS_MMU_DIRTY) == 2) {

          if ( count == 3) {

            writeFrameToDisk(pid, page, clockHand % totalFrames);
            isFound = 1;
            i = totalFrames;
            clockHand--;
          }
        } else {

          if (count == 2) {

            writeFrameToDisk(pid, page, clockHand % totalFrames);
            isFound = 1;
            i = totalFrames;
            clockHand--;
          }
        }
      }

      else if ((clean & USLOSS_MMU_DIRTY) == 2) {

        if (count == 1) {

          writeFrameToDisk(pid, page, clockHand % totalFrames);
          isFound = 1;
          i = totalFrames;
          clockHand--;
        }
      } else {

        writeFrameToDisk(pid, page, clockHand % totalFrames);
        isFound = 1;
        i = totalFrames;
        clockHand--;

      }
    }
    count++;
  }
  clockHand++;
  semvReal(clockSemaphore);

  return (clockHand - 1) % totalFrames;
}



/* -------------------------- Helpers ------------------------------------- */

/* ------------------------------------------------------------------------
Name -  writeFrameToDisk *DONE*
Purpose -
Parameters - None
Returns -
Side Effects -  None
----------------------------------------------------------------------- */

void writeFrameToDisk(int pid, int page, int frame) {

  int buf[pageSize];
  int framePID;
  int track;
  int first;
  int sectors;
  int diskBlock;
  int framePage;
  int access;
  int* region;

  getPID_real(&track);

  USLOSS_MmuMap(TAG, page, frame, USLOSS_MMU_PROT_RW);
  framePID = FTETable[frame].pid;
  framePage = FTETable[frame].page;


  USLOSS_MmuGetAccess(frame, &access);

  region = ((int*)(vmRegion + (page * pageSize)));
  memcpy(buf, region, pageSize);

  USLOSS_MmuGetAccess(frame, &track);
  USLOSS_MmuUnmap(TAG, page);

  processes[framePID].pageTable[framePage].frame = -1;

  if ((access & USLOSS_MMU_DIRTY) != 2) {

    processes[framePID].pageTable[framePage].state = EMPTY;
    return;
  }

  vmStats.pageOuts++;

  processes[framePID].pageTable[framePage].state = ONDISK;

  if (processes[framePID].pageTable[framePage].diskBlock == -1) {

    processes[framePID].pageTable[framePage].diskBlock = nextFreeDiskBlock;
    diskBlock = nextFreeDiskBlock;
    nextFreeDiskBlock += pageSize;
  } else {

    diskBlock = processes[framePID].pageTable[framePage].diskBlock;
  }

  sectors = pageSize / USLOSS_DISK_SECTOR_SIZE;
  track = diskBlock / (USLOSS_DISK_TRACK_SIZE * USLOSS_DISK_SECTOR_SIZE);
  first = track + ( (diskBlock - track * 512 * 16) / USLOSS_DISK_SECTOR_SIZE);
  diskWriteReal(0, track, first, sectors, (void*)buf);
}

/* ------------------------------------------------------------------------
Name -  readFrameFromDisk *DONE*
Purpose -
Parameters - None
Returns -
Side Effects -  None
----------------------------------------------------------------------- */

void readFrameFromDisk(int pid, int page, int frame) {

  char buf[pageSize];
  int track;
  int first;
  int sectors;
  int diskBlock;
  int* region;

  diskBlock = processes[pid].pageTable[page].diskBlock;
  USLOSS_MmuMap(TAG, page, frame, USLOSS_MMU_PROT_RW);
  region = ((int*) (vmRegion + (page * pageSize)));
  USLOSS_MmuUnmap(TAG, page);

  sectors = pageSize / USLOSS_DISK_SECTOR_SIZE;
  track = diskBlock / (USLOSS_DISK_TRACK_SIZE * USLOSS_DISK_SECTOR_SIZE);
  first = track + ((diskBlock - track * 512 * 16) / USLOSS_DISK_SECTOR_SIZE);

  diskReadReal(0, track, first, sectors, (void*)buf);

  USLOSS_MmuMap(TAG, page, frame, USLOSS_MMU_PROT_RW);
  memcpy(region, buf, pageSize);

  USLOSS_MmuSetAccess(frame, 0);
  USLOSS_MmuUnmap(TAG, page);
  vmStats.pageIns++;

}


/* ------------------------------------------------------------------------
Name -  clearTheFrame *DONE*
Purpose -
Parameters - None
Returns -
Side Effects -  None
----------------------------------------------------------------------- */
void clearTheFrame(int currentPid, int pageIndex) {

  int access;
  int dummy;
  int protection;

  USLOSS_MmuGetAccess(currentPid, &access);

  USLOSS_MmuMap(TAG, pageIndex, currentPid, USLOSS_MMU_PROT_RW);
  USLOSS_MmuGetMap(TAG, pageIndex, &dummy, &protection);
  USLOSS_MmuGetAccess(currentPid, &access);

  memset(vmRegion + pageIndex * pageSize, 0,  pageSize);

  USLOSS_MmuSetAccess(currentPid, 0);
  USLOSS_MmuUnmap(TAG, pageIndex);
}

/* ------------------------------------------------------------------------
Name -  vmStarted *DONE*
Purpose -check if vm has started
Parameters - None
Returns -vmInitialization
Side Effects -  None
----------------------------------------------------------------------- */
int vmStarted() {
  return vmInitialization;
}

/* ------------------------------------------------------------------------
Name -  vmStarted *DONE*
Purpose -creates the process table
Parameters - None
Returns -vmInitialization
Side Effects -  None
----------------------------------------------------------------------- */
void initializeProcTable() {
  int i;
  for (i = 0; i < MAXPROC; i++) {

    processes[i].numPages = -1;
    processes[i].pageTable = NULL;
    processes[i].pid = -1;
    processes[i].mailbox = -1;

    PTETable[i] = NULL;

    faults[i].pid = -1;
    faults[i].addr = NULL;
    faults[i].replyMbox = -1;
  }

}

/* ------------------------------------------------------------------------
Name -  releasePTETable *DONE*
Purpose -frees the Page table
Parameters - None
Returns -
Side Effects -  None
----------------------------------------------------------------------- */
void releasePTETable(int currentPid) {

  free(processes[currentPid].pageTable);
}

/* ------------------------------------------------------------------------
Name -  releasePTETable *DONE*
Purpose -gives a p rocess a pagetable
Parameters - None
Returns -
Side Effects -  None
----------------------------------------------------------------------- */
void getPTETable(int pid) {
  int currentPid = pid % MAXPROC;
  int j;
  processes[currentPid].pageTable = malloc(totalPages * sizeof(PTE));
  for (j = 0; j < totalPages; j++) {

    processes[currentPid].pageTable[j].state = UNUSED;
    processes[currentPid].pageTable[j].frame = -1;
    processes[currentPid].pageTable[j].diskBlock = -1;
    processes[currentPid].pageTable[j].clean = 0;
    processes[currentPid].pageTable[j].ref = 0;
    processes[currentPid].pageTable[j].previousAccess = -1;
  }
}

/* ------------------------------------------------------------------------
Name -  releaseFrames *DONE*
Purpose -frees a frametable
Parameters - None
Returns -
Side Effects -  None
----------------------------------------------------------------------- */
void releaseFrames(int currentPid) {
  int i;
  int frame;
  int protection;
  int status;
  if (currentPid < 10)
    return;
  for (i = 0; i < totalPages; i++) {


    status = USLOSS_MmuGetMap(TAG, i, &frame, &protection);
    if (status == USLOSS_MMU_OK) {
      status = USLOSS_MmuUnmap(TAG, i);
      vmStats.freeFrames++;
      FTETable[frame].state = UNUSED;
    }
  }
}

/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void PrintStats(void)
{
  USLOSS_Console("VmStats\n");
  USLOSS_Console("pages:          %d\n", vmStats.pages);
  USLOSS_Console("frames:         %d\n", vmStats.frames);
  USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
  USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
  USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
  USLOSS_Console("switches:       %d\n", vmStats.switches);
  USLOSS_Console("faults:         %d\n", vmStats.faults);
  USLOSS_Console("new:            %d\n", vmStats.new);
  USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
  USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
  USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */
