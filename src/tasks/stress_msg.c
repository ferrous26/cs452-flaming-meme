#include <vt100.h>
#include <std.h>
#include <syscall.h>
#include <scheduler.h>

int stressors[8];

void stress_msg() {

    if (myPriority() == 31) {
	vt_log("Starting message passing stress test");
	vt_flush();

	for (size i = 0; i < 8; i++)
	    stressors[i] = 0;

	for (size i = 22; i < 31; i++) {
	    int tid = Create(i, stress_msg);
	    vt_log("Created %u", tid);
	    vt_flush();
	}

	return;
    }
    else if (myPriority() > 21) {
	stressors[myPriority() - 22] = myTid();

	for (size i = 0; i < 8; i++) {
	    int tid = Create(20, stress_msg);
	    vt_log("created child %u", tid);
	    vt_flush();
	}

	ChangePriority(19);

	int tid;
	char buf[4];

	for (;;) {
	    int siz = Receive(&tid, buf, 4);
	    vt_log("Got message from %d of size %d saying %s", tid, siz, buf);
	    vt_flush();
	    Reply(tid, buf, 4);
	}
    }

    const char* msg = "hey";
    char recv[4];
    for (;;) {
	for (size i = 0; i < 8; i++) {
    	    int resp = Send(stressors[i], (char*)msg, 4, recv, 4);
    	    vt_log("Got reply status %d saying %s", resp, recv);
    	    vt_flush();
	}
    }
}
