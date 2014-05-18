#include <syscall.h>
#include <bootstrap.h>

#include <io.h>
#include <debug.h>

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

    debug_log( "Bootstrap: %p", code );
    
    // set up the stack space
    loc->sp = (uint32*) 0x4000;
    loc->sp[2] = (uint32) code + 0x217000;     /* will get loaded into the pc on exit */
    loc->sp[3] = 0x10;              /* want task to start in user mode */
}

void scheduler_init(void) {

    sbuf_init(&high_priority,   32, highest_priority_buffer);
    sbuf_init(&medium_priority, 32, medium_priority_buffer);
    sbuf_init(&low_priority,    32, low_priority_buffer);
    sbuf_init(&lowest_priority, 32, lowest_priority_buffer);

    // create the idle task
    _task_create( tasks, 4, bootstrap );
    tasks->p_tid = -1; //-1 for kernel
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
    active_task = tsk;

    debug_log( "Activating!! %p %p %p", tsk->sp, tsk->sp[2], tsk->sp[3] );
    debug_cpsr();
    vt_flush();

    // should be able to  context switch into task
    int x = kernel_exit( tsk->sp );
    debug_log( "jumping to %p", x );
    debug_cpsr();
    vt_flush();
}


void scheduler_handle(const task_id tid) {
    UNUSED(tid);
    // context switch into the task
}
