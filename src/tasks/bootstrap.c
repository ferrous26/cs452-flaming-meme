#include <io.h>
#include <debug.h>
#include <syscall.h>

void bootstrap(void) {


    for(;;){
        debug_log("Enter Bootstrap");
        vt_flush();
    }
}

