#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <usyscall.h>
#include <libuser.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sems.h"

#define SETUSERMODE USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE )
//TODO Change
#define MESSAGE_SEM "1"

#define CHECKMODE {                     \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) {                \
        USLOSS_Console("Trying to invoke syscall from kernel OURS\n");   \
        USLOSS_Halt(1);                     \
    }                           \
}

/* ------------------------- Prototypes ----------------------------------- */
int start2(char *arg);
extern int start3(char *);
int spawnReal(char *name, int (*func)(char *), char *arg, int stackSize, int priority);
int waitReal(int *status);
void terminateReal(int status);
int semcreateReal(int value, int *semaphore);
int sempReal(int semaphore);
int semvReal(int semaphore);
int semfreeReal(int semaphore);
void gettimeofdayReal(int *timeOfDay);
void cpuTimeReal(int *cpuTime);
void getpidReal(int *pid);
void spawn(systemArgs *sysArg);
void wait(systemArgs *sysArg);
void terminate(systemArgs *sysArg);
void semcreate(systemArgs *sysArg);
void semp(systemArgs *sysArg);
void semv(systemArgs *sysArg);
void semfree(systemArgs *sysArg);
void gettimeofday(systemArgs *sysArg);
void cputime(systemArgs *sysArg);
void get_pid(systemArgs *sysArg);
void processInitialize(void);
void semaphoresInitialize(void);
void systemVecInitialize(void);
void nullsys3(systemArgs *args);
int isArgsNotNull(systemArgs *sysArg);
int matchingSysNumber(int SYS_OPCODE, systemArgs *sysArg);
void addChild(userProcPtr parentProc, userProcPtr childProc);
int isTerminating(int pid);
int cleanUpProcess(userProcPtr currentProcess);
void clearProcess(userProcPtr deletingProcess);
int spawnLaunch(char *arg);

/* -------------------------- Globals ------------------------------------- */
/* Process Table */
userProcStruct userProcTable[MAXPROC];

/* Semaphore Table */
semStruct semTable[MAXSEMS];

/* the system call vector */
void (*systemCallVec[MAXSYSCALLS])(systemArgs * args);

/*DEBUGflag*/
int DEBUG3 = 0;

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
Name - start2
Purpose - initializes proctable, semaphores, systemvec. launches with spawnreal
Parameters - char pointer
Returns - pid
Side Effects - entirety of phase 3, basically
----------------------------------------------------------------------- */
int start2(char *arg)
{
    int pid;
    int status;
    /*
     * Check kernel mode here.
     */
    CHECKMODE;
    /*
     * Data structure initialization as needed...
     */
    processInitialize();
    semaphoresInitialize();
    systemVecInitialize();

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler; spawnReal is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);
  //  USLOSS_Console("HERE6");
  //  dumpProcesses();
    quit(pid);
    return pid;
} /* start2 */

/*---------------------------------------------------------REAL---------------------------------------------*/

/* ------------------------------------------------------------------------
Name - spawnReal
Purpose - calls fork1 to creates a process and creates the relationship between the parent
                 and the child
Parameters - name, function, argument, stack size, and priority
Returns - child pid
Side Effects -
----------------------------------------------------------------------- */
int spawnReal(char *name, int (*func)(char *), char *arg, int stackSize, int priority) {
    int pid = -1;
    int start3PID = 4;
    /*
    Creating a new process by calling fork1
    */
    pid = fork1(name, spawnLaunch, arg, stackSize, priority);
    /*
    Checking if the process was zapped or if the parent has been terminated
    */
    if (isZapped() || isTerminating(userProcTable[getpid() % MAXPROC].parentPid))
    {
        terminateReal(1);
    }

    /*
    * Returns a negative one if fork1 didn't create a new process
    */
    if (pid < 0)
    {
        return -1;
    }

    int childProcSlot = pid % MAXSLOTS;
    int parentPID = getpid();

    if (parentPID < start3PID)
    {
        parentPID = -1;
    }

    userProcPtr childProc = &userProcTable[childProcSlot];

    MboxSend(childProc->mutexMbox, NULL, 0);

    /*
    Set the process entry
    */
    childProc->myPid = pid;
    childProc->parentPid = parentPID;
    childProc->status = INUSE;
    childProc->func = func;

    MboxReceive(childProc->mutexMbox, NULL, 0);

    if (pid > start3PID)
    {
        int parentProcSlot = parentPID % MAXPROC;
        userProcPtr parentProc = &userProcTable[parentProcSlot];
        addChild(parentProc, childProc);

    }

    MboxSend(childProc->startBox, NULL, 0);

    return pid;

}

/* ------------------------------------------------------------------------
Name - waitReal
Purpose - Calls a join and waits for the child to terminate.  If the process
                is zapped then the process will terminate.
Parameters -status pointer
Returns - -1 if there are no children, else it returns 0;
Side Effects - none
----------------------------------------------------------------------- */
int waitReal(int *status) {

    int pid;

    pid = join(status);
    if (pid == -1 || isZapped())
    {
        terminateReal(*status);
    }

    if (pid == -2)
    {
        return -1;
    }

    return pid;
}

/* ------------------------------------------------------------------------
Name - terminateReal
Purpose - changes proc status to terminating, zaps all children of proc,
            takes proc off userproctable and quits
Parameters - int status, becomes quit status
Returns - nothing
Side Effects - none
----------------------------------------------------------------------- */
void terminateReal(int status) {
    int currentProcSlot = getpid() % MAXPROC;
    userProcTable[currentProcSlot].status = TERMINATING;
    userProcPtr children  = userProcTable[currentProcSlot].child;
    while (children != NULL) {
        int childPid = children->myPid;

        zap(childPid);

        //Should be Blocked Zapped and then Come back here
        children = userProcTable[currentProcSlot].child;
    }
    cleanUpProcess(&userProcTable[currentProcSlot]);
    quit(status);
}

/* ------------------------------------------------------------------------
Name - semcreateReal
Purpose - finds empty spot in semaphore table, creates new semaphore in table
Parameters - int value of semaphore, int pointer semaphore identifier
Returns - -1 if no slots in semtable, 0 for success
Side Effects - none
----------------------------------------------------------------------- */
int semcreateReal(int value, int *semaphore) {
    int i = 0;
    MboxSend(semTable[i].mutexMbox, NULL, 0);

    while (semTable[i].status != FREE) {
        MboxReceive(semTable[i].mutexMbox, NULL, 0);
        i++;
        if (i >= MAXSEMS)
        {
            break;
        }
        MboxSend(semTable[i].mutexMbox, NULL, 0);
    }

    if (i >= MAXSEMS)
    {
        return -1;
    }

    *semaphore = i;
    semTable[i].count = value;
    semTable[i].status = INUSE;

    MboxReceive(semTable[i].mutexMbox, NULL, 0);

    return 0;
}

/* ------------------------------------------------------------------------
Name - sempReal
Purpose - P's semaphore (decreases count), terminates current proc if count = 0
Parameters - int semaphore identifier
Returns - -1 for incorrect semaphore input, 0 for termination or success
Side Effects - possible current proc termination
----------------------------------------------------------------------- */
int sempReal(int semaphore) {
    char string[MAX_MESSAGE];
    if (semaphore < 0 || semaphore >= MAXSEMS)
    {
        return -1;
    }
    semPtr currentSem = &semTable[semaphore];
    MboxSend(currentSem->mutexMbox, NULL, 0);

    if (currentSem->status != INUSE)
    {
        MboxReceive(currentSem->mutexMbox, NULL, 0);
        return -1;
    }

    if (currentSem->count > 0)
    {
        currentSem->count--;
    } else {
        MboxReceive(currentSem->mutexMbox, NULL, 0);
        MboxReceive(currentSem->privateMbox, string, 2);
        if (string[0] == MESSAGE_SEM[0])
        {
            terminateReal(1);
            return 0;
        }
        MboxSend(currentSem->mutexMbox, NULL, 0);
    }
    MboxReceive(currentSem->mutexMbox, NULL, 0);
    return 0;

}

/* ------------------------------------------------------------------------
Name - semvReal
Purpose - V's semaphore (increases count)
Parameters - int semaphore identifier
Returns - -1 for incorrect semaphore input, 0 for success
Side Effects - none
----------------------------------------------------------------------- */
int semvReal(int semaphore) {
    char string[MAX_MESSAGE];
    if (semaphore < 0 || semaphore >= MAXSEMS)
    {
        return -1;
    }
    semPtr currentSem = &semTable[semaphore];
    MboxSend(currentSem->mutexMbox, NULL, 0);

    if (currentSem->status != INUSE)
    {
        MboxReceive(currentSem->mutexMbox, NULL, 0);
        return -1;
    }

    if (currentSem->count > 0)
    {
        currentSem->count++;
        MboxReceive(currentSem->mutexMbox, NULL, 0);
    } else {
        MboxReceive(currentSem->mutexMbox, NULL, 0);
        int conditionalResult = MboxCondSend(currentSem->privateMbox, string, 0);

        if (conditionalResult == -2)
        {
            MboxSend(currentSem->mutexMbox, NULL, 0);
            currentSem->count++;
            MboxReceive(currentSem->mutexMbox, NULL, 0);
        }
    }
    return 0;

}

/* ------------------------------------------------------------------------
Name - semfreeReal
Purpose - free a semaphore from the semtable
Parameters - int semaphore identifier
Returns - -1 for incorrect semaphore input, 1 for success
Side Effects - none
----------------------------------------------------------------------- */
int semfreeReal(int semaphore) {
    char string[] = MESSAGE_SEM;
    int semFreeResult = 0;
    if (semaphore < 0 || semaphore >= MAXSEMS)
    {
        return -1;
    }

    semPtr currentSem = &semTable[semaphore];

    MboxSend(currentSem->mutexMbox, NULL, 0);

    if (currentSem->status != INUSE)
    {

        MboxReceive(currentSem->mutexMbox, NULL, 0);

        return -1;
    }

    while (MboxCondSend(currentSem->privateMbox, string, 1) != -2) {
        semFreeResult = 1;
    }

    currentSem->status = FREE;
    currentSem->count = -1;

    MboxReceive(currentSem->mutexMbox, NULL, 0);

    return semFreeResult;
}

/* ------------------------------------------------------------------------
Name - gettimeofdayReal
Purpose - get and save current time
Parameters - int pointer to timeOfDay
Returns - nothing
Side Effects - pointer value is changed
----------------------------------------------------------------------- */
void gettimeofdayReal(int *timeOfDay) {
    *timeOfDay = USLOSS_Clock();
}

/* ------------------------------------------------------------------------
Name - cpuTimeReal
Purpose - get and save cpu time
Parameters - int pointer to cpuTime
Returns - nothing
Side Effects - pointer value is changed
----------------------------------------------------------------------- */
void cpuTimeReal(int *cpuTime) {
    *cpuTime = readtime();
}

/* ------------------------------------------------------------------------
Name - getpidReal
Purpose - get and save the current pid
Parameters - int pointer to pid
Returns - nothing
Side Effects - pointer value is changed
----------------------------------------------------------------------- */
void getpidReal(int *pid) {
    *pid = getpid();
}
/*---------------------------------------------------lowercase------------------------------------------------------*/

/* ------------------------------------------------------------------------
Name - spawn
Purpose - creates a new user proc from sysArg values by calling spawnReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - changes (clears) sysArg values, switch to usermode
----------------------------------------------------------------------- */
void spawn(systemArgs *sysArg) {
    char *name;
    int (*func)(char *);
    char *arg;
    int stackSize;
    int priority;
    int pid = -1;
    int spawnResult = -1;
    if (isArgsNotNull(sysArg) && matchingSysNumber(SYS_SPAWN, sysArg)) {
        func = (int (*)(char*))(sysArg->arg1);
        arg = (char*)(sysArg->arg2);
        stackSize = (long)(sysArg->arg3);
        priority = (long)(sysArg->arg4);
        name = (char*)(sysArg->arg5);
        if (stackSize >= USLOSS_MIN_STACK && priority < 6 && priority >= 1) {
            pid = spawnReal(name, func, arg, stackSize, priority);
        }

    }
    if (pid != -1)
    {
        spawnResult = 0;
    }
    sysArg->arg1 = (void *)((long)pid);
    sysArg->arg4 = (void *)((long)spawnResult);
    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - wait
Purpose - puts user proc on wait by calling waitReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - changes sysArg values, switch to usermode
----------------------------------------------------------------------- */
void wait(systemArgs *sysArg) {
    int pid = -1;
    int status = -1;
    int waitResult = -1;

    if (matchingSysNumber(SYS_WAIT, sysArg))
    {
        pid = waitReal(&status);
        if (pid != -2)
        {
            waitResult = 0;
        }
    }
    sysArg->arg1 = (void *)((long)pid);
    sysArg->arg2 = (void *)((long)status);
    sysArg->arg4 = (void *)((long) waitResult);
    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - terminate
Purpose - terminates user proc by calling waitReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - terminates process, switch to usermode
----------------------------------------------------------------------- */
void terminate(systemArgs *sysArg) {
    int status = -1;
    if (matchingSysNumber(SYS_TERMINATE, sysArg))
    {
        status = (long)(sysArg->arg1);
        terminateReal(status);
    }
    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - semcreate
Purpose - creates a new semaphore in user mode by calling semcreateReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - creates a semaphore, clears sysArg, swtich to usermode
----------------------------------------------------------------------- */
void semcreate(systemArgs *sysArg) {
    int semaphore = -1;
    int value = -1;
    int semCreateResult = -1;

    if (matchingSysNumber(SYS_SEMCREATE, sysArg))
    {
        value = (long)(sysArg->arg1);
        if (value >= 0)
        {
            semCreateResult = semcreateReal(value, &semaphore);
        }
    }
    (sysArg->arg1) = (void *)((long)semaphore);
    (sysArg->arg4) = (void *)((long)semCreateResult);
    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - semp
Purpose - decreases sem count in user mode by calling sempReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - sem count decreased, sysArg arg4 change, switch to user mode
----------------------------------------------------------------------- */
void semp(systemArgs *sysArg) {
    int semaphore = -1;
    int sempResult = -1;
    if (matchingSysNumber(SYS_SEMP, sysArg))
    {
        semaphore = (long)(sysArg->arg1);
        if (semaphore >= 0)
        {
            sempResult = sempReal(semaphore);
        }
    }
    (sysArg->arg4) = (void *)((long)sempResult);
    SETUSERMODE;

}


/* ------------------------------------------------------------------------
Name - semv
Purpose - increases sem count in user mode by calling semvReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - sem count increased, sysArg arg4 changed, switch to usermode
----------------------------------------------------------------------- */
void semv(systemArgs *sysArg) {
    int semaphore = -1;
    int semvResult = -1;
    if (matchingSysNumber(SYS_SEMV, sysArg))
    {
        semaphore = (long)(sysArg->arg1);
        if (semaphore >= 0)
        {
            semvResult = semvReal(semaphore);
        }
    }
    (sysArg->arg4) = (void *)((long)semvResult);
    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - semfree
Purpose - frees a semaphore in user mode by calling semfreeReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - sem released, sysArg's arg4 changed, switch to user mode
----------------------------------------------------------------------- */
void semfree(systemArgs *sysArg) {
    int semaphore = -1;
    int semFreeResult = -1;
    if (matchingSysNumber(SYS_SEMFREE, sysArg))
    {
        semaphore = (long)(sysArg->arg1);
        if (semaphore >= 0)
        {
            semFreeResult = semfreeReal(semaphore);
        }
    }
    (sysArg->arg4) = (void *)((long)semFreeResult);

    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - gettimeofday
Purpose - get time of day in user mode by calling gettimeofdayReal
Parameters - systemArgs ponter sysArg
Returns - nothing
Side Effects -  sysArg's arg1 changed to time of day, switch to usermode
----------------------------------------------------------------------- */
void gettimeofday(systemArgs *sysArg) {
    int timeOfDay;
    if (matchingSysNumber(SYS_GETTIMEOFDAY, sysArg))
    {
        gettimeofdayReal(&timeOfDay);
    }
    (sysArg->arg1) = (void *)((long)timeOfDay);
    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - cputime
Purpose - get cputime in user mode by calling cpuTimeReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - sysArg's arg1 becomes cpu time, switch to usermode
----------------------------------------------------------------------- */
void cputime(systemArgs *sysArg) {
    int cpu;
    if (matchingSysNumber(SYS_CPUTIME, sysArg))
    {
        cpuTimeReal(&cpu);
    }
    (sysArg->arg1) = (void *)((long)cpu);
    SETUSERMODE;
}

/* ------------------------------------------------------------------------
Name - get_pid
Purpose - get current proc's pid in user mode by calling getpidReal
Parameters - systemArgs pointer sysArg
Returns - nothing
Side Effects - sysArg's arg1 changed to current pid
----------------------------------------------------------------------- */
void get_pid(systemArgs *sysArg) {
    int pid;
    if (matchingSysNumber(SYS_GETPID, sysArg))
    {
        getpidReal(&pid);
    }
    (sysArg->arg1) = (void *)((long)pid);
    SETUSERMODE;
}

/*------------------------------------------helper methods-------------------------------------------------*/

/* ------------------------------------------------------------------------
   Name - processInitialize
   Purpose - initializes empty/initial process values in user proc table
   Parameters - none
   Returns - none
   Side Effects - user process values set to initial values
   ----------------------------------------------------------------------- */
void processInitialize(void) {
    for (int i = 0; i < MAXPROC; ++i)
    {
        userProcTable[i].status = FREE;
        userProcTable[i].mutexMbox = MboxCreate(1, MAX_MESSAGE);
        userProcTable[i].startBox = MboxCreate(1, MAX_MESSAGE);
        userProcTable[i].privateMbox = MboxCreate(0, MAX_MESSAGE);
        userProcTable[i].myPid = -1;
        userProcTable[i].parentPid = -1;
        userProcTable[i].sibling = NULL;
        userProcTable[i].child = NULL;
        userProcTable[i].func = NULL;
    }

}

/* ------------------------------------------------------------------------
Name - semaphoresInitialize
Purpose - initializes semaphore fields to empty/initial values
Parameters - none
Returns - none
Side Effects - semaphore values set to initial values
----------------------------------------------------------------------- */
void semaphoresInitialize(void) {
    for (int i = 0; i < MAX_SEMS; ++i)
    {
        semTable[i].status = FREE;
        semTable[i].count = -1;
        semTable[i].mutexMbox = MboxCreate(1, MAX_MESSAGE);
        semTable[i].privateMbox = MboxCreate(0, MAX_MESSAGE);
    }

}

/* ------------------------------------------------------------------------
Name - systemVecInitialize
Purpose - initialize the System Vec int.  First it sets all to nullsys3
                and then it sets the unique Syscalls to their functions
Parameters - none
Returns - none
Side Effects - systemCallVec values initizalized to proper functions
----------------------------------------------------------------------- */
void systemVecInitialize(void) {

    for (int i = 0; i < MAXSYSCALLS; ++i)
    {
        systemCallVec[i] = nullsys3;
    }


    systemCallVec[SYS_SPAWN] = spawn;
    systemCallVec[SYS_WAIT] = wait;
    systemCallVec[SYS_TERMINATE] = terminate;
    systemCallVec[SYS_SEMCREATE] = semcreate;
    systemCallVec[SYS_SEMP] = semp;
    systemCallVec[SYS_SEMV] = semv;
    systemCallVec[SYS_SEMFREE] = semfree;
    systemCallVec[SYS_GETTIMEOFDAY] = gettimeofday;
    systemCallVec[SYS_CPUTIME] = cputime;
    systemCallVec[SYS_GETPID] = get_pid;



}

/* ------------------------------------------------------------------------
Name - nullsys3
Purpose - terminates a user proc by calling terminateReal
Parameters - systemArgs pointer args
Returns - nothing
Side Effects - current process terminated
----------------------------------------------------------------------- */
void nullsys3(systemArgs *args) {
    terminateReal(0);
}

/* ------------------------------------------------------------------------
Name - isArgsNotNull
Purpose - checks if sysArg fields arg1, arg3, arg4, arg5 are null
Parameters - systemArgs pointer sysArg
Returns - non-zero if sysArg fields are filled (success), 0 if one is null
Side Effects - none
----------------------------------------------------------------------- */
int isArgsNotNull(systemArgs *sysArg) {
    return (sysArg->arg1) != NULL &&  (sysArg->arg3) != NULL && (sysArg->arg4) != NULL && (sysArg->arg5) != NULL;
}

/* ------------------------------------------------------------------------
Name - matchingSysNumber
Purpose - checks that sysArg's number is the same as expected arg's SYS_OPCODE
Parameters - int SYS_OPCODE and systemArg pointer sysArg
Returns - 0 for false, 1 for true
Side Effects - none
----------------------------------------------------------------------- */

int matchingSysNumber(int SYS_OPCODE, systemArgs *sysArg) {
    return sysArg->number == SYS_OPCODE;
}

/* ------------------------------------------------------------------------
Name - addChild
Purpose - adds a child to a user process's list of children
Parameters - userProcPtrs parentProc and childProc
Returns - nothing
Side Effects - nothing
----------------------------------------------------------------------- */
void addChild(userProcPtr parentProc, userProcPtr childProc) {
    if (parentProc != NULL && childProc != NULL && parentProc != childProc)
    {
        MboxSend(parentProc->mutexMbox, NULL, 0);
        MboxSend(childProc->mutexMbox, NULL, 0);

        childProc->sibling = parentProc->child;
        parentProc->child = childProc;

        MboxReceive(childProc->mutexMbox, NULL, 0);
        MboxReceive(parentProc->mutexMbox, NULL, 0);
    }
}

/* ------------------------------------------------------------------------
Name - isTerminating
Purpose - checks if a user process is terminating
Parameters - int pid of process
Returns - 1 for terminating process, 0 for invalid pid or if proc is not terminating
Side Effects - none
----------------------------------------------------------------------- */
int isTerminating(int pid) {
    if (pid != -1 && userProcTable[pid % MAXPROC].status == TERMINATING)
    {
        return 1;
    } else {
        return 0;
    }

}

/* ------------------------------------------------------------------------
Name -cleanUpProcess
Purpose - deletes process from its parent's child list
Parameters - userProcPtr currentProcess
Returns - -2 for null currentProcess, -1 if there are no more children, pid for success
Side Effects - parent's list changed
----------------------------------------------------------------------- */
int cleanUpProcess(userProcPtr currentProcess) {
    int myPid = -1;
    int myParentsPid = -1;

    if (currentProcess == NULL)
    {
        return -2;
    }

    myPid = currentProcess->myPid;
    myParentsPid = currentProcess->parentPid;

    //Start3 check
    if (myParentsPid == -1)
    {
        clearProcess(currentProcess);
        return myPid;
    }

    userProcPtr parent = &userProcTable[myParentsPid % MAXPROC];
    userProcPtr child = parent->child;
    userProcPtr nextChild = NULL;
    userProcPtr deletingProcess = NULL;
    userProcPtr temp = NULL;

    if (child == NULL)
    {
        return -2;
    }

    if (child->myPid == myPid)
    {
        parent->child = child->sibling;
        deletingProcess = child;
    } else {
        nextChild = child->sibling;

        if (nextChild == NULL) {
            return -1;
        }

        while (nextChild != NULL && nextChild->myPid != myPid) {

            child = nextChild;
            nextChild = nextChild ->sibling;
        }

        if (nextChild != NULL)
        {
            temp = nextChild->sibling;
            child->sibling = temp;
            deletingProcess = nextChild;

        } else {

            return -1;

        }
    }

    if (deletingProcess != NULL)
    {
        clearProcess(deletingProcess);
    }

    return myPid;
}

/* ------------------------------------------------------------------------
Name - clearProcess
Purpose - returns user process field's to initial/cleared values
Parameters - userProcPtr deletingProcess
Returns - nothing
Side Effects - deletingProcess' values are changed
----------------------------------------------------------------------- */
void clearProcess(userProcPtr deletingProcess) {
    if (deletingProcess != NULL)
    {
        MboxSend(deletingProcess->mutexMbox, NULL, 0);
        deletingProcess->status = FREE;
        deletingProcess->myPid = -1;
        deletingProcess->parentPid = -1;
        deletingProcess->func = NULL;
        deletingProcess->sibling = NULL;
        deletingProcess->child = NULL;
        MboxReceive(deletingProcess->mutexMbox, NULL, 0);
    }
}

/* ------------------------------------------------------------------------
Name - spawnLaunch
Purpose - spawns user process
Parameters - char pointer arg
Returns - int functionResult from func()
Side Effects - switch to user mode
----------------------------------------------------------------------- */
int spawnLaunch(char *arg) {
    int functionResult;
    int currentProcSlot = getpid() % MAXPROC;
    int (*func)(char *);
    userProcPtr currentProcess = &userProcTable[currentProcSlot];

    MboxReceive(currentProcess->startBox, NULL, 0);
    if (isZapped() || isTerminating(currentProcess->parentPid))
    {
        terminateReal(1);
    }

    MboxSend(currentProcess->mutexMbox, NULL, 0);
    if (isZapped() || isTerminating(currentProcess->parentPid))
    {
        MboxReceive(currentProcess->mutexMbox, NULL, 0);
        terminateReal(1);
    }
    func = currentProcess->func;
    MboxReceive(currentProcess->mutexMbox, NULL, 0);

    SETUSERMODE;

    functionResult = func(arg);
    Terminate(functionResult);

    return functionResult;
}