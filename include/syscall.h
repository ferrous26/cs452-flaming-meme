#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <std.h>
#include <limits.h>

/**
 * The notes very clearly state that we want address 0x08. If that value
 * is wrong then something is euchered or we should tell/ask the professor
 * about the problem.
 *
 * http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/s14/notes/l05.html
 */
#define SWI_HANDLER ((uint*)0x08)
#define HWI_HANDLER ((uint*)0x18)

#define SYS_CREATE   1
#define SYS_TID      2
#define SYS_PTID     3
#define SYS_PASS     4
#define SYS_EXIT     5
#define SYS_PRIORITY 6

#define SYS_SEND     7
#define SYS_REPLY    8
#define SYS_RECEIVE  9


#define SAVED_REGISTERS 13
#define TRAP_FRAME_SIZE (SAVED_REGISTERS * WORD_SIZE)

#define OS_OFFSET         0
#define START_ADDRESS(fn) ((uint)fn) //   + OS_OFFSET
#define EXIT_ADDRESS      START_ADDRESS(Exit)
#define DEFAULT_SPSR      0xD0 // TODO: change back to 0x10 for IRQ

typedef enum {
    NO_DESCRIPTORS   = -1,
    INVALID_PRIORITY = -2
} task_err;

typedef struct {
    int  priority;
    void (*code) (void);
} kreq_create;

typedef struct {
    int   tid;
    char* msg;
    int   msglen;
    char* reply;
    int   replylen;
} kreq_send;

typedef struct {
    int   tid;
    char* reply;
    int   replylen;
} kreq_reply;

typedef struct {
    int*  tid;
    char* msg;
    int   msglen;
} kreq_recv;

void kernel_enter(unsigned int code);  /* found in context.asm */
int  kernel_exit(unsigned int *sp);    /* found in context.asm */

int Create(int priority, void (*code) (void));

int myTid(void);
int myParentTid(void);
int myPriority(void);

void Pass(void);
void Exit(void);

int Send(int tid, char *msg, int msglen, char *reply, int replylen);
int Reply(int tid, char *reply, int replylen);
int Receive(int *tid, char *msg, int msglen); 

#endif
