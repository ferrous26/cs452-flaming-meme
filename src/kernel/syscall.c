#include <kernel.h>
#include <syscall.h>
#include <kernel/arch/arm920/ts7200.h>
#include <kernel/arch/arm920/irq.h>
#include <kernel/arch/arm920/io.h>

#include <tasks/priority.h>
#include <tasks/idle.h>
#include <tasks/name_server.h>
#include <tasks/term_server.h>
#include <tasks/clock_server.h>
#include <tasks/train_server.h>
#include <tasks/task_launcher.h>

#define STRINGIFY(name) #name
#define TO_STRING(name) STRINGIFY(name)

#define SWI_HANDLER ((volatile uint*)0x08)

extern const int _text_start;
#ifdef DEBUG
extern const int _text_end;
#endif

task* int_queue[EVENT_COUNT] DATA_HOT;


static void _print_train() {

    // ASCII art borrowed from http://www.ascii-art.de/ascii/t/train.txt
    char buffer[1024];
    char* ptr = klog_start(buffer);
    ptr = sprintf_string(ptr,
			 "\n"
"               .---._\n"
"           .--(. '  .).--.      . .-.\n"
"        . ( ' _) .)` (   .)-. ( ) '-'\n"
"       ( ,  ).        `(' . _)\n"
"     (')  _________      '-'\n"
"     ____[_________]                                         ________\n"
"     \\__/ | _ \\  ||    ,;,;,,                               [________]\n"
"     _][__|(\")/__||  ,;;;;;;;;,   __________   __________   _| SHAQ |_\n"
"    /             | |____      | |          | |  ___     | |      ____|\n"
"   (| .--.    .--.| |     ___  | |   |  |   | |      ____| |____      |\n"
"   /|/ .. \\~~/ .. \\_|_.-.__.-._|_|_.-:__:-._|_|_.-.__.-._|_|_.-.__.-._|\n"
"+=/_|\\ '' /~~\\ '' /=+( o )( o )+==( o )( o )=+=( o )( o )+==( o )( o )=+=\n"
"='=='='--'==+='--'===+'-'=='-'==+=='-'+='-'===+='-'=='-'==+=='-'=+'-'+===");
    ptr = klog_end(ptr);
    uart2_bw_write(buffer, ptr - buffer);
}

void kernel_init() {
    int i;

    // initialize the lookup table for lg()
    table[0] = table[1] = 0;
    for (i = 2; i < 256; i++)
        table[i] = 1 + table[i >> 1];
    table[0] = 32; // if you want log(0) to return -1, change to -1

    memset(&manager,   0, sizeof(manager));
    memset(&tasks,     0, sizeof(tasks));
    memset(&free_list, 0, sizeof(free_list));
    memset(int_queue,  0, sizeof(int_queue));


    for (i = 0; i < TASK_MAX; i++) {
        tasks[i].tid   = i;
        tasks[i].p_tid = -1;
        task_free_list_produce(&tasks[i]);
    }

    // get the party started
    task_active = &tasks[0];
    task_create(TASK_LAUNCHER_PRIORITY, task_launcher);
    task_create(TASK_PRIORITY_IDLE,     idle);

    // avoid potential race conditions...
    clock_server_tid =
        task_create(CLOCK_SERVER_PRIORITY, clock_server);
    name_server_tid =
        task_create(NAME_SERVER_PRIORITY, name_server);

    task_create(TERM_SERVER_PRIORITY,  term_server);
    task_create(TRAIN_SERVER_PRIORITY, train_server);

    *SWI_HANDLER = (0xea000000 | (((uint)swi_enter >> 2) - 4));

    _print_train();
    klog("Welcome to %sOS build %u", TO_STRING(__CODE_NAME__), __BUILD__);
    klog("Built %s %s", __DATE__, __TIME__);
}

void kernel_deinit() {
    *SWI_HANDLER = 0xE59FF018;
}

inline static void ksyscall_pass() {
    scheduler_reschedule(task_active);
}

inline static void ksyscall_irq() {
    voidf handler = (voidf)VIC_PTR(VIC1_BASE, VIC_VECTOR_ADDRESS);
    handler();
    VIC_PTR(VIC1_BASE, VIC_VECTOR_ADDRESS) = (void*)handler;
    scheduler_schedule(task_active);
}

inline static void ksyscall_create(const kreq_create* const req,
				   int* const result) {
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
    *result = (int)task_active->priority;
    ksyscall_pass();
}

inline static void ksyscall_recv(task* const receiver) {
#ifdef DEBUG
    uint sp;
    asm volatile ("mov %0, sp" : "=r" (sp));
    assert(sp < 0x300000 && sp > 0x200000, "Recv: Smashed the stack");
#endif

    task_q* const q = &receiver->recv_q;

    // only if we actually have a message to get
    if (q->head) {

        task* const sender = q->head;

        assert((uint)sender < 0x200000 && (uint)sender > 0x100000,
               "Receive: %d Invalid head pointer!", receiver->tid);

        assert((uint)q->head->next < 0x200000 &&
               ((uint)q->head->next > 0x100000 ||
                (uint)q->head->next == NULL),
               "Receive: %d Invalid head next! %p",
               receiver->tid, sender->next);

        q->head = q->head->next; // consume

        kreq_send* const   sender_req = (kreq_send* const)sender->sp[1];
        kreq_recv* const receiver_req = (kreq_recv* const)receiver->sp[1];

        // validate that there is enough space in the receiver buffer
        if (sender_req->msglen > receiver_req->msglen) {
            sender->sp[0] = ERR_NOT_ENUF_MEMORY;
            scheduler_reschedule(sender);
            ksyscall_recv(receiver); // DANGER: recursive call
            return;
        }

        // all is good, copy the message over and get back to work!
        *receiver_req->tid = sender->tid;
        receiver->sp[0]    = (int)sender_req->msglen;
        sender->next       = RPLY_BLOCKED;

        if (sender_req->msglen)
            memcpy(receiver_req->msg, sender_req->msg, sender_req->msglen);
        scheduler_schedule(receiver); // schedule this mofo
        return;
    }

    receiver->next = RECV_BLOCKED;
}

inline static void ksyscall_send(const kreq_send* const req,
                                 int* const result) {

    task* const receiver = &tasks[task_index_from_tid(req->tid)];

    // validate the request arguments
    if (receiver->tid != req->tid || !receiver->sp) {
        *result = req->tid < 0 ? ERR_IMPOSSIBLE_TASK : ERR_INVALID_TASK;
        ksyscall_pass();
        return;
    }

    assert(task_active->tid != req->tid,
           "Tried to Send to self! (%u)", task_active->tid);

    /**
     * What we want to do is find the receive q and append to it, as we
     * did in the scheduler.
     */
    task_q* const q = &receiver->recv_q;

    if (!q->head)
        q->head = task_active;
    else
        q->tail->next = task_active;

    q->tail = task_active;
    task_active->next = NULL;

    /**
     * Now, if the receiving task is receive blocked, then we need to wake it
     * up and schedule it so that it can receive the message.
     */
    if (receiver->next == RECV_BLOCKED)
        ksyscall_recv(receiver);
}

inline static void ksyscall_reply(const kreq_reply* const req,
                                  int* const result) {

    task* const sender = &tasks[task_index_from_tid(req->tid)];

    // first, validation of the request arguments
    if (sender->tid != req->tid || !sender->sp) {
        *result = req->tid < 0 ? ERR_IMPOSSIBLE_TASK : ERR_INVALID_TASK;
        ksyscall_pass();
        return;
    }

    if (sender->next != RPLY_BLOCKED) {
        *result = ERR_INVALID_RECVER;
        ksyscall_pass();
        return;
    }

    const kreq_send* const sender_req = (const kreq_send* const)sender->sp[1];

    if (req->replylen > sender_req->replylen) {
        *result = ERR_NOT_ENUF_MEMORY;
        ksyscall_pass();
        return;
    }

    task_active->sp[0] = 0; // success!
    sender->sp[0]      = (int)req->replylen;

    // at this point, it is smooth sailing
    if (req->replylen)
        memcpy(sender_req->reply, req->reply, req->replylen);

    // according to spec, we should schedule the sender first, because, in
    // the case where they are the same priority level we want the sender
    // to run first
    scheduler_schedule(sender);
    scheduler_schedule(task_active);
}

inline static void ksyscall_change_priority(const task_priority priority,
                                            int* const result) {
    task_active->priority = priority;
    *result = (int)priority;
    ksyscall_pass();
}

static inline void ksyscall_await(const kreq_event* const req) {

#ifdef DEBUG
    assert(!int_queue[req->eid],
           "Event Task Collision (%d - %d) has happened on event %d",
           ((task*)int_queue[req->eid])->tid,
	   task_active->tid,
	   req->eid);

    assert(req->eid < EVENT_COUNT, "Invalid event (%d)", req->eid);
#endif

    switch (req->eid) {
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
    }
}

#ifdef DEBUG
static inline bool __attribute__ ((pure)) is_valid_pc(const int* const sp) {
    int pc = sp[2] == 0 ? sp[17] : sp[2];

    if (pc >= (int)&_text_start && pc <= (int)&_text_end) {
        return 0;
    }

    return pc;
}
#endif

void syscall_handle(const syscall_num code,
                    const void* const req,
                    int* const sp) {

#ifdef DEBUG
    assert((uint)sp > TASK_HEAP_BOT && (uint)sp <= TASK_HEAP_TOP,
	   "Reply: task %d has invalid heap %p", task_active->tid, sp);

    assert(!is_valid_pc(sp), "Task %d has invalid return %p for call %d",
           task_active->tid, is_valid_pc(sp), code);
#endif

    assert(code < SYS_COUNT,
	   "Invalid syscall (%d) by %d", code, task_active->tid);

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
        ksyscall_change_priority((const task_priority)req, sp);
        break;
    case SYS_AWAIT:
        ksyscall_await((const kreq_event* const)req);
        break;
    case SYS_SHUTDOWN:
        shutdown();
    case SYS_ABORT:
        abort((const kreq_abort* const)req);
    case SYS_COUNT:
        ABORT("You done goofed, son.");
    }

    scheduler_get_next();
    kernel_exit();
}
