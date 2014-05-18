#include <std.h>

#define TASK_KERNEL_ID 0

#define TASK_PRIORITY_LEVELS 16
#define TASK_PRIORITY_MAX    15
#define TASK_PRIORITY_MIN     0

#define TASK_STATE_ZOMBIE  0
#define TASK_STATE_BLOCKED 1
#define TASK_STATE_READY   2
#define TASK_STATE_ACTIVE  3

typedef int32 task_id;
typedef uint8 task_pri;
typedef uint8 task_state;

typedef enum {
    OK               =  0,
    NO_DESCRIPTORS   = -1,
    INVALID_PRIORITY = -2
} task_err;

// TODO: do we really want to make this public?
typedef struct task_state {
    task_id    tid;
    task_id    p_tid;
    task_state state;
    task_pri   priority;
    uint16     reserved;
    void*      sp;
} task;


/**
 * Initialize global state related to tasks
 */
void task_init(void);

/**
 * @todo Perhaps the sp should be set by the caller?
 */
task_err task_create(task** t,
		     const task_id p_tid,
		     const task_pri pri,
		     const void* const sp);

/**
 * Mark the task descriptor as being available for reuse.
 */
void task_destroy(task* const t);
