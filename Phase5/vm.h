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
#define ONBOTH 503
/* You'll probably want more states */

typedef PTE *pagePointer;
/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;



/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    int privateMbox;
    int mutexMbox;
    int currentPage;
    int mappings;
    int numPages;
    //private nailbox
    // Add more stuff here */
} Process;

/*
 * Per-process information.
 */
typedef struct FTE {
    int  state;
    int pid;
    int page;
    //fault mailbox
    //private nailbox
    // Add more stuff here */
} FTE;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    int frame;
    int offset;
    // Add more stuff here.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
