#include <io.h>
#include <debug.h>

#include <std.h>
#include <syscall.h>

void echo_server() {
    int tid = 99;
    char buffer[128];

    if( RegisterAs("echo") ) {
        debug_log( "echo server register failiure" );
	return;
    }
    debug_log( "echo server registered!" );

    for(;;) {
        int siz = Receive(&tid, buffer, sizeof(buffer));
	UNUSED(siz);

	debug_log("ECHO: Received message From %d of length %u saying %s",
		  tid,
		  siz,
		  buffer);

	Reply(tid, buffer, 4);
	debug_log("ECHO: reply status was %u", siz);
    }
}
