#include <io.h>
#include <debug.h>
#include <syscall.h>

void bootstrap(void) {
    debug_log("Enter Bootstrap");
    debug_cpsr();
    debug_log("To The Kernel!");
    vt_flush();
    int x = myTid();

    debug_log("Enter Bootstrap");
    debug_cpsr();
    debug_log("To The Kernel!");
    vt_flush();
    int y = myParentTid();

    debug_log("There Be Dragons %d %d", x, y);
    for(;;) {
         vt_flush();
    }
}
