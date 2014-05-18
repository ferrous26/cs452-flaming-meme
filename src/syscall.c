#include <io.h>
#include <task.h>
#include <debug.h>
#include <syscall.h>

extern task* active_task;

unsigned int  _syscall(int code) {
    // on init code will be in r0 so we can easily pass it to the handler
    (void) code;
    asm ( "swi 0" );

    // r0 will have the return value of the operation
    register unsigned int ret asm ("r0");
    return ret;
}

int syscall_handle (uint code, uint32* req, void** sp, uint32 spsr) {
    kprintf( "\nIC:\t%p\nR1:\t%p\nSP:\t%p\nSPSR:\t%p\n", code, req, sp, spsr);
    active_task->sp = sp;

    debug_log( "Active_Task %p", active_task );

    debug_log( "Interrupt!" );
    debug_cpsr();
    debug_log( "Exiting Interrupt!" );

    return 0;
}

int Create( int priority, void (*code) () ) {
    (void) priority;
    (void) code;
    return _syscall(SYS_CREATE);
}

int myTid() {
    return _syscall(SYS_TID);
}

int myParentTid() {
    return _syscall(SYS_PTID);
}

void Pass() {
    _syscall(SYS_PASS);
}

void Exit() {
    _syscall(SYS_EXIT);
}

