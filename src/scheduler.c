#include <scheduler.h>
#include <math.h>
#include <syscall.h>
#include <debug.h>
#include <io.h>
#include <kernel.h>

#include <task_launcher.h>

#define TASK_QLENGTH TASK_MAX

struct task_q {
    uint8 size;
    uint8 head;
    uint8 tail;
    uint8 reserved;
};

struct task_manager {
    uint32 state;
    struct task_q q[TASK_QLENGTH];
};

static struct task_manager manager;
static task* priority_queue[TASK_PRIORITY_LEVELS][TASK_QLENGTH];

void scheduler_init(void) {
    manager.state = 0;
    for (size i = 0; i < TASK_QLENGTH; i++) {
	struct task_q* q = &manager.q[i];
	q->size = 0;
	q->head = 0;
	q->tail = 0;
    }

    task_create(0, 0, task_launcher);
}

void scheduler_schedule(task* t) {
    // always turn the bit in the field on
    manager.state |= (1 << t->priority);

    // then, add the job to the correct queue
    struct task_q* curr = &manager.q[t->priority];
    curr->size++;
    priority_queue[t->priority][curr->head] = t;
    curr->head = (curr->head + 1) & (TASK_QLENGTH - 1);
}

// scheduler_consume
void scheduler_get_next(void) {

    // find the msb and add it
    uint qid = short_lg(manager.state) - 1;
    if (qid > TASK_PRIORITY_LEVELS)
	_shutdown();

    struct task_q* curr = &manager.q[qid];
    //debug_log("choosing %u with %u", qid, curr->size);
    assert(curr->size, "trying to dequeue from empty queue %u", qid);

    // turn off the bit if the queue is now empty
    if (!(--curr->size))
	manager.state ^= (1 << qid);

    task_active = priority_queue[qid][curr->tail];
    vt_flush();
    curr->tail = (curr->tail + 1) & (TASK_QLENGTH - 1);
}

// Change Places! https://www.youtube.com/watch?v=msvOUUgv6m8
int scheduler_activate(void) {
    // should be able to context switch into task
    // debug_log("%d: %p %p", task_active->tid, task_active->sp, task_active->sp[1]);
    // vt_flush();
    return kernel_exit((void*)task_active->sp);
    // will return here from syscall_handle
}
