#include <scheduler.h>
#include <io.h>
#include <math.h>
#include <debug.h>
#include <syscall.h>
#include <tasks/task_launcher.h>

struct task_q {
    task_idx head;
    task_idx tail;
    uint16 reserved;
};

struct task_manager {
    uint32 state; // bitmap for accelerating queue selection
    struct task_q q[TASK_PRIORITY_LEVELS];
};

static struct task_manager manager;


void scheduler_init(void) {

    manager.state = 0;
    for (size i = 0; i < TASK_PRIORITY_LEVELS; i++) {
	struct task_q* q = &manager.q[i];
	q->head = TASK_MAX;
	q->tail = TASK_MAX;
    }

    // get the party started
    task_active = 0;
    task_create(TASK_PRIORITY_MIN, task_launcher);
}

void scheduler_schedule(const uint task_index) {

    task* const          t = &tasks[task_index];
    struct task_q* const q = &manager.q[t->priority];

    // _always_ turn on the bit in the manager state
    manager.state |= (1 << t->priority);

    // if the queue was off, set the head
    if (q->head == TASK_MAX)
	q->head = task_index;

    // else there was something in the queue, so append to it
    else
	tasks[q->tail].next = task_index;

    // then we set the tail pointer
    q->tail = task_index;

    // mark the end of the queue
    t->next = TASK_MAX;
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
    q->head = tasks[task_active].next;

    // if we hit the end of the list, then turn off the queue
    if (q->head == TASK_MAX)
	manager.state ^= (1 << priority);

    return 0;
}

// Change Places! https://www.youtube.com/watch?v=msvOUUgv6m8
int scheduler_activate(void) {
    return kernel_exit(tasks[task_active].sp);
}
