
#define DEBUG2 1

typedef struct mailSlot mailSlot;
typedef struct mailSlot *slotPtr;

typedef struct mailbox   mailbox;
typedef struct mailbox  *mailBoxPtr;

typedef struct mboxProc *mboxProcPtr;

typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;

struct mailbox {
    int       mBoxId;
    int       totalNumberSlots;
    int       slotSize;
    int       status;
    int       slotsInUse;
    slotPtr usedSlotList;
    procPtr blockedProcessList;

    // other items as needed...
};

struct mailSlot {
    int       mBoxId;
    int       status;
    int       isConditional;
    char                                       message[MAX_MESSAGE];
    int                                          messageSize;
    slotPtr nextUsedSlot;
    // other items as needed...
};

struct procStruct {
    short                                      pid;               /* process id */
    int                                          status;        /* READY, BLOCKED, QUIT, etc. */
    int                                          zapped;
    char                                       message[MAX_MESSAGE];
    int                                          messageSize;
    procPtr                                  nextBlocked;
    /* other fields as needed... */
};

struct psrBits {
    unsigned int curMode: 1;
    unsigned int curIntEnable: 1;
    unsigned int prevMode: 1;
    unsigned int prevIntEnable: 1;
    unsigned int unused: 28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};

#define EMPTY 0
#define ACTIVE 1
#define SENDBLOCK 11
#define RECEIVEBLOCK 12
/*
extern void nullsys(sysargs *args);
extern void clockHandler2(int dev, int unit);
extern void diskHandler(int dev, int unit);
extern void termHandler(int dev, int unit);
extern void syscallHandler(int dev, int unit);
extern int termMailBox[USLOSS_TERM_UNITS];
extern int clockMailBox[USLOSS_CLOCK_UNITS];
extern int diskMailBox[USLOSS_DISK_UNITS];
*/