#include <debug.h>
#include <syscall.h>
#include <memory.h>
#include <kernel.h>

#include <irq.h>
#include <scheduler.h>

void syscall_init() {
    *SWI_HANDLER = (0xea000000 | (((int)kernel_enter >> 2) - 4));
}

inline static void ksyscall_pass() {
    scheduler_schedule((task*)task_active);
}

inline static void ksyscall_create(const kreq_create* const req, uint* const result) {
    *result = task_create(req->priority, req->code);
    ksyscall_pass();
}

inline static void ksyscall_tid(uint* const result) {
    *result = (uint)task_active->tid;
    ksyscall_pass();
}

inline static void ksyscall_ptid(uint* const result) {
    *result = (uint)task_active->p_tid;
    ksyscall_pass();
}

inline static void ksyscall_exit() {
    task_destroy();
}

inline static void ksyscall_priority(uint* const result) {
    *result = (uint)task_active->priority;
    ksyscall_pass();
}

inline static void ksyscall_change_priority(const uint32 new_priority) {
    task_active->priority = new_priority;
    ksyscall_pass();
}

inline static void ksyscall_recv(task* const receiver) {
#if DEBUG
    register uint sp asm ("sp");
    assert(sp < 0x300000 && sp > 0x200000, "Recv: Smashed the stack");
#endif

    task_q* const q = &recv_q[task_index_from_tid(receiver->tid)];
   
    if (q->head) {
	task* const sender = q->head;

        assert((uint)sender < 0x200000 && (uint)sender > 0x100000,
               "Receive: %d Invalid head pointer!", receiver->tid);
        assert((uint) q->head->next < 0x200000 &&
               ((uint)q->head->next > 0x100000 || (uint)q->head->next == NULL),
               "Receive: %d Invalid head next! %p", receiver->tid, sender->next);
        q->head = q->head->next; // consume

	kreq_send* const   sender_req = (kreq_send* const)sender->sp[1];
	kreq_recv* const receiver_req = (kreq_recv* const)receiver->sp[1];

	// validate that there is enough space in the receiver buffer
	if (sender_req->msglen > receiver_req->msglen) {
	    sender->sp[0] = (uint)NOT_ENUF_MEMORY;
	    scheduler_schedule((task*)sender);
	    ksyscall_recv(receiver); // DANGER: recursive call
	    return;
	}

	// all is good, copy the message over and get back to work!
	*receiver_req->tid = sender->tid;
	receiver->sp[0]    = sender_req->msglen;
	sender->next       = RPLY_BLOCKED;

        if (sender_req->msglen)
	    memcpy(receiver_req->msg, sender_req->msg, sender_req->msglen);
	scheduler_schedule(receiver); // schedule this mofo
	return;
    }

    receiver->next = RECV_BLOCKED;
}

inline static void ksyscall_send(const kreq_send* const req, uint* const result) {
    task* const receiver = &tasks[task_index_from_tid(req->tid)];

    assert(task_index_from_tid(req->tid) < TASK_MAX,
           "Reply: Invalid Sender %d", req->tid);
    assert((uint)receiver->sp > TASK_HEAP_BOT && (uint)receiver->sp < TASK_HEAP_TOP,
           "Send: Sender %d has Invlaid heap %p", receiver->tid, receiver->sp);

    // validate the request arguments
    if (receiver->tid != req->tid || !receiver->sp) {
	*result = (uint)(req->tid < 0 ? IMPOSSIBLE_TASK : INVALID_TASK);
	ksyscall_pass();
        return;
    }

    assert(task_active->tid != req->tid,
	   "Tried to Send to self! (%u)", task_active->tid);

    /**
     * What we want to do is find the receive q and append to it, as we
     * did in the scheduler.
     */
    task_q* const q = &recv_q[task_index_from_tid(receiver->tid)];

    if (!q->head)
	q->head = (task*)task_active;
    else
	q->tail->next = (task*)task_active;

    q->tail = (task*)task_active;
    task_active->next = NULL;

    /**
     * Now, if the receiving task is receive blocked, then we need to wake it up
     * and schedule it so that it can receive the message.
     */
    if (receiver->next == RECV_BLOCKED) {
	ksyscall_recv(receiver);
    }
}

inline static void ksyscall_reply(const kreq_reply* const req, uint* const result) {
    task* const sender = &tasks[task_index_from_tid(req->tid)];

    // first, validation of the request arguments
    if (sender->tid != req->tid || !sender->sp) {
	*result = (uint)(req->tid < 0 ? IMPOSSIBLE_TASK : INVALID_TASK);
        ksyscall_pass();
	return;
    }
    
    assert((uint)sender->sp > TASK_HEAP_BOT && (uint)sender->sp <= TASK_HEAP_TOP,
            "Reply: Sender %d has Invalid heap %p", sender->tid, sender->sp);

    if (sender->next != RPLY_BLOCKED) {
	*result = (uint)INVALID_RECVER;
        ksyscall_pass();
	return;
    }

    const kreq_send* const sender_req = (const kreq_send* const)sender->sp[1];

    if (req->replylen > sender_req->replylen) {
	*result = (uint)NOT_ENUF_MEMORY;
        ksyscall_pass();
	return;
    }

    task_active->sp[0] = 0; // success!
    sender->sp[0]      = req->replylen;

    // at this point, it is smooth sailing
    if (req->replylen)
        memcpy(sender_req->reply, req->reply, req->replylen);

    // according to spec, we should schedule the sender first, because, in
    // the case where they are the same priority level we want the sender
    // to run first
    scheduler_schedule(sender);
    ksyscall_pass();
}

inline static void
ksyscall_await(const kwait_req* const req, uint* const result) {
    switch (req->eventid) {
    case CLOCK_TICK:
	task_events[CLOCK_TICK] = (task*)task_active;
	break;
    default:
	*result = INVALID_EVENT;
        ksyscall_pass();
    }
}

inline static void ksyscall_irq() {
    ksyscall_pass();

    voidf handler = VIC_PTR(VIC1_BASE, VIC_VECTOR_ADDRESS);
    handler();

    register void** r0 asm("r0");
    VIC_PTR(VIC1_BASE, VIC_VECTOR_ADDRESS) = r0;
}

void  __attribute__ ((naked)) syscall_handle(const uint code,
					     const void* const req,
					     uint* const sp) {
    // save it, save it real good
    task_active->sp = sp;

    switch(code) {
    case SYS_IRQ:
        ksyscall_irq();
        break;
    case SYS_CREATE:
        ksyscall_create((const kreq_create* const)req, sp);
        break;
    case SYS_TID:
        ksyscall_tid(sp);
        break;
    case SYS_PTID:
        ksyscall_ptid(sp);
        break;
    case SYS_PASS:
        ksyscall_pass();
        break;
    case SYS_EXIT:
        ksyscall_exit();
        break;
    case SYS_PRIORITY:
	ksyscall_priority(sp);
	break;
    case SYS_SEND:
	ksyscall_send((const kreq_send* const)req, sp);
	break;
    case SYS_RECV:
	ksyscall_recv((task*)task_active);
	break;
    case SYS_REPLY:
	ksyscall_reply((const kreq_reply* const)req, sp);
	break;
    case SYS_CHANGE:
	ksyscall_change_priority((uint)req);
	break;
    case SYS_AWAIT:
	ksyscall_await((const kwait_req* const)req, sp);
	break;
    case SYS_SHUTDOWN:
	shutdown();
	break;
    default:
        assert(false, "Task %d, called invalid system call %d",
	       task_active->tid,
	       code);
    }

    scheduler_get_next();
    //everythings done, just leave
    asm volatile ("mov\tr0, %0    \n\t"
                  "b\tkernel_exit \n\t"
                 :
                 :"r" (task_active->sp)
                 :"r0");
}
