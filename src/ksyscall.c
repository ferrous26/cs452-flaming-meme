#include <io.h>
#include <task.h>
#include <debug.h>
#include <scheduler.h>
#include <syscall.h>

inline static void ksyscall_pass() {
    scheduler_schedule(task_active);
}

inline static void ksyscall_create(const kreq_create* req, uint* const result) {
    *result = task_create(req->priority, req->code);
    scheduler_schedule(task_active);
}

inline static void ksyscall_tid(uint* const result) {
    *result = (uint)tasks[task_active].tid;
    scheduler_schedule(task_active);
}

inline static void ksyscall_ptid(uint* const result) {
    *result = (uint)tasks[task_active].p_tid;
    scheduler_schedule(task_active);
}

inline static void ksyscall_exit() {
    task_destroy(task_active);
}

inline static void ksyscall_priority(uint* const result) {
    *result = (uint)tasks[task_active].priority;
    scheduler_schedule(task_active);
}

void syscall_handle(const uint code,
		    const kreq_create* const req,
		    uint* const sp) {

    // save it, save it real good
    tasks[task_active].sp = sp;

    switch(code) {
    case SYS_CREATE:
        ksyscall_create(req, sp);
        break;
    case SYS_TID:
        ksyscall_tid(sp);
        break;
    case SYS_PTID:
        ksyscall_ptid(sp);
        break;
    case SYS_PASS:
        ksyscall_pass();
        break;
    case SYS_EXIT:
        ksyscall_exit();
        break;
    case SYS_PRIORITY:
	ksyscall_priority(sp);
	break;
    default:
        assert(false, "Task %d, called invalid system call %d",
	       tasks[task_active].tid,
	       code);
    }
}
