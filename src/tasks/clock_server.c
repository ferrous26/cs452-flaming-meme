#include <tasks/clock_server.h>
#include <tasks/clock_notifier.h>
#include <syscall.h>
#include <priority_queue.h>
#include <std.h>
#include <debug.h>
#include <scheduler.h>
#include <vt100.h>

void clock_server() {

    assert(myPriority() < TASK_PRIORITY_MAX,
	   "Clock server should not be top priority");

    int result = RegisterAs("clock");
    if (!result) {
	vt_log("Failed to register clock server name (%d)", result);
	vt_flush();
	return;
    }

    result = Create(TASK_PRIORITY_MAX, clock_notifier);
    if (result < 0) {
	vt_log("Failed to create clock_notifier (%d)", result);
	vt_flush();
	return;
    }

    uint time = 0;
    clock_req req;

    vt_goto_home();
    kprintf_string("CLOCK ");
#define CLOCK_HOME 7

    FOREVER {
	int  tid;
	size siz = Receive(&tid, (char*)&req, sizeof(req));
	switch (req.type) {
	case CLOCK_NOTIFY:
	    time++;
	    vt_goto(1, CLOCK_HOME);
	    kprintf_uint(time);
	    // can we pop someone off the queue?
	    // then do it
	    // recurse until we can't pop someone off the queue
	    break;
	case CLOCK_DELAY:
	    // calculate absolute time and add to queue
	    break;
	case CLOCK_TIME:
	    result = Reply(tid, (char*)&time, sizeof(time));
	    if (!siz) {
		vt_log("Failed to reply to %u (%d)", tid, result);
		vt_flush();
	    }
	    break;
	case CLOCK_DELAY_UNTIL:
	    // check that delay is valid?
	    // add to queue
	    break;
	default:
	    assert(false, "Invalid clock request %d", req.type);
	}
    }
}
