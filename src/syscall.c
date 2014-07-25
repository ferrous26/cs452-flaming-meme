#include <syscall.h>
#include <kernel.h>
#include <io.h>
#include <debug.h>

inline static int _syscall(volatile syscall_num code, volatile void* request) {
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

int ChangePriority(int priority) {
    return _syscall(SYS_CHANGE, (volatile void*)priority);
}

void Pass() {
    _syscall(SYS_PASS, NULL);
}

void Exit() {
    _syscall(SYS_EXIT, NULL);
}

int Send(int tid, char* msg, int msglen, char* reply, int replylen) {
    assert(msglen   >= 0, "NEGITIVE MESSAGE %d", msglen);
    assert(replylen >= 0, "REPLY MESSAGE %d", replylen);
    
    volatile kreq_send req = {
        .tid      = tid,
        .msg      = msg,
        .reply    = reply,
        .msglen   = msglen,
        .replylen = replylen
    };
    return _syscall(SYS_SEND, &req);
}

int Receive(int* tid, char* msg, int msglen) {
    assert(msglen   >= 0, "NEGITIVE MESSAGE %d", msglen);
    volatile kreq_recv req = {
        .tid    = tid,
        .msg    = msg,
        .msglen = msglen
    };
    return _syscall(SYS_RECV, &req);
}

int Reply(int tid, char* reply, int replylen) {
    assert(replylen >= 0, "REPLY MESSAGE %d", replylen);

    volatile kreq_reply req = {
        .tid      = tid,
        .reply    = reply,
        .replylen = replylen
    };
    return _syscall(SYS_REPLY, &req);
}

int AwaitEvent(int eventid, char* event, int eventlen) {
    volatile kreq_event req = {
        .eventid  = eventid,
        .event    = event,
        .eventlen = eventlen
    };
    return _syscall(SYS_AWAIT, &req);
}

void Shutdown() {
    _syscall(SYS_SHUTDOWN, NULL);
    FOREVER;
}

void Abort(const char* const file,
	   const uint line,
	   const char* const msg, ...) {

    va_list args;
    va_start(args, msg);

    volatile kreq_abort req = {
        .file = (char*)file,
        .line = line,
        .msg  = (char*)msg,
        .args = &args
    };

    if (debug_processor_mode() == SUPERVISOR)
        abort((const kreq_abort* const)&req);

    _syscall(SYS_ABORT, &req);

    va_end(args);
    FOREVER;
}
