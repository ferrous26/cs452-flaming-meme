#include <io.h>
#include <debug.h>

#include <std.h>
#include <syscall.h>


void echo_server() {
    int tid = 99;
    char buffer[256];

    for(;;) {
        int siz = Receive(&tid, buffer, sizeof(buffer));

	debug_log("ECHO: Received message From %d saying %s",
                  tid,
                  buffer);
	vt_flush();

	Reply(tid, buffer, siz);
    }
}
