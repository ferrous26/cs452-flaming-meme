#include <io.h>
#include <task.h>
#include <debug.h>
#include <syscall.h>

extern task* active_task;

//TODO: reqest should be a pointer to an object that hold all of the arguments
unsigned int  _syscall(int code, void* request) {
    // on init code will be in r0 so we can easily pass it to the handler
    UNUSED(code);
    UNUSED(request);
    asm ( "swi 0" );

    // r0 will have the return value of the operation
    register unsigned int ret asm ("r0");
    return ret;
}

int syscall_handle (uint code, uint32* req, void** sp, uint32 spsr) {
    kprintf( "\nIC:\t%p\nR1:\t%p\nSP:\t%p\nSPSR:\t%p\n", code, req, sp, spsr);
    active_task->sp = sp;

    switch(code) {
    case SYS_CREATE:
        break;
    case SYS_TID:
        sp[0] = (void*) active_task->tid;
        break;
    case SYS_PTID:
        sp[0] = (void*) active_task->p_tid;	
        break;
    case SYS_PASS:
	break;
    case SYS_EXIT:
	break;
    default:
	debug_log("why am i here?");
    }

    return 0;
}

int Create( int priority, void (*code) () ) {
    (void) priority;
    (void) code;
    return _syscall(SYS_CREATE, NULL);
}

int myTid() {
    return _syscall(SYS_TID, NULL);
}

int myParentTid() {
    return _syscall(SYS_PTID, NULL);
}

void Pass() {
    _syscall(SYS_PASS, NULL);
}

void Exit() {
    _syscall(SYS_EXIT, NULL);
}

