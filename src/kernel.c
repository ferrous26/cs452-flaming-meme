#include <kernel.h>
#include <std.h>
#include <syscall.h>
#include <ts7200.h>
#include <irq.h>
#include <io.h>
#include <debug.h>
#include <char_buffer.h>

#include <tasks/idle.h>
#include <tasks/name_server.h>
#include <tasks/term_server.h>
#include <tasks/clock_server.h>
#include <tasks/train_server.h>
#include <tasks/task_launcher.h>
#include <tasks/mission_control.h>
#include <tasks/train_station.h>


#define SWI_HANDLER ((volatile uint*)0x08)

#define SAVED_REGISTERS 13
#define TRAP_FRAME_SIZE (SAVED_REGISTERS * WORD_SIZE)

#define START_ADDRESS(fn) ((int)fn)
#define EXIT_ADDRESS      START_ADDRESS(Exit)
#define DEFAULT_SPSR      0x50  //no fiq

#define TASK_HEAP_TOP 0x1F00000 // 31 MB
#define TASK_HEAP_BOT 0x0300000 //  3 MB
#define TASK_HEAP_SIZ 0x40000   // 64 pages * 4096 bytes per page

#ifdef DEBUG
extern const int _TextStart;
extern const int _TextEnd;
#endif

typedef struct task_q_pointers {
    task* head;
    task* tail;
} task_q;



CHAR_BUFFER(TASK_MAX)
static char_buffer free_list;

static struct task_manager {
    uint32 state; // bitmap for accelerating queue selection
    task_q q[TASK_PRIORITY_LEVELS];
} manager DATA_HOT;


task*  task_active DATA_HOT;
task*  int_queue[EVENT_COUNT] DATA_HOT;

static task   tasks[TASK_MAX] DATA_HOT;
static task_q recv_q[TASK_MAX] DATA_HOT;
static uint8  table[256] DATA_HOT;



static inline uint __attribute__ ((const)) task_index_from_tid(const task_id tid) {
    return mod2((uint32)tid, TASK_MAX);
}

// This algorithm is borrowed from
// http://graphics.stanford.edu/%7Eseander/bithacks.html#IntegerLogLookup
static inline uint32 __attribute__ ((pure)) choose_priority(const uint32 v) {
    uint32 result;
    uint  t;
    uint  tt;

    if ((tt = v >> 16))
        result = (t = tt >> 8) ? 24 + table[t] : 16 + table[tt];
    else
        result = (t = v >> 8) ? 8 + table[t] : table[v];

    return result;
}

static inline int* __attribute__ ((const)) task_stack(const task_idx idx) {
    return (int*)(TASK_HEAP_TOP - (TASK_HEAP_SIZ * idx));
}

/**
 * @return tid of new task, or an error code as defined by CreateTask()
 */
inline static int  task_create(const task_pri pri,
                               void (*const start)(void));
inline static void task_destroy(void);
inline static void scheduler_schedule(task* const t);
inline static void scheduler_get_next(void);

static void _print_train() {

    // ASCII art borrowed from http://www.ascii-art.de/ascii/t/train.txt
    char buffer[1024];
    char* ptr = log_start(buffer);
    ptr = sprintf_string(ptr,
			 "\n"
"               .---._\n"
"           .--(. '  .).--.      . .-.\n"
"        . ( ' _) .)` (   .)-. ( ) '-'\n"
"       ( ,  ).        `(' . _)\n"
"     (')  _________      '-'\n"
"     ____[_________]                                         ________\n"
"     \\__/ | _ \\  ||    ,;,;,,                               [________]\n"
"     _][__|(\")/__||  ,;;;;;;;;,   __________   __________   _| LILI |_\n"
"    /             | |____      | |          | |  ___     | |      ____|\n"
"   (| .--.    .--.| |     ___  | |   |  |   | |      ____| |____      |\n"
"   /|/ .. \\~~/ .. \\_|_.-.__.-._|_|_.-:__:-._|_|_.-.__.-._|_|_.-.__.-._|\n"
"+=/_|\\ '' /~~\\ '' /=+( o )( o )+==( o )( o )=+=( o )( o )+==( o )( o )=+=\n"
"='=='='--'==+='--'===+'-'=='-'==+=='-'+='-'===+='-'=='-'==+=='-'=+'-'+===");
    ptr = log_end(ptr);
    uart2_bw_write(buffer, ptr - buffer);
}

void kernel_init() {
    int i;

    // initialize the lookup table for lg()
    table[0] = table[1] = 0;
    for (i = 2; i < 256; i++)
        table[i] = 1 + table[i >> 1];
    table[0] = 32; // if you want log(0) to return -1, change to -1

    memset(&manager,  0, sizeof(manager));
    memset(&recv_q,   0, sizeof(recv_q));
    memset(&tasks,    0, sizeof(tasks));
    memset(int_queue, 0, sizeof(int_queue));

    cbuf_init(&free_list);

    for (i = 0; i < 16; i++) {
        tasks[i].tid   = i;
        tasks[i].p_tid = -1;
        cbuf_produce(&free_list, (char)i);
    }

    // get the party started
    task_active = &tasks[0];
    task_create(TASK_PRIORITY_IDLE + 1,    task_launcher);
    task_create(TASK_PRIORITY_IDLE,        idle);

    // avoid potential race conditions...
    clock_server_tid =
        task_create(TASK_PRIORITY_MEDIUM_HIGH, clock_server);
    name_server_tid =
        task_create(TASK_PRIORITY_MEDIUM_HIGH, name_server);

    task_create(TASK_PRIORITY_MEDIUM,      term_server);
    task_create(TASK_PRIORITY_MEDIUM - 1,  mission_control);
    task_create(TASK_PRIORITY_MEDIUM,      train_server);

    for (; i < TASK_MAX; i++) {
        tasks[i].tid   = i;
        tasks[i].p_tid = -1;
        cbuf_produce(&free_list, (char)i);
    }

    *SWI_HANDLER = (0xea000000 | (((uint)kernel_enter >> 2) - 4));

    _print_train();
    klog("Welcome to ferOS build %u", __BUILD__);
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
    *result = task_active->priority;
    ksyscall_pass();
}

inline static void ksyscall_recv(task* const receiver) {
#ifdef DEBUG
    uint sp;
    asm volatile ("mov %0, sp" : "=r" (sp));
    assert(sp < 0x300000 && sp > 0x200000, "Recv: Smashed the stack");
#endif

    task_q* const q = &recv_q[task_index_from_tid(receiver->tid)];

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
            sender->sp[0] = NOT_ENUF_MEMORY;
            scheduler_reschedule(sender);
            ksyscall_recv(receiver); // DANGER: recursive call
            return;
        }

        // all is good, copy the message over and get back to work!
        *receiver_req->tid = sender->tid;
        receiver->sp[0]    = sender_req->msglen;
        sender->next       = RPLY_BLOCKED;

        if (sender_req->msglen)
            memcpy(receiver_req->msg,
                   sender_req->msg,
                   (uint)sender_req->msglen);
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
        memcpy(sender_req->reply, req->reply, (uint)req->replylen);

    // according to spec, we should schedule the sender first, because, in
    // the case where they are the same priority level we want the sender
    // to run first
    scheduler_schedule(sender);
    scheduler_schedule(task_active);
}

inline static void ksyscall_change_priority(const int32 priority,
                                            int* const result) {
    *result = task_active->priority = priority;
    ksyscall_pass();
}

static inline void ksyscall_await(const kreq_event* const req) {

#ifdef DEBUG
    assert(!int_queue[req->eventid],
           "Event Task Collision (%d - %d) has happened on event %d",
           ((task*)int_queue[req->eventid])->tid,
	   task_active->tid,
	   req->eventid);

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
    }
}

#ifdef DEBUG
static inline bool __attribute__ ((pure)) is_valid_pc(const int* const sp) {
    int pc = sp[2] == 0 ? sp[17] : sp[2];

    if (pc >= (int)&_TextStart && pc <= (int)&_TextEnd) {
        return 0;
    }

    return pc;
}
#endif

void syscall_handle(const uint code, const void* const req, int* const sp) {
#ifdef DEBUG
    assert((uint)sp > TASK_HEAP_BOT && (uint)sp <= TASK_HEAP_TOP,
	   "Reply: task %d has Invalid heap %p", task_active->tid, sp);
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
        ksyscall_change_priority((int)req, sp);
        break;
    case SYS_AWAIT:
        ksyscall_await((const kreq_event* const)req);
        break;
    case SYS_SHUTDOWN:
        shutdown();
    case SYS_ABORT:
    case SYS_COUNT:
        abort((const kreq_abort* const)req);
    }

    scheduler_get_next();
    //everythings done, just leave
    asm volatile ("mov  r0, %0     \n"
                  "b    kernel_exit\n"
                  :
                  : "r" (task_active->sp)
                  : "r0");
}

void scheduler_first_run() {
    scheduler_get_next();
    kernel_exit(task_active->sp);
}

void scheduler_reschedule(task* const t) {
    scheduler_schedule(t);
}

inline static void scheduler_schedule(task* const t) {

    task_q* const q = &manager.q[t->priority];

    assert(t >= tasks && t < &tasks[TASK_MAX],
           "schedule: cannot schedule task at %p (%p-%p)",
           t, tasks, &tasks[TASK_MAX]);

    // _always_ turn on the bit in the manager state
    manager.state |= (1 << t->priority);

    // if there was something in the queue, append to it
    if (q->head)
      q->tail->next = t;
    // else the queue was off, so set the head
    else
      q->head = t;

    // then we set the tail pointer
    q->tail = t;

    // mark the end of the queue
    t->next = NULL;
}

// scheduler_consume
inline static void scheduler_get_next() {
    // find the msb and add it
    uint32 priority = choose_priority(manager.state);
    assert(priority != 32, "Ran out of tasks to run");

    task_q* const q = &manager.q[priority];

    task_active = q->head;
    q->head = q->head->next;

    // if we hit the end of the list, then turn off the queue
    if (!q->head)
      manager.state ^= (1 << priority);
}

inline static int task_create(const task_pri pri,
                              void (*const start)(void)) {

    // double check that priority was checked in user land
    assert(pri <= TASK_PRIORITY_MAX, "Invalid priority %u", pri);

    // make sure we have something to allocate
    if (!cbuf_count(&free_list)) return NO_DESCRIPTORS;

    // actually take the task descriptor and load it up
    const task_idx task_index = cbuf_consume(&free_list);

    task* tsk      = &tasks[task_index];
    tsk->p_tid     = task_active->tid; // task_active is _always_ the parent
    tsk->priority  = pri;

    // setup the default trap frame for the new task
    tsk->sp        = task_stack(task_index) - TRAP_FRAME_SIZE;
    tsk->sp[2]     = START_ADDRESS(start);
    tsk->sp[3]     = DEFAULT_SPSR;
    tsk->sp[12]    = EXIT_ADDRESS; // set link register to auto-call Exit()

    // set tsk->next
    scheduler_schedule(tsk);

    return tsk->tid;
}

inline static void task_destroy() {
    // TODO: handle overflow (trololol)
    task_active->tid += TASK_MAX;
    task_active->sp   = NULL;

    // empty the receive buffer
    task_q* q = &recv_q[task_index_from_tid(task_active->tid)];
    while (q->head) {
        q->head->sp[0] = INCOMPLETE;
        scheduler_reschedule(q->head);
        q->head = q->head->next;
    }

    // put the task back into the allocation pool
    cbuf_produce(&free_list, (char)task_index_from_tid(task_active->tid));
}



/** ABORT UI CODE **/

static char* _abort_pad(char* ptr, const int val) {
    int count = 12 - val;
    if (val == 0) count = 11;

    for (int i = 0; i < count; i++)
        ptr = sprintf_char(ptr, ' ');
    return ptr;
}

static char* _abort_tid_num(char* ptr, int tid) {
    char* name = kWhoTid(tid);
    if (name) {
        char* new_ptr = sprintf_string(ptr, name);
        return _abort_pad(new_ptr, new_ptr - ptr);
    }

    ptr = sprintf_int(ptr, tid);
    return _abort_pad(ptr, log10(tid));
}

static char* _abort_tid(char* ptr, task* const t) {
    if (!t)
        return sprintf_string(ptr, "-           ");
    return _abort_tid_num(ptr, t->tid);
}

static char* _abort_ptid(char* ptr, task* const t) {
    if (t->p_tid == -1)
        return sprintf_string(ptr, "-           ");
    return _abort_tid_num(ptr, t->p_tid);
}

static char* _abort_priority(char* ptr, task* const t) {
    if (!t) return ptr;
    ptr = sprintf(ptr, "%d", t->priority);
    return _abort_pad(ptr, log10(t->priority));
}

static char* _abort_next(char* ptr, task* const t) {
    if (t->next == RECV_BLOCKED)
        return sprintf_string(ptr, "RECV        ");

    if (t->next == RPLY_BLOCKED)
        return sprintf_string(ptr, "RPLY        ");

    return _abort_tid(ptr, t->next);
}

static char* _abort_sp(char* ptr, task* const t) {
    if (t->sp)
        return sprintf(ptr, "%p  ", t->sp);
    return sprintf_string(ptr, "-           ");
}

static char* _abort_pc(char* ptr, task* const t) {
    if (t->sp)
        return sprintf(ptr, "%p  ", t->sp[2] ? t->sp[2] : t->sp[17]);
    return sprintf_string(ptr, "-           ");
}

static char* _abort_receiver(char* ptr, task* const t) {
    task_q* const q = &recv_q[task_index_from_tid(t->tid)];
    return _abort_tid(ptr, q->head);
}

static char* _abort_send(char* ptr, task* const t) {
    if (t->next != RPLY_BLOCKED)
        return sprintf_string(ptr, "-           ");

    const kreq_send* const req = (const kreq_send* const)t->sp[1];
    return _abort_tid(ptr, &tasks[task_index_from_tid(req->tid)]);
}

void abort(const kreq_abort* const req) {

    // we want a big buffer, we might end up printing a screenful of text
    // plus various escape sequences to get everything formatted nicely
    char buffer[4096 * 4];
    char* ptr = buffer;

    ptr = vt_colour_reset(ptr);
    ptr = vt_reset_scroll_region(ptr);
    ptr = vt_goto(ptr, 80, 1);
    ptr = vt_restore_cursor(ptr);
    ptr = sprintf(ptr, "assertion failure at %s:%u\n", req->file, req->line);
    ptr = sprintf_va(ptr, req->msg, *req->args);

#define ITID(n) _abort_tid(ptr, int_queue[n])
    ptr = sprintf_string(ptr, "\n       Active Task: ");
    ptr = _abort_tid(ptr, task_active);
    ptr = sprintf_string(ptr, "\n        Clock Task: ");
    ptr = ITID(0);
    ptr = sprintf_string(ptr, "\nUART2    Send Task: ");
    ptr = ITID(1);
    ptr = sprintf_string(ptr,   "UART2 Receive Task: ");
    ptr = ITID(2);
    ptr = sprintf_string(ptr, "\nUART1    Send Task: ");
    ptr = ITID(3);
    ptr = sprintf_string(ptr,   "UART1 Receive Task: ");
    ptr = ITID(4);
    ptr = sprintf_string(ptr, "\nUART1     CTS Task: ");
    ptr = ITID(5);
    ptr = sprintf_string(ptr,   "UART1    Down Task: ");
    ptr = ITID(6);

    // Table header
    ptr = sprintf_string(ptr,
                         "\n"
                         "TID         "
                         "PTID        "
                         "Priority    "
                         "Next        "
                         "Stack       "
                         "PC          "
                         "Receiver    "
                         "Send      \n");
    for (int i = 0; i < 96; i++)
        ptr = sprintf_char(ptr, '#');
    ptr = sprintf_char(ptr, '\n');

    for (int i = 0; i < TASK_MAX; i++) {
        task* t = &tasks[i];

        // skip descriptors that have never been allocated
        if (t->p_tid == -1) continue;

        ptr = _abort_tid(ptr, t);
        ptr = _abort_ptid(ptr, t);
        ptr = _abort_priority(ptr, t);
        ptr = _abort_next(ptr, t);
        ptr = _abort_sp(ptr, t);
              ptr = _abort_pc(ptr, t);
        ptr = _abort_receiver(ptr, t);
        ptr = _abort_send(ptr, t);
        ptr = sprintf_string(ptr, "\n");
    }

    uart2_bw_write(buffer, ptr - buffer);
    shutdown();
}
