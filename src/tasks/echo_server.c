#include <io.h>
#include <debug.h>

#include <std.h>
#include <syscall.h>


void echo_server() {
    int tid;
    char buffer[256];

    for(;;) {
        Receive(&tid, buffer, sizeof(buffer));

	debug_log("ECHO: Received message From %d starting with %c",
                  tid,
                  buffer[0]);
	vt_flush();
        
	Reply(tid, buffer, sizeof(buffer));
    }
}

