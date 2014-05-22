#include <io.h>
#include <debug.h>
#include <scheduler.h>

#include <syscall.h>

// TODO: what if this fucker gets inlined?
unsigned int _syscall(int code, void* request) {
    // on init code will be in r0 so we can easily pass it to the handler
    UNUSED(code);
    UNUSED(request);
    asm("swi 0");

    // r0 will have the return value of the operation
    register unsigned int ret asm ("r0");
    return ret;
}

int Create( int priority, void (*code) () ) {
    kreq_create req;
    req.code = code;
    req.priority = priority;

    if (priority > TASK_PRIORITY_MAX) {
        return INVALID_PRIORITY;
    }
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
