
#include <io.h>
#include <debug.h>
#include <syscall.h>

void bootstrap() {
    debug_log("Enter Bootstrap");

    for (;;) {
        debug_log("BOOTSTRAP TOP");
        int id = myTid();	
        debug_log("BOOTSTRAP BOT %u", id);
    }
}

