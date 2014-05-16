#include <scheduler.h>
#include <circular_buffer.h>

#define TASK_MAX  32
#define IDLE_TASK 0

task tasks[TASK_MAX];

uint8 highest_priority_buffer[TASK_MAX];
uint8 medium_priority_buffer[TASK_MAX];
uint8 low_priority_buffer[TASK_MAX];
uint8 lowest_priority_buffer[TASK_MAX];

short_buffer high_priority;
short_buffer medium_priority;
short_buffer low_priority;
short_buffer lowest_priority;


void scheduler_init(void) {

    sbuf_init(&high_priority,   32, highest_priority_buffer);
    sbuf_init(&medium_priority, 32, medium_priority_buffer);
    sbuf_init(&low_priority,    32, low_priority_buffer);
    sbuf_init(&lowest_priority, 32, lowest_priority_buffer);

    // create the idle task
}

task_id scheduler_next_task(void) {
    // search through queues to find a task
    // assembly optimize (we can do it all in a FIQ)

    // assembly optimized cb_can_consume
    // conditional jump to cb_consume
    // repeat for each level

    // consume will consume value and return the id

    return IDLE_TASK;
}

void scheduler_handle(const task_id tid) {
    UNUSED(tid);
    // context switch into the task
}
