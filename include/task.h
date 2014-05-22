#ifndef __TASK_H__
#define __TASK_H__

#include <std.h>
#include <math.h>

#define TASK_MAX 64

#define TASK_PRIORITY_LEVELS 32
#define TASK_PRIORITY_MAX    31
#define TASK_PRIORITY_MIN     0

typedef int32 task_id;
typedef uint8 task_idx;
typedef uint8 task_pri;
typedef uint8 task_state;

#define NO_DESCRIPTORS   -1
#define INVALID_PRIORITY -2

typedef struct task_descriptor {
    task_id  tid;
    task_idx p_index;
    task_pri priority;
    task_idx next;
    uint8    reserved;
    uint*    sp;
} task;

extern task_idx task_active;
extern task     tasks[TASK_MAX];

static inline uint __attribute__ ((pure)) task_index_from_pointer(task* t) {
    return mod2(t->tid, TASK_MAX);
}

static inline uint __attribute__ ((pure)) task_index_from_tid(const int32 tid) {
    return mod2(tid, TASK_MAX);
}


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
int task_create(const task_pri pri, void (*const start)(void));

/**
 * Mark the task descriptor as being available for reuse.
 */
void task_destroy();

#endif
