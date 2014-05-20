#include <io.h>
#include <debug.h>
#include <syscall.h>

void bootstrap(void) {

    //    debug_log("Enter Bootstrap");
    //    debug_log("To The Kernel!");
    //    vt_flush();

    int x = myTid();
    //   debug_log("My Id is ~ %d", x);
    //   vt_flush();
    //   debug_log("To The Kernel!");

    int y = myParentTid();
    //   debug_log("I was created by ~ %d", y);
    //   vt_flush();
    //    debug_log("To The Kernel!");

    int z = myPriority();
    //    debug_log("My priority is ~ %d", z);
    //    vt_flush();
    //    debug_log("To The Kernel!");

    Create(z + 1, bootstrap);
    debug_log("There Be Dragons %d %d", x, y);
    vt_flush();
}
