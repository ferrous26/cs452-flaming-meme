#include <syscall.h>
#include <kernel.h>
#include <io.h>
#include <debug.h>

static inline int
_syscall(volatile syscall_num code, volatile void* request)
{
    register int r0 asm ("r0") = code;
    register volatile void* r1 asm ("r1") = request;

    asm ("swi\t0"
         :
         : "r"(r0), "r"(r1)
         : "r2", "r3", "ip");

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

task_id Create(const task_priority priority, void (* const code)())
{
    if (priority > TASK_PRIORITY_MAX) return ERR_INVALID_PRIORITY;

    volatile kreq_create req = {
        .code     = code,
        .priority = priority
    };
    return _syscall(SYS_CREATE, &req);
}

task_id myTid()
{
    return _syscall(SYS_TID, NULL);
}

task_id myParentTid()
{
    return _syscall(SYS_PTID, NULL);
}

task_priority myPriority()
{
    return (task_priority)_syscall(SYS_PRIORITY, NULL);
}

task_priority ChangePriority(const task_priority priority)
{
    return (task_priority)_syscall(SYS_CHANGE, (volatile void*)priority);
}

void Pass()
{
    _syscall(SYS_PASS, NULL);
}

void Exit()
{
    _syscall(SYS_EXIT, NULL);
}

int Send(const task_id tid,
         const char* const msg, const size_t msglen,
         char* const reply,     const size_t replylen)
{
    volatile kreq_send req = {
        .tid      = tid,
        .msg      = msg,
        .reply    = reply,
        .msglen   = msglen,
        .replylen = replylen
    };
    return _syscall(SYS_SEND, &req);
}

int Receive(task_id* const tid,
            char* const msg, const size_t msglen)
{
    volatile kreq_recv req = {
        .tid    = tid,
        .msg    = msg,
        .msglen = msglen
    };
    return _syscall(SYS_RECV, &req);
}

int Reply(const task_id tid,
          char* const reply, const size_t replylen)
{
    volatile kreq_reply req = {
        .tid      = tid,
        .reply    = reply,
        .replylen = replylen
    };
    return _syscall(SYS_REPLY, &req);
}

int AwaitEvent(const event_id eid,
               char* const event, const size_t eventlen)
{
    if (eid >= EVENT_COUNT) return ERR_INVALID_EVENT;

    volatile kreq_event req = {
        .eid      = eid,
        .event    = event,
        .eventlen = eventlen
    };
    return _syscall(SYS_AWAIT, &req);
}

void Shutdown()
{
    _syscall(SYS_SHUTDOWN, NULL);
    FOREVER;
}

void Abort(const char* const file,
	   const uint line,
	   const char* const msg, ...)
{
    va_list args;
    va_start(args, msg);

    volatile kreq_abort req = {
        .file = (char*)file,
        .line = line,
        .msg  = (char*)msg,
        .args = &args
    };

    // if called from within the kernel, shortcut to the proper handler
    if (debug_processor_mode() == SUPERVISOR)
        abort((const kreq_abort* const)&req);

    _syscall(SYS_ABORT, &req);

    va_end(args);

    FOREVER; // so that flow analysis thinks the function does not return
}
