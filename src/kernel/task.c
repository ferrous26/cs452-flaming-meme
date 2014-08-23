#include <kernel.h>

#define TASK_HEAP_TOP  0x1F00000 // 31 MB
#define TASK_HEAP_BOT  0x0300000 //  3 MB
#define TASK_HEAP_SIZE 0x8000    // 32 kB

#define SAVED_REGISTERS 13 // trap frame size (for software interrupt)
#define TRAP_FRAME_SIZE (SAVED_REGISTERS * WORD_SIZE)

#define DEFAULT_SPSR    0x50  // no fiq


task*  task_active             DATA_HOT;
static task   tasks[TASK_MAX]  DATA_WARM;
static task_q free_list        DATA_HOT;

// TODO: need to abstract out the stack work
// TODO: need to abstract the use of task->next instead of all the copy pasta

static inline void scheduler_schedule(task* const t);


static inline uint __attribute__ ((const))
task_index_from_tid(const task_id tid)
{
    return mod2((uint)tid, TASK_MAX);
}

/* Top of stack */
static inline int* __attribute__ ((const))
task_stack(const task* const t)
{
    const uint idx = task_index_from_tid(t->tid);
    return (int*)(TASK_HEAP_TOP - (TASK_HEAP_SIZE * idx));
}

static inline void
task_free_list_produce(task* const t)
{
    // if there was something in the queue, append to it
    if (free_list.head)
        free_list.tail->next = t;
    // else the queue was off, so set the head
    else
        free_list.head = t;

    // then we set the tail pointer
    free_list.tail = t;

    // mark the end of the queue
    t->next = NULL;
}

static inline task*
task_free_list_consume()
{
    assert(free_list.head,
           "free_list: trying to consume from empty free list");

    task* const t = free_list.head;
    free_list.head = t->next;

    return t;
}

static inline task_id
task_create(const task_priority priority, void (*const start)(void))
{
    // double check that priority was checked in user land
    assert(priority <= TASK_PRIORITY_MAX, "Invalid priority %u", priority);

    // make sure we have something to allocate
    if (free_list.head == NULL) return ERR_NO_DESCRIPTORS;

    task* const tsk = task_free_list_consume();
    tsk->p_tid      = task_active->tid; // task_active is _always_ the parent
    tsk->priority   = priority;

    // setup the default trap frame for the new task
    tsk->sp         = task_stack(tsk) - TRAP_FRAME_SIZE;
    tsk->sp[2]      = (int)start;
    tsk->sp[3]      = DEFAULT_SPSR;
    tsk->sp[12]     = (int)Exit; // set link register to auto-call Exit()

    // set tsk->next
    scheduler_schedule(tsk);

    return tsk->tid;
}

static inline void
task_destroy()
{
    // TODO: handle overflow (trololol)
    task_active->tid += TASK_MAX;
    task_active->sp   = NULL;

    // empty the receive buffer
    task_q* q = &task_active->recv_q;
    while (q->head) {
        q->head->sp[0] = ERR_INCOMPLETE;
        scheduler_reschedule(q->head);
        q->head = q->head->next;
    }

    // put the task back into the allocation pool
    task_free_list_produce(task_active);
}
