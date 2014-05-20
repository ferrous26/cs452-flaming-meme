#include <task.h>
#include <syscall.h>
#include <debug.h>
#include <math.h>
#include <scheduler.h>

#define TASK_HEAP_BASE 0x1000
#define TASK_STACK_SIZE 786432 // 192 pages * 4096 bytes per page

// this acts like a mask, just like the scheduler queue
static uint tasks_bitmap = 0;
static task tasks[TASK_MAX];
task* task_active;

void task_init(void) {
    task_active  = NULL;

    tasks_bitmap = 0;

    for (size i = 0; i < TASK_MAX; i++)
	tasks[i].tid = -TASK_MAX + i;

    // TODO: setup the stack canaries
}

void debug_task(const task_id tid) {

    task* tsk = &tasks[tid & (TASK_MAX - 1)];

    debug_log("Task:");
    debug_log("             ID: %u", tsk->tid);
    debug_log("      Parent ID: %u", tsk->p_tid);
    debug_log("       Priority: %u", tsk->priority);
    debug_log("  Stack Pointer: %p", tsk->sp);
}

static inline uint* task_stack(const uint8 task_index) __attribute__ ((pure));
static inline uint* task_stack(const uint8 task_index) {
    return (uint*)(TASK_HEAP_BASE + (TASK_STACK_SIZE * task_index));
}

task_id task_create(const task_id p_tid,
		    const task_pri pri,
		    void (*const start)(void)) {

    assert(pri <= TASK_PRIORITY_MAX, "Invalid priority");

    // first, find a task id
    const uint task_index = lg(tasks_bitmap);

    // but bail if we have no descriptors available
    if (task_index >= TASK_MAX) return NO_DESCRIPTORS;

    // else, take the tid (mark the bit on the bitmap)
    tasks_bitmap |= (1 << task_index);

    // actually take the task descriptor and load it up
    task* tsk     = &tasks[task_index];
    tsk->tid     += TASK_MAX; // TODO: handle overflow (trololol)
    tsk->p_tid    = p_tid;
    tsk->priority = pri;

    // setup the default trap frame for the new task
    tsk->sp       = task_stack(task_index) - TRAP_FRAME_SIZE;
    tsk->sp[1]    = START_ADDRESS(start);
    tsk->sp[2]    = DEFAULT_SPSR;
    tsk->sp[12]   = EXIT_ADDRESS;

    scheduler_schedule(tsk);

    return tsk->tid;
}

void task_destroy(const task* const t) {
    // put the task back into the allocation pool
    tasks_bitmap ^= (1 << (t->tid & (TASK_MAX - 1)));
}
