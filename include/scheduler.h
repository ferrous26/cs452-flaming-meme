#include <std.h>
#include <task.h>

void scheduler_init(void);
void scheduler_schedule(task* t);
int  scheduler_get_next(void);
int  scheduler_activate(void);
