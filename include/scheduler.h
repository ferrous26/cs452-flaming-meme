#include <std.h>
#include <task.h>

void  scheduler_init(void);
task* scheduler_schedule(void);
void  scheduler_activate(task* const tsk);
