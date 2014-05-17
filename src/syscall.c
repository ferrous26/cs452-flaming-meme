#include <io.h>
#include <debug.h>
#include <syscall.h>

unsigned int  _syscall(int code) {
    // on init code will be in r0 so we can easily pass it to the handler
    (void) code;
    asm ( "swi 0" );

    // r0 will have the return value of the operation
    register unsigned int ret asm ("r0");
    return ret;
}

int syscall_handle (uint code, uint32* sp) {
    
    kprintf_char( '\n' );
    kprintf_ptr( (void*)code );
    kprintf_char( '\n' );
    kprintf_ptr( (void*)sp );

    sp[0] = 16;			     // write the value 16 to return
    // sp[14] = (uint32)syscall_handle; //modify the lr to return back to this function

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
