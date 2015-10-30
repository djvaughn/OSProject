/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015
   Daniel Vaughn and Meg Dever-Hansen

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void contextSwitch(int currentNULLFlag) ;
void launch();
static void enableInterrupts();
static void disableInterrupts();
static void checkDeadlock();
void kernelCheck(char *function);
static void clockHandler(int dev, void *arg);
void zeroProcess(int procSlot);
void addToReadyList(procPtr newProcess);
procPtr removeFromReadyList();
procPtr find(procPtr head, int processPriority);
void insertChild(procPtr childProcess);
void addChildToQuitTable(procPtr quitChild);
procPtr removeChildFromQuitTable(void);
int clearQuitProcess(int childrenPid);
void addToZappedLIst(procPtr zappee);
procPtr removeFromZappedList(procPtr zappee);


/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 0;

/* the process table */
procStruct ProcTable[MAXPROC];

/* Process lists  */
static procPtr ReadyList;

/* current process ID */
procPtr Current;

/*Old process ID*/
procPtr Old;

/* the next pid to be assigned */
unsigned int nextPid = SENTINELPID;

/*Total number of process*/
int totalProcessAlive = 0;

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup *DONE*
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
  int i;      /* loop index */
  int result; /* value returned by call to fork1() */

  /* initialize the process table */
  if (DEBUG && debugflag)
    USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
  for (i = 0; i < MAXPROC; i++) {
    zeroProcess(i);
  }

  /* Initialize the Ready list, etc. */
  if (DEBUG && debugflag)
    USLOSS_Console("startup(): initializing the Ready list\n");
  ReadyList = NULL;
  Current = NULL;

  /* Initialize the clock interrupt handler */
  USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

  /* startup a sentinel process */
  if (DEBUG && debugflag)
    USLOSS_Console("startup(): calling fork1() for sentinel\n");
  result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                 SENTINELPRIORITY);

  /*Sentinel didn't fork correctly */
  if (result < 0) {
    if (DEBUG && debugflag) {
      USLOSS_Console("startup(): fork1 of sentinel returned error, ");
      USLOSS_Console("halting...\n");
    }
    USLOSS_Halt(1);
  }

  /* start the test process */
  if (DEBUG && debugflag)
    USLOSS_Console("startup(): calling fork1() for start1\n");
  result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);

  /*start1 didn't fork correctly */
  if (result < 0) {
    USLOSS_Console("startup(): fork1 for start1 returned an error, ");
    USLOSS_Console("halting...\n");
    USLOSS_Halt(1);
  }

  USLOSS_Console("startup(): Should not see this message! ");
  USLOSS_Console("Returned from fork1 call that created start1\n");

  return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish *DONE*
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
  if (DEBUG && debugflag)
    USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1 *DONE*
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */



int fork1(char *name, int (*procCode)(char *), char *arg,
          int stacksize, int priority)
{
  /* test if in kernel mode; halt if in user mode */
  kernelCheck("fork1");
  disableInterrupts();
  int procSlot = -1;
  int currentPid = nextPid;
  int currentProcSlot = nextPid % MAXPROC;
  nextPid++;
  if ((currentPid != 1) && (currentProcSlot == 1))
  {
    nextPid++;
    currentPid = nextPid;
    currentProcSlot = nextPid % MAXPROC;
    nextPid++;
  }
  if (DEBUG && debugflag)
    USLOSS_Console("fork1(): creating process %s\n", name);

  /*checks the priority.  First to see if its out of rang
   then checks if they are the sentinel*/
  if (DEBUG && debugflag) {
    USLOSS_Console("fork1(): checking if priority is out of range\n");
  }

  if (((priority > MINPRIORITY) || (priority < MAXPRIORITY)) && (priority != SENTINELPRIORITY) ) {
    return -1;
  } else if ((priority == SENTINELPRIORITY) && (strcmp(name, "sentinel") != 0)) {
    return -1;
  }

  /*check if the procCode pointer is NULL*/
  if (DEBUG && debugflag) {
    USLOSS_Console("fork1(): Checking if the procCode pointer is null\n");
  }
  if (procCode == NULL) {
    return -1;
  }

  /*Check if the name of the process is NULL*/
  if (DEBUG && debugflag) {
    USLOSS_Console("fork1(): Checking if name is NULL\n");
  }
  if (name == NULL) {
    return -1;
  }

  /* find an empty slot in the process table return -1 if no empty slots*/
  if (DEBUG && debugflag)
    USLOSS_Console("fork1(): Checking for empty slots in the process table\n");
  if (ProcTable[currentProcSlot].status == NO_PROCESS)
  {
    procSlot = currentProcSlot;
  }

  /*Halt if there is no availabe slots in the table*/
  if (procSlot == -1) {
    return -1;
  }

  /* Return -2 if stack size is too small */
  if (DEBUG && debugflag)
    USLOSS_Console("fork1(): Check for stake size\n");

  if (stacksize < USLOSS_MIN_STACK) {
    return -2;
  }

  /* fill-in entry in process table */
  if ( strlen(name) >= (MAXNAME - 1) ) {
    USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
    USLOSS_Halt(1);
  }/*DONE update the struct*/

  strcpy(ProcTable[procSlot].name, name);
  ProcTable[procSlot].pid = currentPid;
  ProcTable[procSlot].start_func = procCode;
  ProcTable[procSlot].priority = priority;
  ProcTable[procSlot].status = READY;
  ProcTable[procSlot].stackSize = stacksize;
  ProcTable[procSlot].stack = malloc(stacksize);

  /*Checking if it failed to allocate memory*/
  if (ProcTable[procSlot].stack == NULL) {
    USLOSS_Console("fork1(): Failed to allocate memory to the stack.  Halting...\n");
    USLOSS_Halt(1);
  }

  /*Checking if the arg is null*/
  if ( arg == NULL ) {
    ProcTable[procSlot].startArg[0] = '\0';
  } else if ( strlen(arg) >= (MAXARG - 1) ) {
    USLOSS_Console("fork1(): argument too long.  Halting...\n");
    USLOSS_Halt(1);
  } else {
    strcpy(ProcTable[procSlot].startArg, arg);
  }

  if ((procSlot != 1) && (procSlot != 2)) {
    if (DEBUG && debugflag)
      USLOSS_Console("fork1(): Inserting Child\n");
    /*update the parent about their child.*/
    insertChild(&ProcTable[procSlot]);
    Current->childCounter += 1;
    if (DEBUG && debugflag)
      USLOSS_Console("fork1(): %s child counter is %d\n", Current->name, Current->childCounter);
  }

  /* Initialize context for this process, but use launch function pointer for
   * the initial value of the process's program counter (PC)
   */
  if (DEBUG && debugflag) {
    USLOSS_Console("fork1(): Initializing the context\n");
  }

  USLOSS_ContextInit(&(ProcTable[procSlot].state), USLOSS_PsrGet(),
                     ProcTable[procSlot].stack,
                     ProcTable[procSlot].stackSize,
                     launch);

  /*Adding to ready list */
  if (debugflag && DEBUG) {
    USLOSS_Console("fork1(): Add process to ReadyList");
  }
  addToReadyList(&ProcTable[procSlot]);

  /*Calling the dispatcher*/
  if (DEBUG && debugflag) {
    USLOSS_Console("fork1(): Calling the dispatcher\n");
  }
  totalProcessAlive++;
  dispatcher();

  /* for future phase(s) */
  p1_fork(ProcTable[procSlot].pid);

  /*Returning the process id of the created child or -1 if no child could
   be created or if priority is not between max and min priority*/
  if (DEBUG && debugflag) {
    USLOSS_Console("fork1(): Returning the PID which is %d \n", ProcTable[procSlot].pid);
  }
  return ProcTable[procSlot].pid;

} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch *DONE*
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
  int result;

  if (DEBUG && debugflag)
    USLOSS_Console("launch(): started\n");

  /* Enable interrupts */
  enableInterrupts();

  /* Call the function passed to fork1, and capture its return value */
  result = Current->start_func(Current->startArg);

  if (DEBUG && debugflag)
    USLOSS_Console("Process %d returned to launch\n", Current->pid);

  quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join *DONE*
   Purpose - Wait for a child process (if one has been forked) to quit.  If
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
  disableInterrupts();
  if (DEBUG && debugflag) {
    USLOSS_Console("join(): Starts\n");
  }
  /* test if in kernel mode; halt if in user mode */
  kernelCheck("join");

  if ((Current->childCounter == 0) && (Current->childQuitList == NULL))
  {
    if (DEBUG && debugflag) {
      USLOSS_Console("join(): No Children: HERE Returning -2\n");
    }
    return -2;
  }


  if (DEBUG && debugflag) {
    USLOSS_Console("join(): has children\n");
  }
  if (isZapped())
  {
    enableInterrupts();
    return -1;
  }
  procPtr quitProc = removeChildFromQuitTable();
  if (quitProc == NULL)
  {
    Current->status = BLOCK_JOIN;
    while (quitProc == NULL) {
      dispatcher();
      if (isZapped()) {
        enableInterrupts();
        return -1;
      }
      quitProc = removeChildFromQuitTable();
    }
  }
  if (DEBUG && debugflag) {
    USLOSS_Console("join(): Child has quit\n");
  }
  if (DEBUG && debugflag) {
    USLOSS_Console("join(): Quit child PID is %d\n", quitProc->pid);
  }

  /* Setting code to the quit childs status */
  *status = quitProc->returnStatus;
  int childrenPid = quitProc->pid;
  if (DEBUG && debugflag)
    USLOSS_Console("join(): Child PID: %d Quit Code: %d\n", childrenPid, *status);

  clearQuitProcess(childrenPid);

  if (DEBUG && debugflag)
    USLOSS_Console("join(): Enable Interrupts\n");
  enableInterrupts();

  if (DEBUG && debugflag)
    USLOSS_Console("join(): Exiting\n");

  return childrenPid;
  /*but wait, there's more. RIP BILLY MAYS*/
} /* join */


/* ------------------------------------------------------------------------
   Name - quit *DONE*
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
  if (debugflag && DEBUG) {
    USLOSS_Console("quit(): Starting the dispatcher.\n");
  }

  /*Checking if in kernel mode*/
  kernelCheck("quit");
  disableInterrupts();

  if (Current->childCounter != 0)
  {
    USLOSS_Console("Quit(): process %d quit with active children. Halting...\n", Current->pid);
    USLOSS_Halt(1);
  }

  if (debugflag && DEBUG) {
    USLOSS_Console("quit(): %s is Quitting.\n", Current->name);
  }

  Current->returnStatus = status;
  if (debugflag && DEBUG) {
    USLOSS_Console("quit(): %s return status set\n", Current->name);
  }
  Current->status = QUIT;
  if (debugflag && DEBUG) {
    USLOSS_Console("quit(): %s status set to quit\n", Current->name);
  }
  int tempSlot = (Current->pid % MAXPROC);
  if (debugflag && DEBUG) {
    USLOSS_Console("quit(): %s trying to free PID %d\n", Current->name, tempSlot);
  }

  if (debugflag && DEBUG) {
    USLOSS_Console("quit(): %s  Stack is freed.\n", Current->name);
  }

  if (Current->parentPtr != NULL)
  {
    /*childcounter*/
    addChildToQuitTable(Current);
    if (Current->parentPtr->status == BLOCK_JOIN)
    {
      if (debugflag && DEBUG) {
        USLOSS_Console("quit(): Parent is BLOCK_JOIN, setting to READY.\n");
      }
      Current->parentPtr->status = READY;
      addToReadyList(Current->parentPtr);
    }
  }
  if (debugflag && DEBUG) {
    USLOSS_Console("quit(): %s  Checking for zapped\n", Current->name);
  }
  if (isZapped())
  {
    procPtr zapper = NULL;
    zapper = Current->zapper;
    if (zapper == NULL)
    {

    }
    while (zapper != NULL) {
      zapper->status = READY;
      addToReadyList(zapper);
      zapper = zapper->nextZapper;
    }
  }
  if ((Current->pid != 1) && (Current->pid != 2))
  {
    Current->parentPtr->childCounter -= 1;
  }
  totalProcessAlive--;
  dispatcher();
  p1_quit(Current->pid);
} /* quit */

/* ------------------------------------------------------------------------
   Name - zap *DONE*
   Purpose -This operation marks a process as being zapped.
                   Subsequent calls to isZapped by that process will return 1.
                   zap does not return until the zapped process has called quit.
                   The kernel should print an error message and call
                   USLOSS_Halt(1) if a process tries to zap itself or attempts to
                   zap a nonexistent process.
   Parameters - the pid of the process to be zapped
   Returns -
                   -1: the calling process itself was zapped while in zap.
                    0: the zapped process has called quit.
   Side Effects
   ------------------------------------------------------------------------ */
int zap(int pid) {
  /* test if in kernel mode; halt if in user mode */
  kernelCheck("zap");
  disableInterrupts();
  if (debugflag && DEBUG) {
    USLOSS_Console("zap(): %s  Checking for same pid as current\n", Current->name);
  }
  if (Current->pid == pid) {
    USLOSS_Console("zap(): process %d tried to zap itself.  Halting...\n", pid);
    USLOSS_Halt(1);
  }
  if (debugflag && DEBUG) {
    USLOSS_Console("zap(): %s  checking if PID matches\n", Current->name);
  }
  if (pid < SENTINELPID || ProcTable[pid % MAXPROC].pid != pid )
  {
    USLOSS_Console("zap(): The process being zap doesn't exist.\n");
    USLOSS_Halt(1);
  }
  procPtr zappee = &ProcTable[pid % MAXPROC];
  if (debugflag && DEBUG) {
    USLOSS_Console("zap(): Setting zappee to %s\n", zappee->name);
  }
  zappee->zapped = 1;

  /* If pid does not exist*/
  if (zappee == NULL)
  {
    USLOSS_Console("zap(): The process does not exist\n");
    USLOSS_Halt(1);
  }
  if (isZapped()) {
    if (debugflag && DEBUG) {
      USLOSS_Console("zap(): Current has been zapped\n");
    }
    /*If the process has been zapped while in zap return -1*/
    enableInterrupts();
    return -1;
  } else if (zappee->status == QUIT) {

    if (debugflag && DEBUG) {
      USLOSS_Console("zap(): Zappee has quit\n");
    }
    /*If the zappe has already quit then return 0*/
    enableInterrupts();
    return 0;
  }
  if (debugflag && DEBUG) {
    USLOSS_Console("zap(): Zappee adding to zappedlist\n");
  }
  /*add the zappee to the Zappers Zapped List*/
  addToZappedLIst(zappee);

  /*Boolean zapped is true and status is changed to BLOCK_ZAP*/
  Current->status = BLOCK_ZAP;
  zappee->zapped = 1;
  if (debugflag && DEBUG) {
    USLOSS_Console("zap(): Zappee added to zappedlist\n");
  }

  /*Call the dispatcher*/
  dispatcher();

  if (isZapped()) {
    if (debugflag && DEBUG) {
      USLOSS_Console("zap(): Current has been zapped\n");
    }
    /*If the process has been zapped while in zap return -1*/
    enableInterrupts();
    return -1;
  }

  return 0;
}

/* ------------------------------------------------------------------------
   Name - isZapped *DONE*
   Purpose -This operation determines if the current process
                   has been zapped.
   Parameters -
   Returns -
                  0: the process has not been zapped.
                  1: the process has been zapped.
   Side Effects
   ------------------------------------------------------------------------ */
int isZapped(void) {
  kernelCheck("isZapped");
  if (Current->zapped)
  {
    return 1;
  } else {
    return 0;
  }
}

/* ------------------------------------------------------------------------
   Name - dispatcher *DONE*
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
  if (debugflag && DEBUG) {
    USLOSS_Console("dispatcher(): Starting the dispatcher.\n");
  }
  kernelCheck("dispatcher");

  if (ReadyList == NULL) {
    if (Current->pid == SENTINELPID)
    {
      addToReadyList(Current);
    } else {
      USLOSS_Console("dispatcher(): ReadyList can't be empty");
      USLOSS_Halt(1);
    }
  }

  if (Current == NULL) {
    /*If the current process running is null*/
    if (ReadyList->priority != SENTINELPRIORITY) {
      Current = removeFromReadyList();
      contextSwitch(1); /*1 = true because no booleans exist in C*/
    }

    /*DONE at currentstart time*/
  } else if (Current->status == RUNNING) {
    if ((Current->priority > ReadyList->priority ) || (readtime() > (QUANTUM * 1000))) {
      /*here use helper method to get total run time and then switch current*/
      Current->totalRunTime += readtime();
      Old = Current;
      Old->startTime = -1;
      Current = removeFromReadyList();
      Old->status = READY;
      addToReadyList(Old);
      if (DEBUG && debugflag)
      {
        USLOSS_Console("dispatcher(): Conext switching %s for %s\n", Old->name, Current->name);
      }
      contextSwitch(0);
    }
  } else {
    Current->totalRunTime += readtime();
    Old = Current;
    Old->startTime = -1;
    Current = removeFromReadyList();
    {
      if (DEBUG && debugflag)
      {
        USLOSS_Console("dispatcher(): Conext switching %s for %s\n", Old->name, Current->name);
      }

    }
    contextSwitch(0);
  }

  /*Checking if the next process on the readyllist is the sentinel*/
  if (ReadyList->priority == SENTINELPRIORITY) {
    if (DEBUG && debugflag) {
      USLOSS_Console("dispatcher(): Enabling Interrupts\n");
    }
    enableInterrupts();
  }
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - contextSwitch *DONE*
   Purpose - the function perfoms a context switch with the old
                    process and the current one.  It receives a flag when
                    the old process is null.
   Parameters - int flag if the old process is null
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void contextSwitch(int currentNULLFlag) {
  if (currentNULLFlag) {
    p1_switch(0, Current->pid);

    if (DEBUG && debugflag) {
      USLOSS_Console("contextSwitch(): Enabling Interrupts\n");
    }
    Current->status = RUNNING;
    enableInterrupts();

    Current->startTime = USLOSS_Clock();
    USLOSS_ContextSwitch(NULL, &(Current->state));

  } else {
    p1_switch(Old->pid, Current->pid);

    if (DEBUG && debugflag) {
      USLOSS_Console("contextSwitch(): Enabling Interrupts\n");
    }
    Current->status = RUNNING;
    enableInterrupts();
    Current->startTime = USLOSS_Clock();
    USLOSS_ContextSwitch(&(Old->state), &(Current->state));
  }
}

/* ------------------------------------------------------------------------
   Name - sentinel *DONE*
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
  if (DEBUG && debugflag) {
    USLOSS_Console("sentinel(): called\n");
  }
  while (1)
  {
    checkDeadlock();
    enableInterrupts();
    USLOSS_WaitInt();
  }
} /* sentinel */

/* ------------------------------------------------------------------------
   Name - checkDeadlock *DONE*
   Purpose - Checks for a deadlock and halts the system if one is
     found or it allows the system to continue running the sentinel process
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
static void checkDeadlock()
{
  kernelCheck("checkDeadlock");
  disableInterrupts();
  int hasAllQuit = 1;
  int areAllBlocked = 1;
  /*change to QUIT and no process*/
  int i = 0;
  if (totalProcessAlive != 1)
  {
    hasAllQuit = 0;
  }
  for (i = 0; i < MAXPROC; i++)
  {
    if (ProcTable[i].status == READY) {
      areAllBlocked = 0;
      i = MAXPROC;
    }
  }
  if (hasAllQuit)
  {
    USLOSS_Console("All processes completed\n");
    USLOSS_Halt(0);
  } else if (areAllBlocked) {
    USLOSS_Console("check_deadlock(): num_proc = %d\n", totalProcessAlive);
    USLOSS_Console("check_deadlock(): processes still present.  Halting...\n");
    USLOSS_Halt(1);
  }
} /* checkDeadlock */

/* ------------------------------------------------------------------------
   Name - enableInterrupts *DONE*
   Purpose - enableInterrupts
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
static void enableInterrupts()
{
  /* turn the interrupts on iff we are in kernel mode */
  if ( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
    /*not in kernel mode*/
    USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
    USLOSS_Console("disable interrupts\n");
    USLOSS_Halt(1);
  } else {
    /* We ARE in kernel mode */
    USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
  }

}

/* ------------------------------------------------------------------------
   Name - disableInterrupts *DONE*
   Purpose - disableInterrupts
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
static void disableInterrupts()
{
  /* turn the interrupts OFF iff we are in kernel mode */
  if ( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
    /*not in kernel mode*/
    USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
    USLOSS_Console("disable interrupts\n");
    USLOSS_Halt(1);
  } else {
    /* We ARE in kernel mode */
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
  }
} /* disableInterrupts */

/* ------------------------------------------------------------------------
   Name - dumpProcesses *DONE*
   Purpose - This routine should print process information
                    to the console. For each PCB in the process
                    table, print (at a minimum) its PID, parent’s PID,
                    priority, process status (e.g. unused, running, ready,
                    blocked, etc.), number of children, CPU time
                    consumed, and name.
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void  dumpProcesses(void) {
  kernelCheck("dumpProcesses");
  USLOSS_Console("%s%11s%10s%14s%16s%9s%5s\n", "PID", "Parent", "Priority", "Status", "# Kids", "CPUtime", "Name");
  int i = 0;
  int pid = -1;
  int parent = -1;
  int priority = -1;
  int kidsCounter = 0;
  int cpuTime = -1;
  char *name[] = {
    "EMPTY", "READY",
    "RUNNING", "QUIT",
    "BLOCK_JOIN", "BLOCK_ZAP"
  };
  for (i = 0; i < MAXPROC; ++i)
  {
    if (ProcTable[i].status != NO_PROCESS)
    {
      int status = ProcTable[i].status;
      pid = ProcTable[i].pid;
      if (ProcTable[i].parentPtr != NULL)
      {
        parent = ProcTable[i].parentPtr->pid;

      } else {
        parent = -2;
      }
      priority = ProcTable[i].priority;
      kidsCounter = ProcTable[i].childCounter;
      if (ProcTable[i].totalRunTime > 0)
      {
        cpuTime = ProcTable[i].totalRunTime ;
      }
      if (status > BLOCK)
      {
        USLOSS_Console("%3d%9d%9d%16d%16d%9d%8s\n", pid, parent, priority, status,  kidsCounter, cpuTime, ProcTable[i].name);
      } else {
        USLOSS_Console("%3d%9d%9d%16s%16d%9d%8s\n", pid, parent, priority, name[status],  kidsCounter, cpuTime, ProcTable[i].name);
      }
    } else {
      USLOSS_Console("%3d%9d%9d%16s%16d%9d\n", pid, parent, priority, name[0],  kidsCounter, cpuTime);
    }
    pid = -1;
    parent = -1;
    priority = -1;
    kidsCounter = 0;
    cpuTime = -1;
  }
}

/* ------------------------------------------------------------------------
   Name - timeSlice *DONE*
   Purpose - If the current process has exceeded the timeslice
                   dispatcher is called.
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void timeSlice(void) {
  kernelCheck("timeSlice");
  if ((Current->status == RUNNING) && (readtime() >= (QUANTUM * 1000)))
  {
    dispatcher();
  }
}

/* ------------------------------------------------------------------------
   Name - getpid *DONE*
   Purpose - returns the pid of the currrent process
   Parameters - none
   Returns - the pid of the current process
   Side Effects - none
   ----------------------------------------------------------------------- */
int getpid(void) {
  kernelCheck("getpid");
  return Current->pid;
}

/* ------------------------------------------------------------------------
   Name - blockMe *DONE*
   Purpose - This operation will block the calling process.
   Parameters - int value of the status of the process
   Returns - -1 if the process was zapped while blocked
                    0 otherwise
   Side Effects - none
   ----------------------------------------------------------------------- */
int blockMe(int block_status) {
  kernelCheck("blockMe");
  if (block_status <= 10) {
    USLOSS_Console("blockMe(): new_status is less than 10.");
    USLOSS_Halt(1);
  }
  disableInterrupts();

  Current->status = block_status;
  dispatcher();

  if (isZapped())
  {
    return -1;
  } else {
    return 0;
  }
}

/* ------------------------------------------------------------------------
   Name - unblockProc *DONE*
   Purpose - This operation unblocks process pid that had previously
                    been blocked by calling blockMe. The status of that
                    process is changed to READY, and it is put on the
                    Ready List.
   Parameters - int value of the pid of the process
   Returns - -2 if the indicated process was not blocked,
                       does not exist, is the Current process, or
                       is blocked on a status less than or equal to
                       10. Thus, a process that is zap-blocked or
                       join-blocked cannot be unblocked with this
                       function call.
                   -1 if the calling process was zapped.
                    0 otherwise
   Side Effects - The dispatcher will be called as a side-effect of this function.
   ----------------------------------------------------------------------- */
int unblockProc(int pid) {
  kernelCheck("blockMe");
  disableInterrupts();

  if (ProcTable[pid % MAXPROC].pid != pid) {
    enableInterrupts();
    return -2;
  }

  if (Current->pid == pid)
  {
    enableInterrupts();
    return -2;
  }

  if (ProcTable[pid % MAXPROC].status <= BLOCK) {
    USLOSS_Console("unblockProc(): Process status is less that or equal to 10");
    enableInterrupts();
    return -2;
  }

  if (isZapped()) {
    enableInterrupts();
    return -1;
  }

  procPtr unblockProc = &ProcTable[pid % MAXPROC];
  unblockProc->status = READY;
  addToReadyList(unblockProc);
  dispatcher();
  return 0;
}

/* ------------------------------------------------------------------------
   Name - clockHandler *DONE*
   Purpose - Calls time slice to see if the current process runs out
    of its time
   Parameters - Called by USLOSS
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
static void clockHandler(int dev, void *arg) {
  timeSlice();
}

/* ------------------------------------------------------------------------
   Name - kernelCheck *DONE*
   Purpose - Checks if we are in Kernel mode.  If not it will halt
   Parameters - a string of the function that called it
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void kernelCheck(char *function) {
  if (DEBUG && debugflag)
    USLOSS_Console("%s(): Check for kernel mode\n", function);
  if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) {
    USLOSS_Console("%s(): called while in user mode, by process %d. Halting...\n", function, Current->pid);
    USLOSS_Halt(1);
  }
}

/* ------------------------------------------------------------------------
   Name - zeroProcess *DONE*
   Purpose - Takes a procSlot and then goes to the proctable
                    and sets everything to NULL
   Parameters - the procSlot that is being zeroed out
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void zeroProcess(int procSlot) {
  ProcTable[procSlot].nextProcPtr = NULL;
  ProcTable[procSlot].childProcPtr = NULL;
  ProcTable[procSlot].nextSiblingPtr = NULL;
  ProcTable[procSlot].parentPtr = NULL;
  ProcTable[procSlot].childQuitList = NULL;
  ProcTable[procSlot].nextQuitChild = NULL;
  ProcTable[procSlot].zapper = NULL;
  ProcTable[procSlot].nextZapper = NULL;
  ProcTable[procSlot].zappee = NULL;
  ProcTable[procSlot].childCounter = 0;
  ProcTable[procSlot].zapped = 0;
  strcpy(ProcTable[procSlot].name, "\0");
  strcpy(ProcTable[procSlot].startArg, "\0");
  ProcTable[procSlot].pid = -1;
  ProcTable[procSlot].priority = -1;
  ProcTable[procSlot].start_func = NULL;
  ProcTable[procSlot].stackSize = -1;
  ProcTable[procSlot].stack = NULL;
  ProcTable[procSlot].status = NO_PROCESS;
  ProcTable[procSlot].startTime = -1;
  ProcTable[procSlot].totalRunTime = -1;
  ProcTable[procSlot].returnStatus = -1;
  ProcTable[procSlot].parentPid = -1;
}

/* ------------------------------------------------------------------------
   Name - addToReadyList *DONE*
   Purpose - First the function checks if the Ready list is null
             if it is then the new process(the sentinel) become the first node.
             Second it checks if the new process has a higher priority then the
             first node then the new process becomes the head of the list and
             set its next pointer to the ReadyList.
             Third the function creates a temp pointer(current_ptr) and then calls the find
             where in the linked list it needs to insert the process. Then it sets
             the new process next pointer to current_ptr->nextProcPtr.  Then it changes
             current_ptr->nextProcPtr to point at the new process.
   Parameters - A pointer to the new process
   Returns - none
   Side Effects - none
 ----------------------------------------------------------------------- */
void addToReadyList(procPtr newProcess) {
  if (DEBUG && debugflag) {
    USLOSS_Console("addToReadyList(): adding %s to ready list.\n", newProcess->name);
  }

  kernelCheck("addToReadyList");

  if (DEBUG && debugflag) {
    USLOSS_Console("addToReadyList(): %s priority is %d.\n", newProcess->name, newProcess->priority);
  }

  if (ReadyList == NULL) {
    newProcess->nextProcPtr = ReadyList;
    ReadyList = newProcess;
  } else if (ReadyList->priority > newProcess->priority) {
    newProcess->nextProcPtr = ReadyList;
    ReadyList = newProcess;
  } else {
    procPtr currentPtr = find(ReadyList, newProcess->priority);
    newProcess->nextProcPtr = currentPtr->nextProcPtr;
    currentPtr->nextProcPtr = newProcess;
  }
  if (DEBUG && debugflag) {
    USLOSS_Console("addToReadyList(): added %s to ready list.\n", newProcess->name);
  }

}
/* ------------------------------------------------------------------------
    Name - removeFromReadyList *DONE*
    Purpose - The function creates a pointer to point at the head of the ReadyList
              and then changes the head of the ReadyList to the next process.  Then
              it disconnets the process from the linked list and returns the pointer
    Parameters - none
    Returns - a pointer that points to the node of the ready process
    Side Effects - none
 ----------------------------------------------------------------------- */

procPtr removeFromReadyList() {
  if (DEBUG && debugflag) {
    USLOSS_Console("removeFromReadyList(): removing %s from ready list.\n", ReadyList->name);
  }
  kernelCheck("removeFromReadyList");

  if (ReadyList == NULL)
  {
    return NULL;
  }

  procPtr currentPtr = ReadyList;
  ReadyList = currentPtr->nextProcPtr;
  currentPtr->nextProcPtr = NULL;

  if (DEBUG && debugflag) {
    USLOSS_Console("removeFromReadyList(): Returning %s from ready list.\n", currentPtr->name);
  }
  return currentPtr;

}

/* ------------------------------------------------------------------------
   Name - find *DONE*
   Purpose - The function creates a pointer to point at the head of the ReadyList
             Then it goes into a while loop that checks if the next node is node and
             see if the new process priority is greater than or equal to the current
             nodes priority.  If it is then the loop increments to the next node.
   Parameters - A pointer to the head of the ready list and the new process priority
   Returns - a pointer that points to the node were the new process needs to be inserted after
   Side Effects - none
 ----------------------------------------------------------------------- */

procPtr find(procPtr head, int processPriority) {
  procPtr currentPtr = head;
  while (currentPtr->nextProcPtr != NULL && (currentPtr->nextProcPtr->priority <= processPriority)) {
    currentPtr = currentPtr->nextProcPtr;
  }

  return currentPtr;
}

/* ------------------------------------------------------------------------
   Name - insertChild *DONE*
   Purpose - The function takes a child processes and adds it to the
                    parents list of children.
   Parameters - A pointer to the child process
   Returns -
   Side Effects - none
 ----------------------------------------------------------------------- */
void insertChild(procPtr childProcess) {
  kernelCheck("insertChild");
  if (DEBUG && debugflag)
    USLOSS_Console("insertChild(): Parent %d Child %d\n", Current->pid, childProcess->pid);
  childProcess->nextSiblingPtr = Current->childProcPtr;
  childProcess->parentPtr = Current;
  Current->childProcPtr = childProcess;
  childProcess->parentPid = Current->pid;


  if (DEBUG && debugflag)
    USLOSS_Console("insert_child: End: \n");
}

/* ------------------------------------------------------------------------
   Name - addChildToQuitTable *DONE*
   Purpose - The function takes a quit child processes and
                    adds it to the parents list of quit children.
   Parameters - A pointer to the quit  child process
   Returns -
   Side Effects - none
 ----------------------------------------------------------------------- */
void addChildToQuitTable(procPtr quitChild) {
  kernelCheck("addChildToQuitTable");

  if (quitChild == NULL)
  {
    USLOSS_Console("Current process is null. Halting...\n");
    USLOSS_Halt(1);
  }

  procPtr parentPtr = Current->parentPtr;
  if (quitChild->parentPtr == NULL)
  {
    USLOSS_Console("Parent pointer was not set\n");
  }

  if (parentPtr->childQuitList == NULL)
  {
    if (DEBUG && debugflag)
    {
      USLOSS_Console("parentsQuitList was null, adding quitChild\n");
    }
    parentPtr->childQuitList = quitChild;
    if (DEBUG && debugflag)
    {
      USLOSS_Console("Child is %S\n", parentPtr->childQuitList );
    }
  } else {
    while (parentPtr->childQuitList ->nextQuitChild != NULL) {
      parentPtr->childQuitList  = parentPtr->childQuitList ->nextQuitChild;
    }
    parentPtr->childQuitList ->nextQuitChild = Current;
  }
}

/* ------------------------------------------------------------------------
   Name - removeChildFromQuitTable *DONE*
   Purpose - The function takes a quit child processes from the
                    head of the quitlist and returns it
   Parameters - A pointer to the quit  child process
   Returns -
   Side Effects - none
 ----------------------------------------------------------------------- */
procPtr removeChildFromQuitTable(void) {
  kernelCheck("removeChildFromQuitTable");

  if (Current == NULL)
  {
    USLOSS_Console("Current process is null. Halting...\n");
    USLOSS_Halt(1);
  }

  procPtr quitChild = NULL;

  if (Current->childQuitList != NULL)
  {

    quitChild = Current->childQuitList;
    Current->childQuitList = quitChild->nextQuitChild;
    quitChild->nextQuitChild = NULL;
    if (DEBUG && debugflag) {
      USLOSS_Console("removeChildFromQuitTable(): Returning %s from ready list.\n", quitChild->name);
    }
  } else {
    if (DEBUG && debugflag)
    {
      USLOSS_Console("childQuitList was null\n");
    }
  }
  return quitChild;
}

/* ------------------------------------------------------------------------
   Name - clearQuitProcess *DONE* CHECK
   Purpose - The function removes the quit chiild from the
                    parents child pointer list and then cleans it.
   Parameters - childPid
   Returns - childPid
   Side Effects - none
 ----------------------------------------------------------------------- */
int clearQuitProcess(int childrenPid) {
  kernelCheck("clearQuitProcess");

  procPtr childPtr = Current->childProcPtr;
  procPtr nextChildPtr = childPtr->nextSiblingPtr;
  if (childPtr->pid == childrenPid) {
    Current->childProcPtr = nextChildPtr;
    zeroProcess(childrenPid % MAXPROC);
  } else {
    while (nextChildPtr->pid != childrenPid) {
      childPtr = nextChildPtr;
      nextChildPtr = nextChildPtr->nextSiblingPtr;
    }
    childPtr->nextSiblingPtr = nextChildPtr->nextSiblingPtr;
    nextChildPtr->nextSiblingPtr = NULL;
    zeroProcess(childrenPid % MAXPROC);
  }

  return childrenPid;
}

/* ------------------------------------------------------------------------
   Name - addToZappedLIst *DONE*
   Purpose - This function adds a zappee to currents zapper
                    pointer and then it adds the current zapper to the head
                    of the zappee's zapper llist.
   Parameters - pointer to the zappee
   Returns -
   Side Effects
   ------------------------------------------------------------------------ */
void addToZappedLIst(procPtr zappee) {
  kernelCheck("addToZappedLIst");
  procPtr head = NULL;
  if ( zappee->zapper == NULL)
  {
    Current->nextZapper = head;
    zappee->zapper = Current;
  } else {
    head = zappee->zapper;
    procPtr currentPointer = head;
    while (currentPointer->nextZapper != NULL) {
      currentPointer = currentPointer->nextZapper;
    }
    Current->nextZapper = currentPointer->nextZapper;
    currentPointer->nextZapper = Current;
  }

}

/* ------------------------------------------------------------------------
   Name - removeFromZappedList *DONE*
   Purpose - This function removes a zapper from the zappee pointer
                    head list and moves the list to the next zapper
                    of the zappee's zapper llist.
   Parameters - pointer to the zappee
   Returns - pointer to zapper
   Side Effects
   ------------------------------------------------------------------------ */
procPtr removeFromZappedList(procPtr zappee) {
  kernelCheck("removeFromZappedList");

  procPtr currentZapper = NULL;
  if (zappee->zapper != NULL)
  {
    if (DEBUG && debugflag)
    {
      USLOSS_Console("1\n");
    }
    procPtr currentZapper = zappee->zapper;
    if (DEBUG && debugflag)
    {
      USLOSS_Console("2\n");
    }
    zappee->zapper = zappee->nextZapper;
    if (DEBUG && debugflag)
    {
      USLOSS_Console("3\n");
    }
    currentZapper->nextZapper = NULL;
    if (DEBUG && debugflag)
    {
      USLOSS_Console("currentzapper is %s\n", currentZapper->name);
    }
  } else {
    if (DEBUG && debugflag)
    {
      USLOSS_Console("currentzapper is NULL\n");
    }
  }

  return currentZapper;
}

/* ------------------------------------------------------------------------
   Name - readtime *DONE*
   Purpose - gets the current total run time of the current process
   Parameters -
   Returns - the total run time in microseconds
   Side Effects - none
   ----------------------------------------------------------------------- */
int readtime(void) {
  return (USLOSS_Clock() - readCurStartTime());
}

/* ------------------------------------------------------------------------
   Name - readCurStartTime *DONE*
   Purpose - gets the current start time
   Parameters -
   Returns - the start time in microseconds
   Side Effects - none
   ----------------------------------------------------------------------- */
int readCurStartTime(void) {
  return Current->startTime;
}
