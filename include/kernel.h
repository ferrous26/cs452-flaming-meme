#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <std.h>
#include <stdarg.h>
#include <syscall.h>
#include <tasks/name_server_kernel.h>
#include <tasks/clock_server_kernel.h>

#define TEXT_HOT  __attribute__ ((section (".text.kern")))
#define DATA_HOT  __attribute__ ((section (".data.kern")))
#define DATA_WARM __attribute__ ((section (".data.kern.warm")))
#define DATA_HOT  __attribute__ ((section (".data.kern")))

// we want to lockdown 64 task descriptors into the cache
#define TASKS_TO_LOCKDOWN 64

// choose small values so they can be instruction immediates
#define RECV_BLOCKED (task*)0xA
#define RPLY_BLOCKED (task*)0xB

typedef enum {
    SYS_IRQ,
    SYS_CREATE,
    SYS_TID,
    SYS_PTID,
    SYS_PASS,
    SYS_EXIT,
    SYS_PRIORITY,
    SYS_SEND,
    SYS_RECV,
    SYS_REPLY,
    SYS_CHANGE,
    SYS_AWAIT,
    SYS_SHUTDOWN,
    SYS_ABORT,
    SYS_COUNT
} syscall_num;

typedef struct {
    int  priority;
    void (*code)(void);
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

typedef struct {
    int eventid;
    char* event;
    int eventlen;
} kreq_event;

typedef struct {
    char*    file;
    uint     line;
    char*    msg;
    va_list* args;
} kreq_abort;


void kernel_init(void) TEXT_COLD;
void kernel_deinit(void) TEXT_COLD;

void hwi_enter(void);                             /* found in context.asm */
void kernel_enter(unsigned int code, void* req);  /* found in context.asm */

void syscall_handle(const syscall_num code, const void* const req, int* const sp)
    __attribute__ ((naked)) TEXT_HOT;

void __attribute__ ((noreturn)) shutdown(void) TEXT_COLD;
void __attribute__ ((noreturn)) abort(const kreq_abort* const req) TEXT_COLD;


typedef int32  task_id;
typedef int32  task_pri;
typedef uint8  task_idx;

typedef struct task_q_pointers {
    struct task_descriptor* head;
    struct task_descriptor* tail;
} task_q;

typedef struct task_descriptor {
    task_id                 tid;
    task_id                 p_tid;
    task_pri                priority;
    int*                    sp;
    struct task_descriptor* next;
    struct task_q_pointers  recv_q;
} task;

extern task* task_active;
extern task* int_queue[EVENT_COUNT];

void scheduler_first_run(void) TEXT_COLD;

/**
 * The non-inlined, but still important, version of scheduler_schedule().
 */
void scheduler_reschedule(task* const t) TEXT_HOT;

#endif
