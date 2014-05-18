#include <io.h>
#include <debug.h>
#include <syscall.h>

void bootstrap(void) {
    debug_log("Enter Bootstrap");
    debug_cpsr();
    debug_log("To The Kernel!");
    vt_flush();

    for(;;) {
         myTid();
         debug_log("There Be Dragons");
         vt_flush();
    }
}

