#include <tasks/test_msg.h>
#include <vt100.h>
#include <std.h>
#include <syscall.h>
#include <scheduler.h>
#include <rand.h>

static int stressors[8];

static void stress_msg() {

    if (myPriority() == 31) {
	srand(__BUILD__);

	vt_log("Starting message passing stress test");
	vt_flush();

	for (size i = 0; i < 8; i++)
	    stressors[i] = 0;

	for (size i = 22; i < 31; i++) {
	    int tid = Create(i, test_msg);
	    vt_log("Created %u", tid);
	    vt_flush();
	    if (tid < 0) {
		for (size j = 0; j < 8; j++)
		    stressors[j] = 0;
		return;
	    }
	}

	return;
    }
    else if (myPriority() > 21) {
	stressors[myPriority() - 22] = myTid();

	for (size i = 0; i < 8; i++) {
	    int tid = Create(20, test_msg);
	    vt_log("created child %u", tid);
	    vt_flush();
	}

	ChangePriority(19);

	int mTid = myTid();
	int tid;
	char buf[4];

	for (size i = 100000; i; i--) {
	    int siz = Receive(&tid, buf, 4);
	    vt_log("%u got msg from %d of size %d saying %s",
		   mTid, tid, siz, buf);
	    vt_flush();
	    Reply(tid, buf, 4);
	}
    }

    const char* msg = "hey";
    char recv[4];
    for (size i = 100000; i; i--) {
	int resp = Send(stressors[mod2(rand(), 8)], (char*)msg, 4, recv, 4);
	if (resp > 0)
	    vt_log("Got reply status %d saying %s", resp, recv);
	else
	    vt_log("Got reply status %d", resp);

	vt_flush();
    }
}

void test_msg() {

    // test the send error cases
    // -1 impossible task
    // -2 invalid task (not allocated)
    // -2 invalid task (dead task)
    // -4 not enuf memory

    // test the recv error cases
    // test the reply error cases
    // send before receive
    // receive before send

    stress_msg();
}
