/*
*Driver.h
*authors Daniel Vaughn and Meg Dever Hanson
*/

#ifndef DRIVER_H_
#define DRIVER_H_

#define DEBUG4 0

#define MAXDEVPIDS 10
#define NOTUSEDPID -1
#define CLOCKPID 0

#define CHECKMODE(funcName) {                     \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) {                \
        USLOSS_Console("Trying to preform kernel function while in user mode\n", funcName);   \
        USLOSS_Halt(1);                     \
    }                           \
}


typedef int semaphore;
typedef struct driverProcStruct driverProcStruct;
typedef struct driverProcStruct *driverProcPtr;

struct driverProcStruct
{
    int privateMbox;
    int mutexMbox;
    driverProcPtr nextProcessAsleep;
    int sleepTime;
    int isRead;
    driverProcPtr prevReadWrite;
    driverProcPtr nextReadWrite;
    int start;
    int end;
    void* buffer;
};

#endif
