#include <io.h>
#include <task.h>
#include <debug.h>
#include <scheduler.h>
#include <syscall.h>

inline static void ksyscall_pass() {
    scheduler_schedule(task_active);
}

inline static void ksyscall_create(kreq_create* req, uint* result) {
    *result = task_create(task_active->tid, req->priority, req->code);
    ksyscall_pass();
}

inline static void ksyscall_tid(uint* result) {
    *result = (uint)task_active->tid;
    ksyscall_pass();
}

inline static void ksyscall_ptid(uint* result) {
    *result = (uint)task_active->p_tid;
    ksyscall_pass();
}

inline static void ksyscall_exit() {
    task_destroy(task_active);
}

inline static void ksyscall_priority(uint* result) {
    *result = (uint)task_active->priority;
    ksyscall_pass();
}

void syscall_handle (uint code, void* req, uint* sp) {
    // save it, save it real good
    task_active->sp = sp;

    switch(code) {
    case SYS_CREATE:
        ksyscall_create((kreq_create*) req, sp);
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
