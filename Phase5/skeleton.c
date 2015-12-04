/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */


#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>

extern void mbox_create(sysargs *args_ptr);
extern void mbox_release(sysargs *args_ptr);
extern void mbox_send(sysargs *args_ptr);
extern void mbox_receive(sysargs *args_ptr);
extern void mbox_condsend(sysargs *args_ptr);
extern void mbox_condreceive(sysargs *args_ptr);

static Process processes[MAXPROC];

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;
PTE** PTETable[MAXPROC];
FTE FTETable[MAXPROC];

int faultmBox;
int maxPage;
int totalFrames;


static void
FaultHandler(int  type,  // USLOSS_MMU_INT
             void *arg); // Offset within VM region
extern void *vmRegion;
static void vmInit(sysargs *sysargsPtr);
static void vmDestroy(sysargs *sysargsPtr);
int virtualMemInitialized;
/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers.
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
  int pid;
  int result;
  int status;

  virtualMemInitialized = 0;

  /* to get user-process access to mailbox functions */
  systemCallVec[SYS_MBOXCREATE]      = mboxCreate;
  systemCallVec[SYS_MBOXRELEASE]     = mboxRelease;
  systemCallVec[SYS_MBOXSEND]        = mboxSend;
  systemCallVec[SYS_MBOXRECEIVE]     = mboxReceive;
  systemCallVec[SYS_MBOXCONDSEND]    = mboxCondsend;
  systemCallVec[SYS_MBOXCONDRECEIVE] = mboxCondreceive;

  /* user-process access to VM functions */
  systemCallVec[SYS_VMINIT]    = vmInit;
  systemCallVec[SYS_VMDESTROY] = vmDestroy;

  intializeProcStruct();

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

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(systemArgs *sysArg)
{
  int mappings;
  int pages;
  int frames;
  int pagers;
  int returnStatus = -1;
  void **region;

  CheckMode();
  if (sysArg->number == SYS_VMINIT && sysArg->arg1 >= 0 && sysArg->arg2 >= 0 && sysArg->arg3 >= 0 && sysArg->arg4 >= 0 )
  {
    mappings = (long)sysArg->arg1;
    pages = (long)sysArg->arg2;
    frames = (long)sysArg->arg3;
    pagers = (long)sysArg->arg4;
    if (mappings == pages)
    {
      *region = vmInitReal(mappings, pages, frames, pagers);
      returnStatus = 0;
    }
  }
  (sysArg->arg1) = *region;
  (sysArg->arg4) = (void *)((long)returnStatus);

} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
vmDestroy(sysargs *sysargsPtr)
{
  CheckMode();
  if (sysArg->number == SYS_VMDESTROY)
  {
    vmDestroyReal();
  }
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
  char    name[128];
  char    buffer[10];
  int status;
  int dummy;
  virtualMemInitialized = 1;
  CheckMode();
  status = USLOSS_MmuInit(mappings, pages, frames);
  if (status != USLOSS_MMU_OK) {
    USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
    abort();
  }
  maxPage = pages;
  totalFrames = frames;
  USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;


  /*
   * Initialize page tables.
   */
  for (int i = 0; i < MAXPROC; i++)
  {
    PTETable[i] = malloc(pages * sizeof(PTE*));
    processes[i].numPages = pages;
    for (int j = 0; j < pages; j++)
    {
      PTETable[i][j] = malloc(sizeof(PTE));
      PTETable[i][j]->state = UNUSED;
      PTETable[i][j]->frame = -1;
      PTETable[i][j]->diskBlock - 1;
    }


  }
  FTETable = malloc(frames * sizeof(FTE));
  for (int i = 0; i < frames; i++)
  {
    FTETable[i].state = UNUSED;
  }
  /*
   * Create the fault mailbox.
   */
  faultmBox = mboxCreate(1, sizeof(FaultMsg));

  /*
   * Fork the pagers.
   */
  for (i = 0; i < pagers; i++)
  {
    sprintf(buffer, "%d", i);
    sprintf(name, "Pager %d", i);
    fork1(name, Pager, buffer, USLOSS_MIN_STACK, PAGER_PRIORITY);
  }


  /*
   * Zero out, then initialize, the vmStats structure
   */
  memset((char *) &vmStats, 0, sizeof(VmStats));
  vmStats.pages = pages;
  vmStats.frames = frames;
  /*
   * Initialize other vmStats fields.
   */

  return USLOSS_MmuRegion(&pages);
} /* vmInitReal */


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
void
PrintStats(void)
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


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{

  CheckMode();
  USLOSS_MmuDone();
  /*
   * Kill the pagers here.
   */
  /*
   * Print vm statistics.
   */
  USLOSS_Console("vmStats:\n");
  USLOSS_Console("pages: %d\n", vmStats.pages);
  USLOSS_Console("frames: %d\n", vmStats.frames);
  USLOSS_Console("blocks: %d\n", vmStats.blocks);
  /* and so on... */

} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int  type /* USLOSS_MMU_INT */,
             void *arg  /* Offset within VM region */)
{
  int cause;
  FTE frame;
  int offset = (int) (long) arg;
  int pageLocation = offset / USLOSS_MmuPageSize;
  assert(type == USLOSS_MMU_INT);
  cause = USLOSS_MmuGetCause();
  assert(cause == USLOSS_MMU_FAULT);
  vmStats.faults++;
  /*
   * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
   * reply.
   */
  int pid = getPid();
  faults[pid % MAXPROC].pid = pid;
  faults[pid % MAXPROC].addr = vmRegion + offset;
  faults[pid % MAXPROC].replyMbox = processes[pid % MAXPROC].privateMbox;
  faults[pid % MAXPROC].offset = offset;
  MboxSend(faultmBox, &faults[pid % MAXPROC], sizeof(FaultMsg));
  MboxReceive(processes[pid % MAXPROC].privateMbox, &frame, sizeof(FTE));
  USLOSS_MmuMap(TAG, pageLocation, fault[pid % MAXPROC].frame, TAG);

} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
  int count = -1;
  while (1) {
    FaultMsg recieveMsg;
    Mbox_Receive(faultmBox, &recieveMsg, sizeof(FaultMsg));
    for (int i = 0; i < totalFrames; i++)
    {
      if (FTETable[i].state == UNUSED)
      {
        count = i;
        i = totalFrames
      }
    }
    if (count == - 1)
    {
      count = clockAlgorithm(recieveMsg.addr);
    }
    mapPager( count, recieveMsg.offset);
    recieveMsg.frame = count;
    if (FTETable[count].state == ONDISK)
    {
      FTETable[count].state = ONBOTH;
    } else {
      FTETable[count].state = INCORE;
    }

    /* Wait for fault to occur (receive from mailbox) */
    /* Look for free frame */
    /* If there isn't one then use clock algorithm to
     * replace a page (perhaps write to disk) */
    /* Load page into frame from disk, if necessary */
    /* Unblock waiting (faulting) process */
  }
  return 0;
} /* Pager */

void intializeProcStruct(void) {
  for (int i = 0; i < MAXPROC; i++)
  {
    processes[i].mutexMbox = MboxCreate(1, MAX_MESSAGE);
    processes[i].privateMbox = MboxCreate(0, MAX_MESSAGE);
    processes[i].numPages = 0;
    processes[i].pageTable = NULL;
    faults[i].pid = -1;
    faults[i].addr = NULL;
    faults[i].replyMbox = -1

  }
}

void getPageTable(int pid) {
  int currentPid = pid % MAXPROC;
  processes[currentPid].pageTable = PTETable[currentPid];
}

void swapPageTable(int oldProcessPid, int newProcessPid) {
  int code;
  vmStats.switches++;
  int oldProcess = oldProcessPid % MAXPROC;
  int newProcess = newProcessPid % MAXPROC;


}

int clockAlgorithm(void *addr) {

}

void mapPager(int count, int slot) {
  USLOSS_MmuMap(TAG, slot, count, TAG);
  memcpy(vmRegion + slot * USLOSS_MmuPageSize(), 0, USLOSS_MmuPageSize());
  USLOSS_MmuUnmap(TAG, slot);
}
//fte table malck page pointer times frames