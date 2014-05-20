#ifndef __TASK_H__
#define __TASK_H__

#include <std.h>

#define TASK_MAX 32 // 32 for now, because of implementation reasons

#define TASK_PRIORITY_LEVELS 16
#define TASK_PRIORITY_MAX    15
#define TASK_PRIORITY_MIN     0

typedef uint16 task_id;
typedef uint8 task_pri;
typedef uint8 task_state;

#define NO_DESCRIPTORS   -1
#define INVALID_PRIORITY -2

typedef struct task_descriptor {
    task_id  tid;
    task_id  p_tid;
    task_pri priority;
    uint8    reserved1;
    uint16   reserved2;
    uint*    sp;
} task;

extern task* task_active;

/**
 * Initialize global state related to tasks
 */
void task_init(void);

/**
 * Print debug info about the given task to the debug console.
 */
void debug_task(const task_id tid);

/**
 * @return tid of new task, or an error code as defined by CreateTask()
 */
task_id task_create(const task_id p_tid,
		    const task_pri pri,
		    void (*const start)(void));

/**
 * Mark the task descriptor as being available for reuse.
 */
void task_destroy(const task* const t);

#endif
