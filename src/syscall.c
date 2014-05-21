#include <io.h>
#include <task.h>
#include <debug.h>
#include <scheduler.h>

#include <syscall.h>

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
    // save it, save it real good
    task_active->sp = sp;

    switch(code) {
    case SYS_CREATE: {
        kreq_create* r = (kreq_create*) request;
	sp[0] = task_create(task_active->tid, r->priority, r->code);
	scheduler_schedule(task_active);
        return 1;
    }
    case SYS_TID:
        sp[0] = (uint)task_active->tid;
	return 0;
    case SYS_PTID:
        sp[0] = (uint)task_active->p_tid;
        return 0;
    case SYS_PASS:
	scheduler_schedule(task_active);
        return 1;
    case SYS_EXIT:
	task_destroy(task_active);
        return 1;
    case SYS_PRIORITY:
        sp[0] = (uint)task_active->priority;
	return 0;
    default:
	// TODO: maybe find a way to tell us which task was the culprit
        return 1;
    }
}

int Create( int priority, void (*code) () ) {
    kreq_create req;
    req.code = code;
    req.priority = priority;

    if (priority > TASK_PRIORITY_MAX)
	return INVALID_PRIORITY;

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
