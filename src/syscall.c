#include <io.h>
#include <debug.h>
#include <limits.h>
#include <scheduler.h>
#include <tasks/name_server.h>

#include <syscall.h>

task_id name_server_tid;

uint __attribute__ ((noinline)) _syscall(int code, void* request) {
    // on init code will be in r0 so we can easily pass it to the handler
    UNUSED(code);
    UNUSED(request);
    asm("swi 0");

    // r0 will have the return value of the operation
    register unsigned int ret asm ("r0");
    return ret;
}

int Create( int priority, void (*code) () ) {
    kreq_create req;
    req.code     = code;
    req.priority = priority;

    if (priority > TASK_PRIORITY_MAX)
        return INVALID_PRIORITY;

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
    kreq_send req;
    req.tid      = tid;
    req.msg      = msg;
    req.reply    = reply;
    req.msglen   = msglen;
    req.replylen = replylen;

    return _syscall(SYS_SEND, &req);
}

int Receive(int *tid, char *msg, int msglen) {
    kreq_recv req;
    req.tid    = tid;
    req.msg    = msg;
    req.msglen = msglen;

    return _syscall(SYS_RECV, &req);
}

int Reply(int tid, char *reply, int replylen) {
    kreq_reply req;
    req.tid      = tid;
    req.reply    = reply;
    req.replylen = replylen;

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
