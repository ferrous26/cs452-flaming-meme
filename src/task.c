#include <task.h>
#include <syscall.h>
#include <debug.h>
#include <math.h>
#include <scheduler.h>
#include <circular_buffer.h>

#define TASK_HEAP_TOP 0x1F00000 // 31 MB
#define TASK_HEAP_BOT 0x0300000 //  3 MB
#define TASK_HEAP_SIZ 262144    // 64 pages * 4096 bytes per page

// this acts like a mask, just like the scheduler queue
task     tasks[TASK_MAX];
task_idx task_active;

// force grouping by putting them into a struct
struct task_free_list {
    uint_buffer list;
    uint32 buffer[TASK_MAX*2];
};
static struct task_free_list free_list;


void task_init(void) {

    ibuf_init(&free_list.list, TASK_MAX*2, free_list.buffer);

    for (size i = 0; i < TASK_MAX; i++) {
	tasks[i].tid = -TASK_MAX + i;
	ibuf_produce(&free_list.list, i);
    }

    // TODO: setup the stack canaries
}

void debug_task(const task_id tid) {

    task* tsk = &tasks[task_index_from_tid(tid)];

    debug_log("Task:");
    debug_log("             ID: %u", tsk->tid);
    debug_log("      Parent ID: %u", tasks[tsk->p_index].tid);
    debug_log("       Priority: %u", tsk->priority);
    debug_log("           Next: %u", tsk->next);
    debug_log("  Stack Pointer: %p", tsk->sp);
}

static inline uint* __attribute__ ((pure)) task_stack(const task_idx idx) {
    return (uint*)(TASK_HEAP_TOP - (TASK_HEAP_SIZ * idx));
}

int task_create(const task_pri pri, void (*const start)(void)) {

    // double check that priority was checked in user land
    assert(pri <= TASK_PRIORITY_MAX, "Invalid priority %u", pri);

    // make sure we have something to allocate
    if (!ibuf_can_consume(&free_list.list)) return NO_DESCRIPTORS;

    // actually take the task descriptor and load it up
    const task_idx task_index = ibuf_consume(&free_list.list);

    task* tsk     = &tasks[task_index];
    tsk->tid     += TASK_MAX; // TODO: handle overflow (trololol)
    tsk->p_index  = task_active; // task_active is _always_ the parent
    tsk->priority = pri;

    // setup the default trap frame for the new task
    tsk->sp       = task_stack(task_index) - TRAP_FRAME_SIZE;
    tsk->sp[1]    = START_ADDRESS(start);
    tsk->sp[2]    = DEFAULT_SPSR;
    tsk->sp[12]   = EXIT_ADDRESS; // set link register to auto-call Exit()

    // set tsk->next
    scheduler_schedule(task_index);

    return tsk->tid;
}

void task_destroy() {
    // put the task back into the allocation pool
    ibuf_produce(&free_list.list, task_active);
}
