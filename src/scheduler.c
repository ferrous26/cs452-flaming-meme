#include <scheduler.h>
#include <syscall.h>
#include <bootstrap.h> // TODO: does this belong here?


#define TASK_QLENGTH 32

// TODO: we need to rethink these scheduler data structures
//       based on what we need to do; they are too messy at
//       the moment. A 2D array for priority_queue might actually
//       be a good idea

typedef struct {
    uint8 size;
    uint8 head;
    uint8 tail;
    uint8 reserved;
} task_q;

typedef struct task_manager {
    uint32 state;
    task*  active_task;
    task_q q[TASK_QLENGTH];
} task_man;

static inline uint msb16(uint x) {

    // TODO: test that this initializes correctly (like in task.c)
    static const uint bit_vals[16] = {
	0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
    };

    uint r = 0;
    // TODO: re-enable this if we increase the number of priority levels
    //    if (x & 0xFFFF0000) { r += 16; x >>= 16; }
    if (x & 0x0000FF00) { r +=  8; x >>=  8; }
    if (x & 0x000000F0) { r +=  4; x >>=  4; }
    return r + bit_vals[x];
}

static task_man manager;
static task* priority_queue[TASK_QLENGTH * TASK_PRIORITY_LEVELS];

static void scheduler_produce(task* const t) {
    // always turn the bit in the field on
    manager.state |= (1 << t->priority);

    // then, add the job to the correct queue
    priority_queue[(t->priority * TASK_QLENGTH) + manager.q[t->priority].head] =
	t;

    manager.q[t->priority].head++;
    if (manager.q[t->priority].head >= TASK_QLENGTH)
	manager.q[t->priority].head = 0;
}

static task* scheduler_consume(void) {
    // find the msb and add it
    uint  qid = msb16(manager.state);
    task_q* q = manager.q + qid;

    // TODO: optimize this scenario by putting
    //       the active task back into its queue
    //       before fetching the next task
    // turn off the bit if the queue is now empty
    if (!(q->size--))
	manager.state ^= (1 << qid);

    manager.active_task =
	priority_queue[(qid*TASK_QLENGTH) + manager.q[qid].tail];

    manager.q[qid].tail++;
    if (manager.q[qid].tail >= TASK_QLENGTH)
	manager.q[qid].tail = 0;

    // TODO: seems redundant to have to return this as it is in a known location
    return manager.active_task;
}

// TODO: REWRITE THIS MOTHER TRUCKER
static void _task_create(task* loc, task_pri priority, void (*code)(void)) {

    task_create(&loc, manager.active_task->tid, priority, NULL);

    // TODO: should task_create be in charge of setting up the stack?
    loc->sp = (void*)0x00400000;
    UNUSED(code);
    //    loc->sp[16] = (void*)code; // TODO: why the fuck are you writing pass end of array?
}

void scheduler_init(void) {
    manager.state       = 0;
    manager.active_task = NULL; // TODO: if bootstrap changes, this might need to change
    for (size i = 0; i < TASK_QLENGTH; i++) {
	task_q* q = manager.q + i;
	q->size = 0;
	q->head = 0;
	q->tail = 0;
    }

    // TODO: WAT
    _task_create(NULL, 0, bootstrap);
}

task* scheduler_schedule() {
    // place active task back into queue (unless blocked or exiting)
    // get next task to execute
    scheduler_produce(NULL);
    return scheduler_consume();
}

void scheduler_activate(task* const tsk) {
    UNUSED(tsk);
    // Change Places! https://www.youtube.com/watch?v=msvOUUgv6m8
}
