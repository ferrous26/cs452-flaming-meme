#include <io.h>
#include <debug.h>

#include <std.h>
#include <syscall.h>


void echo_server() {
    int tid = 99;
    char buffer[4];

    for(;;) {
        Receive(&tid, buffer, 4);

	//vt_log("ECHO: Received message From %d of length %u saying %s",
	//	       tid,
	//	       siz,
	//	       buffer);
	//vt_flush();

	Reply(tid, buffer, 4);
	//vt_log("ECHO: reply status was %u", siz);
	//vt_flush();
    }
}
