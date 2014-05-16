#include <std.h>

typedef uint8 task_id;

struct task_state;
typedef struct task_state task;

typedef enum {
    ZOMBIE = 0,
    BLOCKED,
    READY,
    ACTIVE
} task_state;

void    scheduler_init(void);
task_id scheduler_next_task(void);
void    scheduler_handle(const task_id tid);
