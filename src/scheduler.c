#include <syscall.h>
#include <bootstrap.h>
#include <scheduler.h>
#include <circular_buffer.h>

#define TASK_MAX  32
#define IDLE_TASK 0

task* active_task = 0;
static task tasks[TASK_MAX];

static uint8 highest_priority_buffer[TASK_MAX];
static uint8 medium_priority_buffer[TASK_MAX];
static uint8 low_priority_buffer[TASK_MAX];
static uint8 lowest_priority_buffer[TASK_MAX];

static short_buffer high_priority;
static short_buffer medium_priority;
static short_buffer low_priority;
static short_buffer lowest_priority;


void _task_create( task* loc, task_pri priority, void (*code) () ) {
    // this is why i would prefer to store the task id inside of the task
    loc->p_tid = (tasks - active_task) >> 2;
    loc->priority = priority;
    
    // set up the stack space
    loc->sp = (uint32*)0x00400000;
    loc->sp[16] = (uint32) code;
}

void scheduler_init(void) {

    sbuf_init(&high_priority,   32, highest_priority_buffer);
    sbuf_init(&medium_priority, 32, medium_priority_buffer);
    sbuf_init(&low_priority,    32, low_priority_buffer);
    sbuf_init(&lowest_priority, 32, lowest_priority_buffer);

    // create the idle task
    _task_create( tasks, 4, bootstrap );
    tasks->p_tid = -1; //no parent -1 is kernel
}

task* scheduler_next_task(void) {
    // search through queues to find a task
    // assembly optimize (we can do it all in a FIQ)

    // assembly optimized cb_can_consume
    // conditional jump to cb_consume
    // repeat for each level

    // consume will consume value and return the id

    return (task*)tasks;
}

task* scheduler_schedule() {
    return (task*)tasks;
}

void scheduler_activate(task* const tsk) {
    UNUSED(tsk);
    // active_task = tsk;
    // should be able to context switch into task
}


void scheduler_handle(const task_id tid) {
    UNUSED(tid);
    // context switch into the task
}
