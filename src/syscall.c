#include <io.h>
#include <debug.h>
#include <limits.h>
#include <scheduler.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <syscall.h>

task_id name_server_tid;
task_id clock_server_tid;

inline uint _syscall(int code, volatile void* request) {
    // on init code will be in r0 so we can easily pass it to the handler
    asm volatile ("mov\tr0, %0\n\t"
                  "mov\tr1, %1\n\t"
                  "swi\t0"
                 :
                 :"r"(code), "r"(request)
                 :"r0", "r1", "r2", "r3");

    // r0 will have the return value of the operation
    register unsigned int ret asm ("r0");
    return ret;
}

int Create( int priority, void (*code) () ) {
    if (priority > TASK_PRIORITY_MAX)
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

void ChangePriority(uint new_priority) {
    _syscall(SYS_CHANGE, (uint*)new_priority);
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

    int ret;
    int res = Send(name_server_tid,
                   (char*)&req, sizeof(req),
                   (char*)&ret, sizeof(int));
    if(res < 0) return res;
    return ret;
}

int RegisterAs(char* name) {
    ns_req req;
    req.type = REGISTER;
    memset((void*)&req.payload, 0, sizeof(ns_payload));

    for(uint i = 0; name[i] != '\0'; i++) {
        if (i == NAME_MAX_SIZE) return -1;
	req.payload.text[i] = name[i];
    }

    int ret;
    int res = Send(name_server_tid,
                   (char*)&req, sizeof(req),
                   (char*)&ret, sizeof(int));

    if(res < 0) return res;
    if(ret < 0) return ret;
    return 0;
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
    clock_req req;
    req.type  = CLOCK_DELAY;
    req.ticks = ticks;

    int result = Send(clock_server_tid,
		      (char*)&req, sizeof(clock_req),
		      (char*)&req, sizeof(clock_req));
    if (result < 0) return result;
    return 0;
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
    if (result < 0) return result;
    return ticks;
}

int DelayUntil(int ticks) {
    clock_req req;
    req.type  = CLOCK_DELAY_UNTIL;
    req.ticks = ticks;

    int result = Send(clock_server_tid,
		      (char*)&req, sizeof(clock_req),
		      (char*)&req, sizeof(clock_req));
    if (result < 0) return result;
    return 0;
}
