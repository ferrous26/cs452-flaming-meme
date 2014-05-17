#include <io.h>
#include <debug.h>
#include <syscall.h>

void bootstrap(void) {
    debug_log("Enter Bootstrap");

    for (;;) {
        debug_log("BOOTSTRAP TOP");
        int id = myTid();
        debug_log("BOOTSTRAP BUTT %u", id);
    }
}
