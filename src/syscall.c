#include <io.h>
#include <debug.h>
#include <limits.h>
#include <scheduler.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>

#include <syscall.h>

inline static int _syscall(volatile int code, volatile void* request) {
    register int r0 asm ("r0") = code;
    register volatile void* r1 asm ("r1") = request;

    asm ("swi\t0"
	:
	:"r"(r0), "r"(r1)
	:"r2", "r3", "ip");

#ifdef CLANG
    // r0 will have the return value of the operation
    int ret;
    asm ("mov %0, r0"
	 : "=r" (ret));
    return ret;
#else
    // r0 will have the return value of the operation
    register int ret asm ("r0");
    return ret;
#endif
}

int Create(int priority, void (*code)()) {
    if (priority > TASK_PRIORITY_MAX || priority < TASK_PRIORITY_MIN)
        return INVALID_PRIORITY;

    volatile kreq_create req = {
        .code     = code,
        .priority = priority
    };
    return _syscall(SYS_CREATE, &req);
}

int myTid() {
    return _syscall(SYS_TID, NULL);
}

int myParentTid() {
    return _syscall(SYS_PTID, NULL);
}

int myPriority() {
    return _syscall(SYS_PRIORITY, NULL);
}

void ChangePriority(int new_priority) {
    _syscall(SYS_CHANGE, (volatile void*)new_priority);
}

void Pass() {
    _syscall(SYS_PASS, NULL);
}

void Exit() {
    _syscall(SYS_EXIT, NULL);
}

int Send(int tid, char* msg, int msglen, char* reply, int replylen) {
    volatile kreq_send req = {
        .tid      = tid,
        .msg      = msg,
        .reply    = reply,
        .msglen   = msglen,
        .replylen = replylen
    };
    return _syscall(SYS_SEND, &req);
}

int Receive(int *tid, char *msg, int msglen) {
    volatile kreq_recv req = {
        .tid    = tid,
        .msg    = msg,
        .msglen = msglen
    };
    return _syscall(SYS_RECV, &req);
}

int Reply(int tid, char *reply, int replylen) {
    volatile kreq_reply req = {
        .tid      = tid,
        .reply    = reply,
        .replylen = replylen
    };
    return _syscall(SYS_REPLY, &req);
}

int WhoIs(char* name) {
    ns_req req;
    req.type = LOOKUP;
    memset((void*)&req.payload, 0, sizeof(ns_payload));

    for(uint i = 0; name[i] != '\0'; i++) {
        if (i == NAME_MAX_SIZE) return -1;
	req.payload.text[i] = name[i];
    }

    int status;
    int result = Send(name_server_tid,
		      (char*)&req,    sizeof(req),
		      (char*)&status, sizeof(int));

    if (result > OK) return status;

    // maybe clock server died, so we can try again
    if (result == INVALID_TASK || result == INCOMPLETE)
	if (Create(TASK_PRIORITY_HIGH - 1, name_server) > 0)
	    return WhoIs(name);

    // else, error out
    return result;
}

int RegisterAs(char* name) {
    ns_req req;
    req.type = REGISTER;
    memset((void*)&req.payload, 0, sizeof(ns_payload));

    for (uint i = 0; name[i] != '\0'; i++) {
        if (i == NAME_MAX_SIZE) return -1;
	req.payload.text[i] = name[i];
    }

    int status;
    int result = Send(name_server_tid,
		      (char*)&req,    sizeof(req),
		      (char*)&status, sizeof(int));

    if (result > OK) return (status < 0 ? status :  0);

    // maybe clock server died, so we can try again
    if (result == INVALID_TASK || result == INCOMPLETE)
	if (Create(TASK_PRIORITY_HIGH - 1, name_server) > 0)
	    return RegisterAs(name);

    // else, error out
    return result;
}

int AwaitEvent(int eventid, char* event, int eventlen) {
    volatile kwait_req req = {
        .eventid  = eventid,
        .event    = event,
        .eventlen = eventlen
    };
    return _syscall(SYS_AWAIT, &req);
}

int Delay(int ticks) {

    // handle negative/non-delay cases on the task side
    if (ticks <= 0) return 0;

    clock_req req;
    req.type  = CLOCK_DELAY;
    req.ticks = ticks;

    int result = Send(clock_server_tid,
		      (char*)&req, sizeof(clock_req),
		      (char*)&req, sizeof(clock_req));

    switch (result) {
    case OK:
	return 0;
    case INVALID_TASK:
    case INCOMPLETE:
	// maybe clock server died, so we can try again
	if (Create(TASK_PRIORITY_HIGH - 1, clock_server) > 0)
	    return Delay(ticks);
	// fall through
    default:
	return result;
    }
}

int Time() {
    clock_req req;
    req.type = CLOCK_TIME;

    int ticks;
    int result = Send(clock_server_tid,
		      (char*)&req, sizeof(clock_req_type),
		      (char*)&ticks, sizeof(ticks));

    // Note: since we return _result_ in error cases, we inherit
    // additional error codes not in the kernel spec for this function
    if (result > OK) return ticks;

    // maybe clock server died, so we can try again
    if (result == INVALID_TASK || result == INCOMPLETE)
	if (Create(TASK_PRIORITY_HIGH - 1, clock_server) > 0)
	    return Time();

    // else, error out
    return result;
}

int DelayUntil(int ticks) {

    // handle negative/non-delay cases on the task side
    if (ticks <= 0) return 0;

    clock_req req;
    req.type  = CLOCK_DELAY_UNTIL;
    req.ticks = ticks;

    int result = Send(clock_server_tid,
		      (char*)&req, sizeof(clock_req),
		      (char*)&req, sizeof(clock_req));

    switch (result) {
    case OK:
	return 0;
    case INVALID_TASK:
    case INCOMPLETE:
	// maybe clock server died, so we can try again
	if (Create(TASK_PRIORITY_HIGH - 1, clock_server) > 0)
	    return DelayUntil(ticks);
	// fall through
    default:
	return result;
    }
}

void Shutdown() {
    _syscall(SYS_SHUTDOWN, NULL);
}

int Getc(int channel) {
    term_req req = {
	.type = GETC
    };

    char byte;

    switch (channel) {
    case TRAIN_CONTROLLER:
	return INVALID_CHANNEL;
    case TERMINAL: {
	int result = Send(term_server_tid,
			  (char*)&req, sizeof(term_req_type),
			  &byte, sizeof(byte));
	if (result > 0) return byte;
	return result;
    }
    default:
	return INVALID_CHANNEL;
    }
}

int Putc(int channel, char ch) {
    term_req req = {
	.type = PUTC,
	.size = ch
    };

    switch (channel) {
    case TRAIN_CONTROLLER:
	return INVALID_CHANNEL;
    case TERMINAL: {
	int result = Send(term_server_tid,
			  (char*)&req, sizeof(term_req_type) + sizeof(int),
			  NULL, 0);
	if (result > 0) return OK;
	return result;
    }
    default:
	return INVALID_CHANNEL;
    }
}

int Puts(char* const str, int length) {
    term_req req = {
	.type = PUTS,
	.size = length,
	.payload.string = str
    };

    int result = Send(term_server_tid,
		      (char*)&req,
		      sizeof(term_req_type) + sizeof(int) + sizeof(char*),
		      NULL,
		      0);
    if (result > 0) return OK;
    return result;
}
