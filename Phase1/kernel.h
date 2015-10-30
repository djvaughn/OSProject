/* ------------------------------------------------------------------------
   kernel.h

   University of Arizona
   Computer Science 452
   Fall 2015
   Daniel Vaughn and Meg Dever-Hansen

   ------------------------------------------------------------------------ */
/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
  procPtr                                   nextProcPtr;       /* points to the next process on the ready list*/
  procPtr                                   childProcPtr;      /* points to the child of this process*/
  procPtr                                   nextSiblingPtr;    /* points to the sibling of this process*/
  procPtr                                   parentPtr;    /* points to the sibling of this process*/
  procPtr                                   childQuitList;
  procPtr                                   nextQuitChild;
  procPtr                                   zapper; //who zapped this process
  procPtr                                   nextZapper;
  procPtr                                   zappee; //who this process zapped
  int                                           childCounter;
  int                                           zapped;
  char                                        name[MAXNAME];     /* process's name */
  char                                        startArg[MAXARG];  /* args passed to process */
  USLOSS_Context                  state;             /* current context for process */
  short                                       pid;               /* process id */
  int                                           priority;
  int (* start_func) (char *);   /* function where process begins -- launch */
  char                                       *stack;
  unsigned int                           stackSize;
  int                                          status;        /* READY, BLOCKED, QUIT, etc. */
  int                                          startTime;
  int                                          totalRunTime;
  int                                          returnStatus;
  int                                          parentPid;
  /* other fields as needed... */
};

struct psrBits {
  unsigned int curMode: 1;
  unsigned int curIntEnable: 1;
  unsigned int prevMode: 1;
  unsigned int prevIntEnable: 1;
  unsigned int unused: 28;
};

union psr_values {
  struct psrBits bits;
  unsigned int integerPart;
};

/* Some useful constants.  Add more as needed... */
#define SENTINELPID 1
#define QUANTUM     80

/*PROCESS CONSTANTS*/
#define NO_CURRENT_PROCESS NULL
#define NO_PROCESS                    -1
#define READY                                1
#define RUNNING                            2
#define QUIT                                    3
#define BLOCK_JOIN                      4
#define BLOCK_ZAP                       5
#define BLOCK                                10




/*PRIORITY CONSTANTS*/
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPRIORITY (MINPRIORITY + 1)
