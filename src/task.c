#include <task.h>
#include <io.h>

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
static task tasks[TASK_MAX];
static uint task_alloc = 0xffffffff;

void task_init(void) {
    size i = 0;
    for (; i < TASK_MAX; i++)
	tasks[i].tid = 1; // why 1? because then we can reserve 0!
}

// a couple things
// - we dont know the avaliable task pointer coming in without checking the queue
// - it should be this functions job to give us the space to do that
// - for testing its easier if we know the location but it shouldnt matter because
//      scheduler get next will get us the task that we want to run next anyways
//
//
// task_create ( p_tid, pri, sp, starting point) should be all this needs to work
task_err task_create(task* tsk,
		     const task_id p_tid,
		     const task_pri pri,
		     void** sp,
		     void (*start) ()) {
    if (pri > TASK_PRIORITY_MAX)
	return INVALID_PRIORITY;

    // first, find a task id
    const task_id tid = msb32(task_alloc);
    // but bail if we have no descriptors available
    if (!tid) return NO_DESCRIPTORS;

    // then, xor it out of the mask
    task_alloc ^= (1 << tid);
    
    /* couldn't use 
     * tasks are static to this file
     * scheduler is in another file
     * i need the pointer to the task so i can activate it
    task* tsk       = tasks + tid;
    */
    tsk->tid        = tid; // TODO: handle tid overflow (trololo)
    tsk->p_tid      = p_tid;
    tsk->state      = TASK_STATE_READY;
    tsk->priority   = pri;

    // TODO: sp should probably not be a thing passed in but grabbed by this function
    // type needs to point to something that is a word large to get the compiler to
    // insert contents properly
    tsk->sp         = sp - 13 * 4; // # of saved registers * word size
    // TASK CREATE NEEDS TO SET UP THE STACK PROPERLY SO WE CAN CONTEXT SWITCH INTO THE
    // TASK TRANSPARENTLY THIS IS LITERALLY THE MOST IMPORTANT PART OF task_create
    tsk->sp[2]      = (void*) start + 0x217000;
    tsk->sp[3]      = (void*) 0x10;

    // *t = tsk; why this adds more memory copying overhead,
    // somone gives you space then you use it don't add more unnessicary redirection
    return OK;
}

void task_destroy(task* const t) {
    // put the task back into the allocation pool
    task_alloc |= (1 << t->tid);
}
