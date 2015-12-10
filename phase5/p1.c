
#include <usloss.h>
#include "phase5.h"
#include "vm.h"
#define DEBUG 0
extern int debugflag;

extern int vmStarted();
extern void getPTETable(int pid);
extern void tableSwap(int oldPid, int newPid);
extern void releaseFrames(int pid);
extern void releasePTETable(int pid);

void
p1_fork(int pid)
{
    if (vmStarted()) {

        if (DEBUG && debugflag)
            USLOSS_Console("p1_fork() called: pid = %d\n", pid);

        getPTETable(pid);

    }
} /* p1_fork */

void
p1_switch(int oldPid, int newPid)
{
    if (vmStarted()) {
        if (DEBUG && debugflag)
            USLOSS_Console("p1_switch() called: old = %d, new = %d\n", oldPid, newPid);

        vmStats.switches++;
        tableSwap(oldPid, newPid);

    }
} /* p1_switch */

void
p1_quit(int pid)
{
    if (vmStarted()) {
        releaseFrames(pid % MAXPROC);
        releasePTETable(pid % MAXPROC);
        if (DEBUG && debugflag)
            USLOSS_Console("p1_quit() called: pid = %d\n", pid);
    }
    /* p1_quit */
}
