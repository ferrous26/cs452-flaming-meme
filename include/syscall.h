#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <std.h>
#include <limits.h>

#define SWI_HANDLER ((uint*)0x08)

#define SYS_IRQ      0

#define SYS_CREATE   1
#define SYS_TID      2
#define SYS_PTID     3
#define SYS_PASS     4
#define SYS_EXIT     5
#define SYS_PRIORITY 6

#define SYS_SEND     7
#define SYS_RECV     8
#define SYS_REPLY    9
#define SYS_CHANGE   10

#define SAVED_REGISTERS 13
#define TRAP_FRAME_SIZE (SAVED_REGISTERS * WORD_SIZE)

#define START_ADDRESS(fn) ((uint)fn)
#define EXIT_ADDRESS      START_ADDRESS(Exit)
#define DEFAULT_SPSR      0x50  //no fiq

typedef enum {
    NO_DESCRIPTORS   = -1,
    INVALID_PRIORITY = -2
} task_err;

typedef enum {
    IMPOSSIBLE_TASK = -1, // task id is negative
    INVALID_TASK    = -2, // task no longer alive (or has not yet been created)
    INCOMPLETE      = -3,
    INVALID_RECVER  = -3,
    NOT_ENUF_MEMORY = -4
} msg_err;

typedef struct {
    uint  priority;
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
    int*  tid;
    char* msg;
    int   msglen;
} kreq_recv;

typedef struct {
    int   tid;
    char* reply;
    int   replylen;
} kreq_reply;

void syscall_init();
void kernel_enter(unsigned int code, void* req);  /* found in context.asm */
int  kernel_exit(unsigned int* sp);               /* found in context.asm */

int Create(int priority, void (*code) (void));

int myTid(void);
int myParentTid(void);
int myPriority(void);
void ChangePriority(uint);

void Pass(void);
void Exit(void);

int Send(int tid, char* msg, int msglen, char* reply, int replylen);
int Receive(int* tid, char* msg, int msglen);
int Reply(int tid, char* reply, int replylen);

int WhoIs(char* name);
int RegisterAs(char* name);

#endif

