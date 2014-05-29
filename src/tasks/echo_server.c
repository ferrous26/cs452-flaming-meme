#include <io.h>
#include <debug.h>

#include <std.h>
#include <syscall.h>

void echo_server() {
    int tid = 99;
    char buffer[128];

    if( RegisterAs("echo") ) {
        debug_log( "ECHO:\tregister failiure!" );
	return;
    }
    debug_log( "ECHO:\tStarted!" );

    for(;;) {
        int siz = Receive(&tid, buffer, sizeof(buffer));
	UNUSED(siz);

	debug_log("ECHO:\tReceived message From %d of length %u saying %s",
		  tid,
		  siz,
		  buffer);

	Reply(tid, buffer, 4);
	debug_log("ECHO:\treply status was %u", siz);
    }
}
