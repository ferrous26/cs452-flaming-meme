#include <kernel.h>
#include <clz.h>

static struct task_manager {
    uint32 state; // bitmap for accelerating queue selection
    task_q q[TASK_PRIORITY_LEVELS];
} manager DATA_HOT;

uint8 table[256] DATA_HOT;

static inline void
scheduler_schedule(task* const t)
{
    task_q* const q = &manager.q[t->priority];

    assert(t >= tasks && t < &tasks[TASK_MAX],
           "scheduler: cannot schedule task at %p (%p-%p)",
           t, tasks, &tasks[TASK_MAX]);

    // _always_ turn on the bit in the manager state
    manager.state |= (1 << t->priority);

    // if there was something in the queue, append to it
    if (q->head)
      q->tail->next = t;
    // else the queue was off, so set the head
    else
      q->head = t;

    // then we set the tail pointer
    q->tail = t;

    // mark the end of the queue
    t->next = NULL;
}

// scheduler_consume
static inline void
scheduler_get_next()
{
    // find the msb and add it
    const task_priority priority = choose_priority(manager.state);
    assert(priority != 32, "Ran out of tasks to run");

    task_q* const q = &manager.q[priority];

    task_active = q->head;
    q->head = q->head->next;

    // if we hit the end of the list, then turn off the queue
    if (!q->head)
      manager.state ^= (1 << priority);
}

void scheduler_first_run()
{
    scheduler_get_next();
    kernel_exit();
}

void scheduler_reschedule(task* const t)
{
    scheduler_schedule(t);
}
