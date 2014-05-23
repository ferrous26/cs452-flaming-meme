
#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <std.h>
#include <math.h>

#define TASK_PRIORITY_LEVELS 32
#define TASK_PRIORITY_MAX    31
#define TASK_PRIORITY_MIN     0

typedef int32  task_id;
typedef uint32 task_pri;
typedef uint8  task_idx;

// this acts like a mask, just like the scheduler queue
typedef struct task_descriptor {
    task_id                 tid;
    task_id                 p_tid;
    task_pri                priority;
    struct task_descriptor* next;
    uint*                   sp;
} task;

extern task* task_active;


void scheduler_init(void);

void scheduler_schedule(task* t);

int  scheduler_get_next(void);

/**
 * Change Places! https://www.youtube.com/watch?v=msvOUUgv6m8
 */
void scheduler_activate(void);

/**
 * @return tid of new task, or an error code as defined by CreateTask()
 */
int task_create(const task_pri pri, void (*const start)(void));

void task_destroy();

/**
 * Print debug info about the given task to the debug console.
 */
void debug_task(const task_id tid);

#endif
