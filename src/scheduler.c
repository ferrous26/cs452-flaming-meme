#include <scheduler.h>
#include <circular_buffer.h>
#include <debug.h>
#include <tasks/task_launcher.h>



// force grouping by putting them into a struct
static struct task_free_list {
    char_buffer list;
    int8 buffer[TASK_MAX];
} free_list DATA_HOT;

static struct task_manager {
    uint32 state; // bitmap for accelerating queue selection
    task_q q[TASK_PRIORITY_LEVELS];
} manager DATA_HOT;

task_q recv_q[TASK_MAX] DATA_HOT;
struct task_descriptor  tasks[TASK_MAX] DATA_HOT;
struct task_pointers    task_ptrs DATA_HOT;
static uint8 table[256] DATA_HOT;

// This algorithm is borrowed from
// http://graphics.stanford.edu/%7Eseander/bithacks.html#IntegerLogLookup
static inline uint32 __attribute__ ((pure)) choose_priority(const uint32 v) {
    uint32 result;
    uint  t;
    uint  tt;

    if ((tt = v >> 16))
	result = (t = tt >> 8) ? 24 + table[t] : 16 + table[tt];
    else
	result = (t = v >> 8) ? 8 + table[t] : table[v];

    return result;
}

static inline int* __attribute__ ((const)) task_stack(const task_idx idx) {
    return (int*)(TASK_HEAP_TOP - (TASK_HEAP_SIZ * idx));
}

void scheduler_init(void) {
    int i;

    // initialize the lookup table for lg()
    table[0] = table[1] = 0;
    for (i = 2; i < 256; i++)
	table[i] = 1 + table[i >> 1];
    table[0] = 32; // if you want log(0) to return -1, change to -1

    memset(&manager, 0, sizeof(manager));
    memset(&recv_q,  0, sizeof(recv_q));
    memset(&tasks,   0, sizeof(tasks));

    cbuf_init(&free_list.list, TASK_MAX, free_list.buffer);

    for (i = 0; i < 16; i++) {
	tasks[i].tid = i;
	cbuf_produce(&free_list.list, (char)i);
    }

    // get the party started
    task_active = &tasks[0];
    task_create(TASK_PRIORITY_IDLE + 1, task_launcher);

    for (; i < TASK_MAX; i++) {
	tasks[i].tid = i;
	cbuf_produce(&free_list.list, (char)i);
    }

    memset(&task_ptrs, 0, sizeof(task_ptrs));
}

void scheduler_schedule(task* const t) {
    task_q* const q = &manager.q[t->priority];

    assert(t >= tasks, "schedule: cant schedule task at %p", t);
    assert(t < &tasks[TASK_MAX], "schedule: cant schedule task at %p", t);
    assert(t->priority <= TASK_PRIORITY_MAX, "schedule: Bad Priority %u", t->priority);

    // _always_ turn on the bit in the manager state
    manager.state |= (1 << t->priority);

    // if there was something in the queue, append to it
    if (q->head) {
	q->tail->next = t;
    }
    // else the queue was off, so set the head
    else
	q->head = t;

    // then we set the tail pointer
    q->tail = t;

    // mark the end of the queue
    t->next = NULL;
}

// scheduler_consume
void scheduler_get_next(void) {

    // find the msb and add it
    uint32 priority = choose_priority(manager.state);
    assert(priority != 32, "Ran out of tasks to run");

    task_q* const q = &manager.q[priority];

    task_active = q->head;
    q->head = q->head->next;

    // if we hit the end of the list, then turn off the queue
    if (!q->head)
	manager.state ^= (1 << priority);
}

void scheduler_activate(void) {
    kernel_exit(task_active->sp);
}

int task_create(const task_pri pri, void (*const start)(void)) {
    // double check that priority was checked in user land
    assert(pri <= TASK_PRIORITY_MAX, "Invalid priority %u", pri);

    // make sure we have something to allocate
    if (!cbuf_can_consume(&free_list.list)) return NO_DESCRIPTORS;

    // actually take the task descriptor and load it up
    const task_idx task_index = cbuf_consume(&free_list.list);

    task* tsk      = &tasks[task_index];
    tsk->p_tid     = task_active->tid; // task_active is _always_ the parent
    tsk->priority  = pri;

    // setup the default trap frame for the new task
    tsk->sp        = task_stack(task_index) - TRAP_FRAME_SIZE;
    tsk->sp[2]     = START_ADDRESS(start);
    tsk->sp[3]     = DEFAULT_SPSR;
    tsk->sp[12]    = EXIT_ADDRESS; // set link register to auto-call Exit()

    // set tsk->next
    scheduler_schedule(tsk);

    return tsk->tid;
}

void task_destroy() {
    // TODO: handle overflow (trololol)
    task_active->tid += TASK_MAX;
    task_active->sp   = NULL;

    // empty the receive buffer
    task_q* q = &recv_q[task_index_from_tid(task_active->tid)];
    while (q->head) {
	q->head->sp[0] = INCOMPLETE;
	scheduler_schedule(q->head);
	q->head = q->head->next;
    }

    // put the task back into the allocation pool
    cbuf_produce(&free_list.list, (char)mod2((uint)task_active->tid, TASK_MAX));
}

#ifdef DEBUG
void debug_task(const task_id tid) {
    task* tsk = &tasks[task_index_from_tid(tid)];

    debug_log("Task:");
    debug_log("             ID: %u", tsk->tid);
    debug_log("      Parent ID: %u", tsk->p_tid);
    debug_log("       Priority: %u", tsk->priority);

    if (tsk->next == RECV_BLOCKED)
	debug_log("           Next: RECEIVE BLOCKED");
    else if (tsk->next == RPLY_BLOCKED)
	debug_log("           Next: REPLY BLOCKED");
    else
	debug_log("           Next: %p", tsk->next);

    debug_log("  Stack Pointer: %p", tsk->sp);
}
#endif
