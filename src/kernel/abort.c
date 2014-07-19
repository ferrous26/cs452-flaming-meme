#include <kernel.h>

#define COLUMN_WIDTH 12

static char* _abort_tid_num(char* ptr, int tid) {
    char* name = kWhoTid(tid);
    if (name) {
        char* new_ptr = sprintf_string(ptr, name);
        return ui_pad(new_ptr, new_ptr - ptr, COLUMN_WIDTH);
    }

    ptr = sprintf_int(ptr, tid);
    return ui_pad(ptr, log10(tid), COLUMN_WIDTH);
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
    return ui_pad(ptr, log10(t->priority), COLUMN_WIDTH);
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

static char* _abort_sp_butt(char* ptr, task* const t) {
    if (t->sp)
        return sprintf(ptr, "%p  ",
                       task_stack((char)task_index_from_tid(t->tid)));
    return sprintf_string(ptr, "-           ");
}

static char* _abort_receiver(char* ptr, task* const t) {
    task_q* const q = &t->recv_q;
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
    ptr = sprintf_string(ptr, "\n\n       Active Task: ");
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
                         "Stack Head  "
                         "Stack Butt  "
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
        ptr = _abort_sp_butt(ptr, t);
        ptr = _abort_receiver(ptr, t);
        ptr = _abort_send(ptr, t);
        ptr = sprintf_string(ptr, "\n");
    }

    uart2_bw_write(buffer, ptr - buffer);
    shutdown();
}
