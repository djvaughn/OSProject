
#include "usloss.h"
#define DEBUG 0
extern int debugflag;
extern int virtualMemInitialized
extern void getPageTable(int pid);
extern void swapPageTable(int oldProcessPid, int newProcessPid);

void
p1_fork(int pid)
{
    if(virtualMemInitialized){
        getPageTable(pid);
    }
} /* p1_fork */

void
p1_switch(int old, int new)
{
    if(virtualMemInitialized){
        swapPageTable(old, new);
    }

} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

