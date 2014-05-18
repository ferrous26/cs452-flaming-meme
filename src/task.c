#include <task.h>

#define TASK_MAX 32 // 32 for now, because of implementation reasons

static inline uint msb32(uint x) {

    // TODO: test that this initializes correctly
    //       if not, then we need to figure out where in the orex
    //       we need to specify this kind of thing, or just put it
    //       in task_init...
    static const uint bit_vals[16] = {
	0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
    };

    uint r = 0;
    if (x & 0xFFFF0000) { r += 16; x >>= 16; }
    if (x & 0x0000FF00) { r +=  8; x >>=  8; }
    if (x & 0x000000F0) { r +=  4; x >>=  4; }
    return r + bit_vals[x];
}

// this acts like a mask, similar to the scheduler queue
// except in reverse (because this one starts off full)
static uint task_alloc = 0xffffffff;

static task tasks[TASK_MAX];

void task_init(void) {
    size i = 0;
    for (; i < TASK_MAX; i++)
	tasks[i].tid = 1; // why 1? because then we can reserve 0!
}

task_err task_create(task** t,
		     const task_id p_tid,
		     const task_pri pri,
		     const void* const sp) {

    if (pri > TASK_PRIORITY_MAX)
	return INVALID_PRIORITY;

    // first, find a task id
    const task_id tid = msb32(task_alloc);

    // but bail if we have no descriptors available
    if (!tid) return NO_DESCRIPTORS;

    // then, xor it out of the mask
    task_alloc ^= (1 << tid);

    task* tsk       = tasks + tid;
    tsk->tid       += tid; // TODO: handle tid overflow (trololo)
    tsk->p_tid      = p_tid;
    tsk->state      = TASK_STATE_READY;
    tsk->priority   = pri;
    tsk->sp         = (void*)sp;

    *t = tsk;
    return OK;
}

void task_destroy(task* const t) {
    // put the task back into the allocation pool
    task_alloc |= (1 << t->tid);
}
