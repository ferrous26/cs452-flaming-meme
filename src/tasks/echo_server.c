#include <io.h>
#include <debug.h>

#include <std.h>
#include <syscall.h>

void echo_server() {
    int tid = 99;
    char buffer[128];

    if( RegisterAs("echo") ) {
        debug_log( "echo server register failiure" );
        vt_flush();
	return;
    }
    debug_log( "echo server registered!" );
    vt_flush();

    for(;;) {
        size siz = Receive(&tid, buffer, sizeof(buffer));

	vt_log("ECHO: Received message From %d of length %u saying %s",
		       tid,
		       siz,
		       buffer);

	Reply(tid, buffer, siz);
	vt_log("ECHO: reply status was %u", siz);
	vt_flush();
    }
}
