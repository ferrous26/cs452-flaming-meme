#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <std.h>
#include <stdarg.h>

typedef enum {
    OK               =  0, // no problems, full steam ahead
    NO_DESCRIPTORS   = -1, // Create(2) failed because descriptor table is exhausted
    IMPOSSIBLE_TASK  = -1, // task id was negative
    INVALID_EVENT    = -1,
    INVALID_PRIORITY = -2, // Create(2) failed because invalid priority was given
    INVALID_TASK     = -2, // task no longer alive (or has not yet been created)
    CORRUPT_DATA     = -2,
    INCOMPLETE       = -3, // receiver Exit()'d before receiving the message
    INVALID_RECVER   = -3, // replyee was not in the RPLY_BLOCKED state
    NOT_ENUF_MEMORY  = -4, // receiver/replyee buffer was not big enough
    INVALID_MESSAGE  = -5, // the receiver did not understand the message
    INVALID_CHANNEL  = -6
} err;

#define TASK_MAX 256 // Maximum number of live tasks in the system

#define TASK_PRIORITY_LEVELS 32
#define TASK_PRIORITY_MAX    31
#define TASK_PRIORITY_MIN     0

typedef enum {
    TASK_PRIORITY_IDLE,
    TASK_PRIORITY_0 = 0,
    TASK_PRIORITY_1,
    TASK_PRIORITY_2,
    TASK_PRIORITY_3,
    TASK_PRIORITY_4,
    TASK_PRIORITY_LOW,
    TASK_PRIORITY_5 = 5,
    TASK_PRIORITY_6,
    TASK_PRIORITY_7,
    TASK_PRIORITY_8,
    TASK_PRIORITY_9,
    TASK_PRIORITY_MEDIUM_LOW,
    TASK_PRIORITY_10 = 10,
    TASK_PRIORITY_11,
    TASK_PRIORITY_12,
    TASK_PRIORITY_13,
    TASK_PRIORITY_14,
    TASK_PRIORITY_MEDIUM,
    TASK_PRIORITY_15 = 15,
    TASK_PRIORITY_16,
    TASK_PRIORITY_17,
    TASK_PRIORITY_18,
    TASK_PRIORITY_19,
    TASK_PRIORITY_MEDIUM_HIGH,
    TASK_PRIORITY_20 = 20,
    TASK_PRIORITY_21,
    TASK_PRIORITY_22,
    TASK_PRIORITY_23,
    TASK_PRIORITY_24,
    TASK_PRIORITY_HIGH,
    TASK_PRIORITY_25 = 25,
    TASK_PRIORITY_26,
    TASK_PRIORITY_27,
    TASK_PRIORITY_28,
    TASK_PRIORITY_29,
    TASK_PRIORITY_30,
    TASK_PRIORITY_EMERGENCY,
    TASK_PRIORITY_31 = 31
} task_priority;

int Create(int priority, void (*code) (void));
int myTid(void);
int myParentTid(void);
int myPriority(void);
int ChangePriority(int priority);

void Pass(void);
void Exit(void);

int Send(int tid, char* msg, int msglen, char* reply, int replylen);
int Receive(int* tid, char* msg, int msglen);
int Reply(int tid, char* reply, int replylen);


enum event_id {
    CLOCK_TICK,
    UART2_SEND,
    UART2_RECV,
    UART1_SEND,
    UART1_RECV,
    UART1_CTS,
    UART1_DOWN,
    EVENT_COUNT
};

int AwaitEvent(int eventid, char* event, int eventlen);

/**
 * http://i0.kym-cdn.com/photos/images/original/000/056/667/madagascar.gif
 *
 * This function does not return and it does not give other tasks an
 * opportunity to shutdown.
 */
void __attribute__ ((noreturn)) Shutdown(void);

#define ABORT(msg, ...) Abort(__FILE__, __LINE__, msg, ## __VA_ARGS__)

void __attribute__ ((noreturn))
Abort(const char* const file, const uint line, const char* const msg, ...);

#endif
