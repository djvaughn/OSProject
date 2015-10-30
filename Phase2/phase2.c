/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
static void zeroMailBox(int mailBox);
static void zeroMailSlots(int mailSlot);
static void zeroProcTable(int procSlot);
static void kernelCheck(char *function);
static void disableInterrupts();
static void enableInterrupts();
static mailBoxPtr getMailBox();
static int activeMailBox(mailBoxPtr mailBox);
static void addToTheBlockList(mailBoxPtr mailBox, int blockCode);
static int removeFromTheBlockList(mailBoxPtr mailBox);
static slotPtr getSlot(void *msg_ptr, int msg_size);
static void addSlotToMailBox(mailBoxPtr mailBox, slotPtr slot);
static slotPtr removeSlotFromMailBox(mailBoxPtr mailBox);
static int freeSlot(void *msg_ptr, slotPtr slot);
int waitdevice(int type, int unit, int *status);
static int checkMailBoxProcess(mailBoxPtr mailBox);
void syscallHandler(int dev, void *arg);
void termHandler(int dev, void *arg);
void diskHandler(int dev, void *arg);
void clockHandler2(int dev, void *arg);
void nullsys(systemArgs *args);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

/* the system call vector */
void (*sys_vec[MAXSYSCALLS])(systemArgs *args);

// the mail boxes
mailbox MailBoxTable[MAXMBOX];

//the mail slots
mailSlot MailSlotTable[MAXSLOTS];

//process table
procStruct ProcTable[MAXPROC];

/* current process ID */
procPtr Current;

/*Old process ID*/
procPtr Old;

//MailBoxId
int nextMailBoxId = 0;

//MailSlots tracker
int totalSlotsInUse = 0;
int termMailBox[USLOSS_TERM_UNITS];
int diskMailBox[USLOSS_DISK_UNITS];
int clockMailBox[USLOSS_CLOCK_UNITS];

int clockCount = 0;
int checkIoBool = 0;

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
  if (DEBUG2 && debugflag2)
    USLOSS_Console("start1(): at beginning\n");

  kernelCheck("start1");
  int status;
  // Disable interrupts
  disableInterrupts();
  int i = 0;
  // Initialize the mail box table, slots, & other data structures.


  for (i = 0; i < MAXMBOX; i++) {
    zeroMailBox(i);
  }
  for (i = 0; i < MAXSLOTS; i++) {
    zeroMailSlots(i);
  }

  for (i = 0; i < MAXPROC; i++) {
    zeroProcTable(i);
  }


  // Initialize USLOSS_IntVec and system call handlers

  USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;
  USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
  USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
  USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;

  // allocate mailboxes for interrupt handlers
  for (int i = 0; i < MAXSYSCALLS; i++)
  {
    sys_vec[i] = nullsys;
  }
  //create mailboxes *TODO*
  for (int i = 0; i < USLOSS_TERM_UNITS; i++)
  {
    termMailBox[i] = MboxCreate(0, 2);
  }
  for (int i = 0; i < USLOSS_DISK_UNITS; i++)
  {
    diskMailBox[i] = MboxCreate(0, 2);
  }
  for (int i = 0; i < USLOSS_CLOCK_UNITS; i++)
  {
    clockMailBox[i] = MboxCreate(0, 2);
  }
  enableInterrupts();

  // Create a process for start2, then block on a join until start2 quits
  if (DEBUG2 && debugflag2)
    USLOSS_Console("start1(): fork'ing start2 process\n");
  int kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
  if ( join(&status) != kid_pid ) {
    USLOSS_Console("start2(): join returned something other than ");
    USLOSS_Console("start2's pid\n");
  }

  return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailBox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailBox and the max size of a msg
                sent to the mailBox.
   Returns - -1 to indicate that no mailBox was created, or a value >= 0 as the
             mailBox id.
   Side Effects - initializes one element of the mail box array
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
  kernelCheck("MboxCreate");
  disableInterrupts();
  int createdMBoxId = -1;

  mailBoxPtr createdMailbox = NULL;
  if (slot_size <= MAX_MESSAGE && slot_size >= 0 && slots >= 0)
  {
    createdMailbox = getMailBox();
    if (createdMailbox != NULL)
    {
      createdMailbox->totalNumberSlots = slots;
      createdMailbox->slotSize = slot_size;
      createdMailbox->status = ACTIVE;
      createdMBoxId = createdMailbox->mBoxId;
    }

  }

  enableInterrupts();

  return createdMBoxId;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - "Zaps" all processes block on mailbox's blocked list, frees slots from mailbox,
              frees mailbox from mailbox array
   Parameters - id of the mailbox
   Returns - (-1) for invalid mailbox id, (0) for success
   Side Effects - erases one element of mailbox array, frees slots in slot array
   ----------------------------------------------------------------------- */

int MboxRelease(int mbox_id) {
  kernelCheck("MboxRelease");
  disableInterrupts();
  if (mbox_id < 0)
  {
    enableInterrupts();
    return -1;
  }
  mailBoxPtr mailBox = &MailBoxTable[mbox_id];
  checkMailBoxProcess(mailBox);


  while (mailBox->blockedProcessList != NULL) {
    mailBox->blockedProcessList->zapped = 1;
    removeFromTheBlockList(mailBox);
  }

  slotPtr tempSlot = removeSlotFromMailBox(mailBox);
  char clearString[MAX_MESSAGE];
  while (tempSlot != NULL) {
    freeSlot(clearString, tempSlot);
    tempSlot = removeSlotFromMailBox(mailBox);
  }
  mailBox->status = EMPTY;
  enableInterrupts();
  return 0;
}


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailBox.
             Block the sending process if no slot available.
   Parameters - mailBox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {

  kernelCheck("MboxSend");
  disableInterrupts();
  int returnValue = -1;
  if (mbox_id < 0)
  {
    enableInterrupts();
    return -1;
  }
  mailBoxPtr mailBox = &MailBoxTable[mbox_id % MAXMBOX];



  returnValue = checkMailBoxProcess(mailBox);
  if (returnValue != 0)
  {
    enableInterrupts();
    return returnValue;
  }
  if (msg_size > mailBox->slotSize || msg_size < 0)
  {
    enableInterrupts();
    return -1;
  }
  if ((mailBox->totalNumberSlots == 0) || (mailBox->slotsInUse == mailBox->totalNumberSlots)) {

    if ((mailBox->blockedProcessList != NULL) && (mailBox->blockedProcessList->status == RECEIVEBLOCK))
    {
      if (msg_ptr != NULL)
      {
        mailBox->blockedProcessList->messageSize = msg_size;
        memcpy(mailBox->blockedProcessList->message, msg_ptr, msg_size);
      }
      removeFromTheBlockList(mailBox);
      enableInterrupts();
      return 0;
    }

    if (msg_ptr != NULL) {
      memcpy(&ProcTable[getpid() % MAXPROC].message, msg_ptr, msg_size);
      ProcTable[getpid() % MAXPROC].messageSize = msg_size;
    }
    addToTheBlockList(mailBox, SENDBLOCK);
    //checking if process was zapped or is not active after adding to block list
    returnValue = checkMailBoxProcess(mailBox);
    if (returnValue != 0)
    {
      enableInterrupts();
      return returnValue;
    } else {
      slotPtr slot = NULL;
      slot = getSlot(msg_ptr, msg_size);
      if (slot == NULL)
      {
        USLOSS_Console("MboxSend: Slot table overflow\n");
        USLOSS_Halt(1);
      }
      addSlotToMailBox(mailBox, slot);
      enableInterrupts();
      return 0;
    }
  }
  returnValue = checkMailBoxProcess(mailBox);
  if (returnValue != 0)
  {
    enableInterrupts();
    return returnValue;
  }
  if ((mailBox->blockedProcessList != NULL) && (mailBox->blockedProcessList->status == RECEIVEBLOCK))
  {
    if (msg_ptr != NULL)
    {
      mailBox->blockedProcessList->messageSize = msg_size;
      memcpy(mailBox->blockedProcessList->message, msg_ptr, msg_size);
    }
    removeFromTheBlockList(mailBox);
    enableInterrupts();
    return 0;
  } else {
    slotPtr slot = NULL;
    slot = getSlot(msg_ptr, msg_size);
    if (slot == NULL)
    {
      USLOSS_Console("MboxSend: Slot table overflow\n");
      USLOSS_Halt(1);
    }
    addSlotToMailBox(mailBox, slot);
    enableInterrupts();
    return 0;
  }
} /* MboxSend */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailBox.
             Block the receiving process if no msg available.
   Parameters - mailBox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size) {
  kernelCheck("MboxReceive");
  disableInterrupts();
  int messageSize = 0;
  int returnValue = -1;
  if (mbox_id < 0) {
    enableInterrupts();
    return -1;
  }
  mailBoxPtr mailBox = &MailBoxTable[mbox_id];
  returnValue = checkMailBoxProcess(mailBox);
  if (returnValue != 0)
  {
    enableInterrupts();
    return returnValue;
  }

  if ((mailBox->totalNumberSlots == 0) || (mailBox->slotsInUse == 0)) {
    if ((mailBox->blockedProcessList != NULL) && (mailBox->blockedProcessList->status == SENDBLOCK)) {
      if (msg_ptr != NULL) {
        messageSize = mailBox->blockedProcessList->messageSize;
        memcpy(msg_ptr, mailBox->blockedProcessList->message, messageSize);
      }

      removeFromTheBlockList(mailBox);
      enableInterrupts();
      return messageSize;
    } else {
      addToTheBlockList(mailBox, RECEIVEBLOCK);
      returnValue = checkMailBoxProcess(mailBox);
      if (returnValue != 0) {
        enableInterrupts();
        return returnValue;
      }

      if (msg_ptr != NULL) {
        int procSlot = getpid() % MAXPROC;
        messageSize = ProcTable[procSlot].messageSize;
        if (msg_size<messageSize)
        {
         return -1;
        }
        memcpy(msg_ptr, ProcTable[procSlot].message, messageSize);
      }
      enableInterrupts();
      return messageSize;
    }
  }

  returnValue = checkMailBoxProcess(mailBox);

  if (returnValue != 0) {
    enableInterrupts();
    return returnValue;
  }

  if ((mailBox->usedSlotList != NULL) && (mailBox->usedSlotList->messageSize > msg_size)) {
    enableInterrupts();
    return -1;
  }
  if ((mailBox->slotsInUse == mailBox->totalNumberSlots))
  {
    removeFromTheBlockList(mailBox);
  }
  slotPtr slot = removeSlotFromMailBox(mailBox);
  messageSize = freeSlot(msg_ptr, slot);

  returnValue = checkMailBoxProcess(mailBox);

  enableInterrupts();
  return messageSize;

} /* MboxReceive */

/* ------------------------------------------------------------------------
 Name - MboxCondSend
 Purpose - Put a message into a slot for the indicated mailBox. DO NOT
            block if message no process is present to receive message, simply return.
 Parameters - mailbox id, pointer to data of msg, # of butes in msg.
 Returns - (0) if successful, (-1) if invalid args, -2 if all slots are filled
 Side Effects - none
 ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
  kernelCheck("MboxCondSend");
  int returnValue = -1;
  if (mbox_id < 0 || mbox_id > MAXMBOX)
  {
    return -1;
  }
  if (totalSlotsInUse >= MAXSLOTS)
  {
    return -2;
  }
  disableInterrupts();
  mailBoxPtr mailBox = &MailBoxTable[mbox_id];
  if (msg_size > mailBox->slotSize || msg_size < 0)
  {
    enableInterrupts();
    return -1;
  }

  returnValue = checkMailBoxProcess(mailBox);
  if (returnValue != 0)
  {
    enableInterrupts();
    return returnValue;
  }

  if (mailBox->slotsInUse < mailBox->totalNumberSlots)
  {
    if (msg_size > mailBox->slotSize)
    {
      enableInterrupts();

      return -1;
    }
    slotPtr tempSlot = getSlot(msg_ptr, msg_size);
    if (tempSlot == NULL)
    {
      enableInterrupts();

      return -2;
    }
    addSlotToMailBox(mailBox, tempSlot);
    tempSlot->isConditional = 1;
    enableInterrupts();

    return 0;
  } else {

    enableInterrupts();
    return -2;
  }
}

/* ------------------------------------------------------------------------
   Name - MboxCondReceive
   Purpose - Get a msg from a slot of the indicated mailBox.
             DO NOT block the receiving process if no msg available.
   Parameters - mailBox id, pointer to put data of msg, max # of bytes that
                can be received
   Returns - actual size of msg if successful, -1 if invalid args, -2 if no slots available
   Side Effects - none.
   ----------------------------------------------------------------------- */

int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_max_size) {
  kernelCheck("MboxCondReceive");
  int returnValue = -1;
  if (mbox_id < 0 || mbox_id > MAXMBOX)
  {
    return -1;
  }
  disableInterrupts();
  mailBoxPtr mailBox = &MailBoxTable[mbox_id];

  returnValue = checkMailBoxProcess(mailBox);
  if (returnValue != 0)
  {
    enableInterrupts();
    return returnValue;
  }

  if (msg_max_size < 0)
  {
    enableInterrupts();
    return -1;
  }

  if ((mailBox->totalNumberSlots != 0) && (mailBox->slotsInUse != 0))
  {
    if (mailBox->usedSlotList->messageSize > msg_max_size )
    {
      enableInterrupts();

      return -1;
    }
    slotPtr tempSlot = removeSlotFromMailBox(mailBox);
    if (tempSlot == NULL)
    {
      enableInterrupts();

      return -2;
    }
    int messageSize = freeSlot(msg_ptr, tempSlot);
    if (!(tempSlot->isConditional))
    {
      removeFromTheBlockList(mailBox);
    }
    enableInterrupts();
    return messageSize;
  } else {
    enableInterrupts();

    return -2;
  }

}


/* ------------------------------------------------------------------------
 Name - zeroMailBox
 Purpose - Takes a procSlot and then goes to the proctable
                  and sets everything to NULL/initial values)
 Parameters - the procSlot that is being zeroed out
 Returns - nothing
 Side Effects - none
 ----------------------------------------------------------------------- */
static void zeroMailBox(int mailBox) {
  MailBoxTable[mailBox].mBoxId = -1;
  MailBoxTable[mailBox].totalNumberSlots = 0;
  MailBoxTable[mailBox].slotSize = -1;
  MailBoxTable[mailBox].status = EMPTY;
  MailBoxTable[mailBox].slotsInUse = 0;
  MailBoxTable[mailBox].usedSlotList = NULL;
  MailBoxTable[mailBox].blockedProcessList = NULL;
}

/* ------------------------------------------------------------------------
 Name - zeroMailBox
 Purpose - Takes a procSlot and then goes to the proctable
                  and sets everything to NULL/initial values
 Parameters - the procSlot that is being zeroed out
 Returns - nothing
 Side Effects - none
 ----------------------------------------------------------------------- */
static void zeroMailSlots(int mailSlot) {
  MailSlotTable[mailSlot].mBoxId = -1;
  MailSlotTable[mailSlot].status = EMPTY;
  MailSlotTable[mailSlot].isConditional = 0;
  MailSlotTable[mailSlot].messageSize = -1;
  MailSlotTable[mailSlot].nextUsedSlot = NULL;
  memset((MailSlotTable[mailSlot].message), 0, MAX_MESSAGE);
}

/* ------------------------------------------------------------------------
 Name - zeroProcTable
 Purpose - Takes a procSlot and then goes to the proctable
                  and sets everything to NULL/initial values
 Parameters - the procSlot that is being zeroed out
 Returns - nothing
 Side Effects - none
 ----------------------------------------------------------------------- */
static void zeroProcTable(int procSlot) {
  ProcTable[procSlot].pid = -1;
  ProcTable[procSlot].status = -1;
  ProcTable[procSlot].zapped = 0;
  ProcTable[procSlot].messageSize = -1;
  ProcTable[procSlot].nextBlocked = NULL;
  memset((ProcTable[procSlot].message), 0, MAX_MESSAGE);

}

/* ------------------------------------------------------------------------
   Name - kernelCheck
   Purpose - Checks if we are in Kernel mode.  If not it will halt
   Parameters - a string of the function that called it
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
static void kernelCheck(char *function) {
  if (debugflag2)
    USLOSS_Console("%s(): Check for kernel mode\n", function);
  if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) {
    USLOSS_Console("%s(): called while in user mode, by process %d. Halting...\n", function, Current->pid);
    USLOSS_Halt(1);
  }
}
/* ------------------------------------------------------------------------
   Name - disableInterrupts
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
    /* We are in kernel mode */
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
  }
} /* disableInterrupts */

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
    USLOSS_Console("enable interrupts\n");
    USLOSS_Halt(1);
  } else {
    /* We are in kernel mode */
    USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
  }

}

/* ------------------------------------------------------------------------
   Name - getMailBox
   Purpose - Finds a free slot in the Mail Box table.
   Parameters - none
   Returns - mailBoxPtr to new mailbox
   Side Effects - None
   ----------------------------------------------------------------------- */
static mailBoxPtr getMailBox() {
  mailBoxPtr newMailbox = NULL;
  for (int i = 0; i < MAXMBOX; ++i)
  {
    if (MailBoxTable[i].status == EMPTY)
    {
      MailBoxTable[i].mBoxId = i;
      newMailbox = &MailBoxTable[i];
      i = MAXMBOX;
    }
  }


  return newMailbox;

}

/* ------------------------------------------------------------------------
   Name - activeMailBox
   Purpose - checks whether or not the mailbox is active
   Parameters - pointer to relevant mailbox
   Returns - and int (boolean): 1 for active, 0 for inactive
   Side Effects - none
   ----------------------------------------------------------------------- */

static int activeMailBox(mailBoxPtr mailBox) {
  if (mailBox->status == ACTIVE)
  {
    return 1;
  } else {
    return 0;
  }
}

/* ------------------------------------------------------------------------
   Name - addToTheBlockList
   Purpose - adds a process to the mailbox's blocked list
   Parameters - pointer to relevant mailbox, int indicating what kind of block
   Returns - nothing
   Side Effects - death, disease, dismemberment. and a new process on the block list.
   ----------------------------------------------------------------------- */

static void addToTheBlockList(mailBoxPtr mailBox, int blockCode) {
  kernelCheck("addToTheBlockList");

  int procSlot = getpid() % MAXPROC;
  procPtr tempProc = &ProcTable[procSlot];
  tempProc->status = blockCode;
  tempProc->pid = getpid();
  procPtr blockList = mailBox->blockedProcessList;
  if (blockList == NULL) {
    tempProc->nextBlocked = mailBox->blockedProcessList;
    mailBox->blockedProcessList = tempProc;
  } else {
    while (blockList->nextBlocked != NULL) {
      blockList = blockList->nextBlocked;
    }
    tempProc->nextBlocked = blockList->nextBlocked;
    blockList->nextBlocked = tempProc;

  }
  blockMe(blockCode);
}

/* ------------------------------------------------------------------------
   Name - removeFromTheBlockList
   Purpose - removes a process from the head of mailbox's blocked list
   Parameters - pointer to relevant mailbox
   Returns - 0 in all cases. for reasons.
   Side Effects - process removed from blok list
   ----------------------------------------------------------------------- */
static int removeFromTheBlockList(mailBoxPtr mailBox) {
  kernelCheck("removeFromTheBlockList");
  if (mailBox == NULL)
  {
    return 0;
  }

  procPtr blockedProcess = mailBox->blockedProcessList;

  if (blockedProcess == NULL)
  {
    return 0;
  }
  int pid = mailBox->blockedProcessList->pid;

  mailBox->blockedProcessList = blockedProcess->nextBlocked;

  ProcTable[pid % MAXPROC].status = -1;
  unblockProc(pid);
  return 0;
}

/* ------------------------------------------------------------------------
   Name - getSlot
   Purpose - gets an empty slot from slot array, copies msg to it, passes pointer
              of slot to caller
   Parameters - pointer to msg, size of the message
   Returns - pointer to slot with message
   Side Effects - slot initialized
   ----------------------------------------------------------------------- */
static slotPtr getSlot(void *msg_ptr, int msg_size) {
  slotPtr temp = NULL;
  int index = -1;
  for (int i = 0; i < MAXSLOTS; ++i)
  {
    if (MailSlotTable[i].status == EMPTY)
    {
      index = i;
      i = MAXSLOTS;
    }
  }
  if (index != -1)
  {
    temp = &MailSlotTable[index];
    temp->status = ACTIVE;
    memcpy(temp->message, msg_ptr, msg_size);
    temp->messageSize = msg_size;
    totalSlotsInUse++;
  }
  return temp;
}

/* ------------------------------------------------------------------------
   Name - addSlotToMailBox
   Purpose - finds empty slot in slot array, passes pointer to mailbox and adds to slot list,
              removes head of mailbox's block list
   Parameters - pointer to relevant mailbox, pointer to relevant new slot
   Returns - nothing
   Side Effects - slot becomes active, added to slot list of mailbox, head process
                  on block list is unblocked
   ----------------------------------------------------------------------- */
static void addSlotToMailBox(mailBoxPtr mailBox, slotPtr slot) {
  kernelCheck("addSlotToMailBox");
  slot->mBoxId = mailBox->mBoxId;
  slotPtr temp = NULL;
  if (mailBox->usedSlotList == NULL)
  {
    mailBox->usedSlotList = slot;

  } else {
    temp = mailBox->usedSlotList;
    while (temp->nextUsedSlot != NULL) {
      temp = temp->nextUsedSlot;
    }
    temp->nextUsedSlot = slot;
  }
  mailBox->slotsInUse++;

  if ((mailBox->blockedProcessList != NULL) && (mailBox->blockedProcessList->status == RECEIVEBLOCK))
  {
    removeFromTheBlockList(mailBox);
  }


}

/* ------------------------------------------------------------------------
   Name - removeSlotFromMailBox
   Purpose - removes a slot from the relevant mailbox
   Parameters - pointer to relevant mailbox
   Returns - null if invalid args, pointer to removed slot
   Side Effects - global slots in use and mailbox total slots in use decremented
   ----------------------------------------------------------------------- */
static slotPtr removeSlotFromMailBox(mailBoxPtr mailBox) {
  kernelCheck("removeSlotFromMailBox");
  if (mailBox == NULL || mailBox->usedSlotList == NULL)
  {
    return NULL;
  }
  totalSlotsInUse--;
  mailBox->slotsInUse--;

  slotPtr tempSlot = mailBox->usedSlotList;
  mailBox->usedSlotList = tempSlot->nextUsedSlot;
  tempSlot->mBoxId = -1;
  return tempSlot;
}

/* ------------------------------------------------------------------------
   Name - freeSlot
   Purpose - zeroes out a slot
   Parameters - pointer to a message, pointer to slot
   Returns - int size of the message
   Side Effects - values are zeroed out
   ----------------------------------------------------------------------- */
static int freeSlot(void *msg_ptr, slotPtr slot) {
  memcpy(msg_ptr, slot->message, slot->messageSize);
  int returnMessageSize = slot->messageSize;
  slot->status = EMPTY;
  slot->messageSize = -1;
  slot->nextUsedSlot = NULL;
  slot->isConditional = 0;
  memset((slot->message), 0, returnMessageSize);
  return returnMessageSize;
}

/* ------------------------------------------------------------------------
   Name - waitdevice
   Purpose - check if we're blocked on a device
   Parameters - into for type of block, int for which device, int ofr if blocked or not
   Returns - (-1) if zapped, 0 if otherwise
   Side Effects - none
   ----------------------------------------------------------------------- */
int waitDevice(int type, int unit, int *status) {
  kernelCheck("waitdevice");
  checkIoBool = 1;
  if (isZapped())
  {
    checkIoBool = 0;
    return -1;
  }
  if (type == USLOSS_CLOCK_DEV) {
    MboxReceive(clockMailBox[unit], status, 1);
    USLOSS_DeviceInput(type, unit, status);
    checkIoBool = 0;
    return 0;
  }
  if (type == USLOSS_TERM_DEV)
  {
    MboxReceive(termMailBox[unit], status, 1);
    USLOSS_DeviceInput(type, unit, status);
    checkIoBool = 0;
    return 0;
  }

checkIoBool = 0;
  return 0;
}

/* ------------------------------------------------------------------------
   Name - check_io
   Purpose - check if we're block on i/o device
   Parameters - none
   Returns - int (boolean) 0 if not block, 1 if blocked
   Side Effects - none
   ----------------------------------------------------------------------- */

int check_io() {
 /* int isBlocked = 0;
  if (checkIoBool)
  {
    isBlocked = 1;
  }
  return isBlocked;*/
  return checkIoBool;
}

/* ------------------------------------------------------------------------
   Name - checkMailBoxProcess
   Purpose - check if a process has various conditions (zapped, active mailbox)
   Parameters - pointer to relevant mailbox
   Returns - (-1) if mailbox is inactive, -3 if process is zapped, 0 otherwise
   Side Effects - none
   ----------------------------------------------------------------------- */
static int checkMailBoxProcess(mailBoxPtr mailBox) {

  int procSlot = getpid() % MAXPROC;
  procPtr tempProc = &ProcTable[procSlot];
  if (tempProc->zapped == 1) {
    tempProc->zapped = 0;
    enableInterrupts();
    return -3;
  }
  if (isZapped())
  {
    enableInterrupts();
    return -3;
  }
  if (!activeMailBox(mailBox))
  {
    enableInterrupts();
    return -1;
  }

  return 0;
}

/*
-------------------------------------------------
Handlers
--------------------------------------------------
*/


void nullsys(systemArgs *args)
{
  USLOSS_Console("syscall_handler(): sys number 0 is wrong.  Halting..\n");
  USLOSS_Halt(1);
} /* nullsys */


void clockHandler2(int dev, void *arg)
{
  timeSlice();
  if (DEBUG2 && debugflag2)
    USLOSS_Console("clockHandler2(): called\n");
  if (clockCount++ == 5)
  {
    clockCount = 0;
    checkIoBool = 0;
    MboxCondSend(clockMailBox[0], NULL, 0);
  }

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
  if (sysPtr->number < 0 || MAXSYSCALLS)
  {
    USLOSS_Console("syscall_handler(): sys number %d is wrong.  Halting...\n",
                   sysPtr->number);
    USLOSS_Halt(1);
  }
  USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

  sys_vec[sysPtr->number](sysPtr);

} /* syscallHandler */
