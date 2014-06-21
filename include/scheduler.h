
#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <std.h>
#include <kernel.h>
#include <syscall.h>

#define TASK_HEAP_TOP 0x1F00000 // 31 MB
#define TASK_HEAP_BOT 0x0300000 //  3 MB
#define TASK_HEAP_SIZ 0x40000   // 64 pages * 4096 bytes per page

typedef int32  task_id;
typedef int32  task_pri;
typedef uint8  task_idx;

struct task_descriptor;
typedef struct task_descriptor task;

// this acts like a mask, just like the scheduler queue
struct task_descriptor {
    task_id  tid;
    task_id  p_tid;
    task_pri priority;
    int*     sp;
    task*    next;
};

struct task_q_pointers;
typedef struct task_q_pointers task_q;

struct task_q_pointers {
    task* head;
    task* tail;
};

// choose small values so they can be instruction immediates
#define RECV_BLOCKED (task*)0xA
#define RPLY_BLOCKED (task*)0xB

extern task_q recv_q[TASK_MAX];
extern task   tasks[TASK_MAX];

extern task*  task_active;
extern task*  int_queue[EVENT_COUNT];

static inline uint __attribute__ ((const)) task_index_from_tid(const task_id tid) {
    return mod2((uint32)tid, TASK_MAX);
}

void scheduler_init(void);
void scheduler_get_next(void) TEXT_HOT;
void scheduler_schedule(task* const t) TEXT_HOT;

/**
 * Change Places! https://www.youtube.com/watch?v=msvOUUgv6m8
 */
void scheduler_activate(void);

/**
 * @return tid of new task, or an error code as defined by CreateTask()
 */
int  task_create(const task_pri pri, void (*const start)(void)) TEXT_HOT;
void task_destroy(void) TEXT_HOT;

#endif
