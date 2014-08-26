#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <std.h>
#include <stdarg.h>

#define TASK_MAX 512 // Maximum number of live tasks in the system

typedef enum {
    OK                   =  0, // no problems, full steam ahead
    ERR_NO_DESCRIPTORS   = -1, // Create(2) failed because task table is full
    ERR_IMPOSSIBLE_TASK  = -1, // task id was negative
    ERR_INVALID_EVENT    = -1,
    ERR_INVALID_PRIORITY = -2, // invalid priority was given
    ERR_INVALID_TASK     = -2, // task no longer alive, or not yet been created
    ERR_CORRUPT_DATA     = -2,
    ERR_INCOMPLETE       = -3, // receiver Exit()'d before receiving message
    ERR_INVALID_RECVER   = -3, // replyee was not in the RPLY_BLOCKED state
    ERR_NOT_ENUF_MEMORY  = -4, // receiver/replyee buffer was not big enough
    ERR_INVALID_MESSAGE  = -5, // the receiver did not understand the message
    ERR_INVALID_CHANNEL  = -6,
    ERR_REQUEST_REJECTED = -7,
    ERR_MISSED_DEADLINE  = -8
} err;

enum task_priority_level {
    TASK_PRIORITY_MIN         = 0,
    TASK_PRIORITY_IDLE        = 0,
    TASK_PRIORITY_0           = 0,
    TASK_PRIORITY_1           = 1,
    TASK_PRIORITY_2           = 2,
    TASK_PRIORITY_3           = 3,
    TASK_PRIORITY_4           = 4,
    TASK_PRIORITY_LOW         = 5,
    TASK_PRIORITY_5           = 5,
    TASK_PRIORITY_6           = 6,
    TASK_PRIORITY_7           = 7,
    TASK_PRIORITY_8           = 8,
    TASK_PRIORITY_9           = 9,
    TASK_PRIORITY_MEDIUM_LOW  = 10,
    TASK_PRIORITY_10          = 10,
    TASK_PRIORITY_11          = 11,
    TASK_PRIORITY_12          = 12,
    TASK_PRIORITY_13          = 13,
    TASK_PRIORITY_14          = 14,
    TASK_PRIORITY_MEDIUM      = 15,
    TASK_PRIORITY_15          = 15,
    TASK_PRIORITY_16          = 16,
    TASK_PRIORITY_17          = 17,
    TASK_PRIORITY_18          = 18,
    TASK_PRIORITY_19          = 19,
    TASK_PRIORITY_MEDIUM_HIGH = 20,
    TASK_PRIORITY_20          = 20,
    TASK_PRIORITY_21          = 21,
    TASK_PRIORITY_22          = 22,
    TASK_PRIORITY_23          = 23,
    TASK_PRIORITY_24          = 24,
    TASK_PRIORITY_HIGH        = 25,
    TASK_PRIORITY_25          = 25,
    TASK_PRIORITY_26          = 26,
    TASK_PRIORITY_27          = 27,
    TASK_PRIORITY_28          = 28,
    TASK_PRIORITY_29          = 29,
    TASK_PRIORITY_30          = 30,
    TASK_PRIORITY_EMERGENCY   = 31,
    TASK_PRIORITY_31          = 31,
    TASK_PRIORITY_MAX         = 31,
    TASK_PRIORITY_LEVELS      = 32
};

task_id Create(const task_priority priority, void (*const code) (void));
task_id myTid(void);
task_id myParentTid(void);
task_priority myPriority(void);
task_priority ChangePriority(const task_priority priority);

void Pass(void);
void Exit(void);

int Send(const task_id tid,
         const void* const msg, const size_t msglen,
         void* const reply,     const size_t replylen);
int Receive(task_id* const tid,
            void* const msg, const size_t msglen);
int Reply(const task_id tid,
          void* const reply, const size_t replylen);


typedef enum event_identifier {
    CLOCK_TICK,
    UART2_SEND,
    UART2_RECV,
    UART1_SEND,
    UART1_RECV,
    UART1_CTS,
    UART1_DOWN
} event_id;
#define EVENT_COUNT (UART1_DOWN + 1)

int AwaitEvent(const event_id eid,
               void* const event, const size_t eventlen);

/**
 * http://i0.kym-cdn.com/photos/images/original/000/056/667/madagascar.gif
 *
 * This function does not return and it does not give other tasks an
 * opportunity to shutdown.
 */
void __attribute__ ((noreturn))
Shutdown(void);

void __attribute__ ((noreturn))
Abort(const char* const file, const uint line, const char* const msg, ...);
#define ABORT(msg, ...) Abort(__FILE__, __LINE__, msg, ## __VA_ARGS__)

#endif
