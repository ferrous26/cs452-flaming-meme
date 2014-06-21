#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <std.h>
#include <stdarg.h>

#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/train_server.h>
#include <tasks/clock_server.h>

typedef enum {
    OK               =  0, // no problems, full steam ahead
    NO_DESCRIPTORS   = -1, // Create(2) failed because descriptor table is exhausted
    IMPOSSIBLE_TASK  = -1, // task id was negative
    INVALID_EVENT    = -1,
    INVALID_PRIORITY = -2, // Create(2) failed because invalid priority was given
    INVALID_TASK     = -2, // task no longer alive (or has not yet been created)
    CORRUPT_DATA     = -2,
    INCOMPLETE       = -3, // receiver Exit()'d before receiving the message
    INVALID_RECVER   = -3, // receiver was not in the RPLY_BLOCKED state
    NOT_ENUF_MEMORY  = -4, // receiver buffer was not big enough
    INVALID_MESSAGE  = -5, // the receiver did not understand the message
    INVALID_CHANNEL  = -6
} err;

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


#define Putc(channel, ch) Putc_##channel(ch)
#define Putc_TERMINAL(ch) put_term_char(ch)
#define Putc_2(ch)        Putc_TERMINAL(ch)
#define Putc_TRAIN(ch)    put_train_char(ch)
#define Putc_1(ch)        Putc_train(ch);

#define Getc(channel) Getc_##channel
#define Getc_TERMINAL get_term_char()
#define Getc_2        Getc_TERMINAL
#define Getc_TRAIN    get_train_char()
#define Getc_1        Getc_TRAIN

#endif
