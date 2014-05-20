#include <io.h>
#include <task.h>
#include <debug.h>
#include <syscall.h>
#include <scheduler.h>

//TODO: reqest should be a pointer to an object that hold all of the arguments
unsigned int  _syscall(int code, void* request) {
    // on init code will be in r0 so we can easily pass it to the handler
    UNUSED(code);
    UNUSED(request);
    asm("swi 0");

    // r0 will have the return value of the operation
    register unsigned int ret asm ("r0");
    return ret;
}

int syscall_handle (uint code, void* request, uint* sp) {
    //    kprintf( "\nIC:\t%p\nR1:\t%p\nSP:\t%p\n", code, request, sp);
    task_active->sp = sp;

    switch(code) {
    case SYS_CREATE: {
        kreq_create* r = (kreq_create*) request;
        //debug_log("CREATING TASK (%d): %p", r->priority, r->code);
	sp[0] = task_create(task_active->tid, r->priority, r->code);
	scheduler_schedule(task_active);
	scheduler_get_next();
	break;
    }
    case SYS_TID:
        sp[0] = (uint)task_active->tid;
        break;
    case SYS_PTID:
        sp[0] = (uint)task_active->p_tid;
        break;
    case SYS_PASS:
	scheduler_schedule(task_active);
	scheduler_get_next();
	break;
    case SYS_EXIT:
	debug_log("Task %d exiting", task_active->tid);
	task_destroy(task_active); // this works, but I wanted to test something...
	break;
    case SYS_PRIORITY:
        sp[0] = (uint)task_active->priority;
        break;
    default:
	assert(false, "Invalid system call");
    }

    vt_flush();
    scheduler_activate();
    return 0;
}

int Create( int priority, void (*code) () ) {
    kreq_create req;
    req.code = code;
    req.priority = priority;

    // TODO: check priority level is valid

    return _syscall(SYS_CREATE, &req);
}

int myTid() {
    return _syscall(SYS_TID, NULL);
}

int myParentTid() {
    return _syscall(SYS_PTID, NULL);
}

int myPriority() {
    return _syscall(SYS_PRIORITY, NULL);
}

void Pass() {
    _syscall(SYS_PASS, NULL);
}

void Exit() {
    _syscall(SYS_EXIT, NULL);
}
