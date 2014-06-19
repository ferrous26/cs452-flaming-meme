#include <debug.h>
#include <syscall.h>
#include <memory.h>
#include <kernel.h>
#include <io.h>
#include <vt100.h>
#include <irq.h>
#include <ts7200.h>
#include <scheduler.h>

void __attribute__ ((noinline)) ksyscall_abort(const kreq_abort* const req);

void syscall_init() {
    *SWI_HANDLER = (0xea000000 | (((uint)kernel_enter >> 2) - 4));
}

void syscall_deinit() {
    *SWI_HANDLER = 0xE59FF018;
}

inline static void ksyscall_pass() {
    scheduler_schedule((task*)task_active);
}

inline static void ksyscall_create(const kreq_create* const req, int* const result) {
    *result = task_create(req->priority, req->code);
    ksyscall_pass();
}

inline static void ksyscall_tid(int* const result) {
    *result = task_active->tid;
    ksyscall_pass();
}

inline static void ksyscall_ptid(int* const result) {
    *result = task_active->p_tid;
    ksyscall_pass();
}

inline static void ksyscall_exit() {
    task_destroy();
}

inline static void ksyscall_priority(int* const result) {
    *result = task_active->priority;
    ksyscall_pass();
}

inline static void ksyscall_change_priority(const int32 new_priority) {
    task_active->priority = new_priority;
    ksyscall_pass();
}

inline static void ksyscall_recv(task* const receiver) {
#ifdef DEBUG
    uint sp;
    asm volatile ("mov %0, sp" : "=r" (sp));
    assert(sp < 0x300000 && sp > 0x200000, "Recv: Smashed the stack");
#endif
    task_q* const q = &recv_q[task_index_from_tid(receiver->tid)];

    if (q->head) {
	task* const sender = q->head;

        assert((uint)sender < 0x200000 && (uint)sender > 0x100000,
	       "Receive: %d Invalid head pointer!", receiver->tid);

        assert((uint) q->head->next < 0x200000 &&
	       ((uint)q->head->next > 0x100000 || (uint)q->head->next == NULL),
	       "Receive: %d Invalid head next! %p",
	       receiver->tid, sender->next);

        q->head = q->head->next; // consume

	kreq_send* const   sender_req = (kreq_send* const)sender->sp[1];
	kreq_recv* const receiver_req = (kreq_recv* const)receiver->sp[1];

	// validate that there is enough space in the receiver buffer
	if (sender_req->msglen > receiver_req->msglen) {
	    sender->sp[0] = NOT_ENUF_MEMORY;
	    scheduler_schedule(sender);
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

inline static void ksyscall_send(const kreq_send* const req, int* const result) {
    task* const receiver = &tasks[task_index_from_tid(req->tid)];

    // validate the request arguments
    if (receiver->tid != req->tid || !receiver->sp) {
	*result = req->tid < 0 ? IMPOSSIBLE_TASK : INVALID_TASK;
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
    if (receiver->next == RECV_BLOCKED)
	ksyscall_recv(receiver);
}

inline static void ksyscall_reply(const kreq_reply* const req, int* const result) {
    task* const sender = &tasks[task_index_from_tid(req->tid)];

    // first, validation of the request arguments
    if (sender->tid != req->tid || !sender->sp) {
	*result = req->tid < 0 ? IMPOSSIBLE_TASK : INVALID_TASK;
        ksyscall_pass();
	return;
    }

    if (sender->next != RPLY_BLOCKED) {
	*result = INVALID_RECVER;
        ksyscall_pass();
	return;
    }

    const kreq_send* const sender_req = (const kreq_send* const)sender->sp[1];

    if (req->replylen > sender_req->replylen) {
	*result = NOT_ENUF_MEMORY;
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

static inline void
ksyscall_await(const kreq_event* const req) {
    #ifdef DEBUG
    assert(!int_queue[req->eventid],
           "Event Task Collision (%d - %d) has happened on event %d",
           ((task*)int_queue[req->eventid])->tid, task_active->tid, req->eventid);

    assert(req->eventid >= CLOCK_TICK && req->eventid < EVENT_COUNT,
	   "Invalid event (%d)", req->eventid);
    #endif

    switch (req->eventid) {
    case CLOCK_TICK:
        int_queue[CLOCK_TICK] = task_active;
        break;
    case UART2_SEND: {
        int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
        *ctlr |= TIEN_MASK;
        int_queue[UART2_SEND] = task_active;
        break;
    }
    case UART2_RECV: {
        int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
        *ctlr |= RTIEN_MASK;

        int_queue[UART2_RECV] = task_active;
        break;
    }
    case UART1_SEND: {
        int* const ctlr = (int*)(UART1_BASE + UART_CTLR_OFFSET);
        *ctlr |= TIEN_MASK;
        int_queue[UART1_SEND] = task_active;
        break;
    }
    case UART1_RECV:
        int_queue[UART1_RECV] = task_active;
        break;
    case UART1_DOWN: {
        int* const flag = (int*)(UART1_BASE + UART_FLAG_OFFSET);
        if (*flag & CTS_MASK) {
            int_queue[UART1_DOWN] = task_active;
        } else {
            scheduler_schedule(task_active);
        }
        break;
    }
    case UART1_CTS: {
        int* const flag = (int*)(UART1_BASE + UART_FLAG_OFFSET);
        if (*flag & CTS_MASK) {
            scheduler_schedule(task_active);
        } else {
            int_queue[UART1_CTS] = task_active;
        }
        break;
    }
    default:
	break;
    }
}

inline static void ksyscall_irq() {
    voidf handler = (voidf)VIC_PTR(VIC1_BASE, VIC_VECTOR_ADDRESS);
    VIC_PTR(VIC1_BASE, VIC_VECTOR_ADDRESS) = (void*)handler;
    handler();
    ksyscall_pass();
}

#ifdef DEBUG
extern const int _TextStart;
extern const int _TextEnd;

static inline bool __attribute__ ((pure)) is_valid_pc(const int* const sp) {
    int pc = sp[2] == 0 ? sp[17] : sp[2];

    if (pc >= (int)&_TextStart && pc <= (int)&_TextEnd) {
        return 0;
    }

    return pc;
}
#endif

void syscall_handle(const uint code, const void* const req, int* const sp)
    __attribute__ ((naked)) TEXT_HOT;
void syscall_handle(const uint code, const void* const req, int* const sp) {
#ifdef DEBUG
    assert((uint)sp > TASK_HEAP_BOT && (uint)sp <= TASK_HEAP_TOP,
	   "Reply: task %d has Invalid heap %p", task_active->tid, sp);
    assert(!is_valid_pc(sp), "Task %d has invalid return %p", is_valid_pc(sp));
#endif

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
	ksyscall_change_priority((int)req);
	break;
    case SYS_AWAIT:
	ksyscall_await((const kreq_event* const)req);
	break;
    case SYS_SHUTDOWN:
	shutdown();
    case SYS_ABORT:
	ksyscall_abort((const kreq_abort* const)req);
    default:
        assert(false, "Task %d, called invalid system call %d",
	       task_active->tid,
	       code);
    }

    scheduler_get_next();
    //everythings done, just leave
    asm volatile ("mov  r0, %0     \n"
                  "b    kernel_exit\n"
		  :
		  : "r" (task_active->sp)
		  : "r0");
}
