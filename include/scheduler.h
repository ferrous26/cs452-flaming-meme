#include <std.h>

#define TASK_Q_LENGTH 16
typedef uint8 task_id;
typedef uint8 task_pri;

typedef struct task_state {
    task_id  p_tid;
    task_pri priority;
    uint16   reserved;
    uint32*  sp;
} task;

typedef struct {
    uint8 size;
    uint8 head;
    uint8 tail;
    uint8 reserved;
} task_q;

typedef union {
    task_q q;
    uint32 info;
} task_q_union;

typedef struct task_manager {
    uint8* start;
    uint32 next;
    task_q q[TASK_Q_LENGTH];
} task_man;

typedef enum {
    ZOMBIE = 0,
    BLOCKED,
    READY,
    ACTIVE
} task_state;

void    scheduler_init(void);
task*   scheduler_schedule(void);
void    scheduler_activate(task* const tsk);
