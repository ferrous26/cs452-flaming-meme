#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <std.h>
#include <stdarg.h>
#include <syscall.h>

#define TEXT_HOT __attribute__ ((section (".text.kern")))
#define DATA_HOT __attribute__ ((section (".data.kern")))

// choose small values so they can be instruction immediates
#define RECV_BLOCKED (task*)0xA
#define RPLY_BLOCKED (task*)0xB

enum int_num {
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
    SYS_ABORT
};

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


void scheduler_init(void);
void ksyscall_init(void);
void ksyscall_deinit(void);

void kernel_enter(unsigned int code, void* req);  /* found in context.asm */
void kernel_exit(int* sp);                        /* found in context.asm */
void syscall_handle(const uint code, const void* const req, int* const sp)
    __attribute__ ((naked)) TEXT_HOT;

void __attribute__ ((noreturn)) shutdown(void);
void __attribute__ ((noreturn)) abort(const kreq_abort* const req);


#define TASK_HEAP_TOP 0x1F00000 // 31 MB
#define TASK_HEAP_BOT 0x0300000 //  3 MB
#define TASK_HEAP_SIZ 0x40000   // 64 pages * 4096 bytes per page

typedef int32  task_id;
typedef int32  task_pri;
typedef uint8  task_idx;

typedef struct task_descriptor {
    task_id                 tid;
    task_id                 p_tid;
    task_pri                priority;
    int*                    sp;
    struct task_descriptor* next;
} task;

typedef struct task_q_pointers {
    task* head;
    task* tail;
} task_q;

extern task_q recv_q[TASK_MAX];
extern task   tasks[TASK_MAX];

extern task*  task_active;
extern task*  int_queue[EVENT_COUNT];

static inline uint __attribute__ ((const)) task_index_from_tid(const task_id tid) {
    return mod2((uint32)tid, TASK_MAX);
}

void scheduler_get_next(void) TEXT_HOT;
void scheduler_schedule(task* const t) TEXT_HOT;

/**
 * @return tid of new task, or an error code as defined by CreateTask()
 */
int  task_create(const task_pri pri, void (*const start)(void)) TEXT_HOT;
void task_destroy(void) TEXT_HOT;

#endif
