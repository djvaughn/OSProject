/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
#define ONDISK 502
#define BOTH   503
#define CLEAN  504
#define DIRTY  505
#define REFERENCED    506
#define EMPTY  508
/* You'll probably want more states */


extern int checkvmStatus(void);
/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    int clean;
    int ref;
    int previousAccess;
} PTE;

typedef struct FTE {
    int state;
    int page;
    int pid;
    int cleanBit;
    int ref;


} FTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    int  pid;
    int  mailbox;
    int currentPage;
    int mappings;
    char buffer[MAXLINE + 1];
    // Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    int  frame;
    int offset;
    // Add more stuff here.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)


