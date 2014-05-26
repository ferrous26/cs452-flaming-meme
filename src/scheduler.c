#include <scheduler.h>
#include <math.h>
#include <circular_buffer.h>
#include <syscall.h>
#include <debug.h>
#include <tasks/task_launcher.h>

#define TASK_HEAP_TOP 0x1F00000 // 31 MB
#define TASK_HEAP_BOT 0x0300000 //  3 MB
#define TASK_HEAP_SIZ 0x40000   // 64 pages * 4096 bytes per page

#define TASK_MAX 64

// force grouping by putting them into a struct
struct task_free_list {
    uint_buffer list;
    uint32 buffer[TASK_MAX*2];
};

struct task_q {
    task* head;
    task* tail;
};

struct task_manager {
    uint32 state; // bitmap for accelerating queue selection
    struct task_q q[TASK_PRIORITY_LEVELS];
};

static struct task_descriptor  tasks[TASK_MAX];
       struct task_descriptor* task_active;
static struct task_free_list   free_list;

static struct task_manager manager;


static inline uint __attribute__ ((const)) task_index_from_pointer(task* t) {
    return mod2(t->tid, TASK_MAX);
}

static inline uint __attribute__ ((const)) task_index_from_tid(const int32 tid) {
    return mod2(tid, TASK_MAX);
}

static inline uint* __attribute__ ((const)) task_stack(const task_idx idx) {
    return (uint*)(TASK_HEAP_TOP - (TASK_HEAP_SIZ * idx));
}

void scheduler_init(void) {

    ibuf_init(&free_list.list, TASK_MAX*2, free_list.buffer);

    for (size i = 0; i < TASK_MAX; i++) {
	tasks[i].tid = i;
	ibuf_produce(&free_list.list, i);
    }

    manager.state = 0;
    for (size i = 0; i < TASK_PRIORITY_LEVELS; i++) {
	struct task_q* q = &manager.q[i];
	q->head = NULL;
	q->tail = NULL;
    }

    // get the party started
    task_active = &tasks[0];
    task_create(TASK_PRIORITY_MIN, task_launcher);
}

void scheduler_schedule(task* t) {

    struct task_q* const q = &manager.q[t->priority];

    // _always_ turn on the bit in the manager state
    manager.state |= (1 << t->priority);

    // if the queue was off, set the head
    if (!q->head)
	q->head = t;

    // else there was something in the queue, so append to it
    else
	q->tail->next = t;

    // then we set the tail pointer
    q->tail = t;

    // mark the end of the queue
    t->next = NULL;
}

// scheduler_consume
int scheduler_get_next(void) {

    // find the msb and add it
    uint priority = lg(manager.state);
    if (!priority)
	return -1;
    priority--;

    struct task_q* const q = &manager.q[priority];

    task_active = q->head;
    q->head = task_active->next;

    // if we hit the end of the list, then turn off the queue
    if (!q->head)
	manager.state ^= (1 << priority);

    return 0;
}

void scheduler_activate(void) {
    kernel_exit(task_active->sp);
}

int task_create(const task_pri pri, void (*const start)(void)) {

    // double check that priority was checked in user land
    assert(pri <= TASK_PRIORITY_MAX, "Invalid priority %u", pri);

    // make sure we have something to allocate
    if (!ibuf_can_consume(&free_list.list)) return NO_DESCRIPTORS;

    // actually take the task descriptor and load it up
    const task_idx task_index = ibuf_consume(&free_list.list);

    task* tsk     = &tasks[task_index];
    tsk->p_tid    = task_active->tid; // task_active is _always_ the parent
    tsk->priority = pri;

    // setup the default trap frame for the new task
    tsk->sp       = task_stack(task_index) - TRAP_FRAME_SIZE;
    tsk->sp[1]    = START_ADDRESS(start);
    tsk->sp[2]    = DEFAULT_SPSR;
    tsk->sp[12]   = EXIT_ADDRESS; // set link register to auto-call Exit()

    // set tsk->next
    scheduler_schedule(tsk);

    return tsk->tid;
}

void task_destroy() {
    // TODO: handle overflow (trololol)
    task_active->tid += TASK_MAX;
    // put the task back into the allocation pool
    ibuf_produce(&free_list.list, task_active->tid);
}


#if DEBUG
void debug_task(const task_id tid) {

    task* tsk = &tasks[task_index_from_tid(tid)];

    debug_log("Task:");
    debug_log("             ID: %u", tsk->tid);
    debug_log("      Parent ID: %u", tsk->p_tid);
    debug_log("       Priority: %u", tsk->priority);
    debug_log("           Next: %u", tsk->next);
    debug_log("  Stack Pointer: %p", tsk->sp);
}
#endif
