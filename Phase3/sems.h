
#define MAX_SEMS 200

typedef struct userProcStruct userProcStruct;
typedef struct userProcStruct *userProcPtr;

struct userProcStruct {
    int status;
    int mutexMbox;
    int startBox;
    int privateMbox;
    short myPid;
    short parentPid;
    userProcPtr sibling;
    userProcPtr child;
    int (*func)(char*);
};

typedef struct semStruct semStruct;
typedef struct semStruct *semPtr;

struct semStruct {
    int status;
    int count;
    int mutexMbox;
    int privateMbox;
};

#define FREE 0
#define INUSE 1
#define TERMINATING 2


