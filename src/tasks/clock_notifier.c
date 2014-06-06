#include <tasks/clock_notifier.h>
#include <tasks/clock_server.h>
#include <syscall.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>

void clock_notifier() {

    int clock = WhoIs((char*)"clock");
    if (clock < 0) {
	vt_log("Clock server not found (%d)", clock);
	vt_flush();
	return;
    }

    // TODO: make this configurable
    int msg = CLOCK_NOTIFY;

    FOREVER {
	int result = AwaitEvent(CLOCK_TICK, NULL, 0);

	UNUSED(result); // it will be unused when asserts are off
	assert(result == 0, "Impossible clock error code (%d)", result);

	// We don't actually need to send anything to the clock
	// server, but sending 0 bytes is going to go down a bad path
	// in memcpy, so send the most optimal case (one word).
	result = Send(clock,
		      (char*)&msg, sizeof(msg),
		      (char*)&msg, sizeof(msg));
	if (result < 0) {
	    vt_log("Failed to send to clock (%d)", result);
	    vt_flush();
	    return;
	}
    }

}
