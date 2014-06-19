#include <std.h>
#include <vt100.h>
#include <scheduler.h>
#include <tasks/name_server.h>

static int log10(int c) {
    int count = 0;
    while (c) {
	count++;
	c /= 10;
    }
    return count;
}

static char* _abort_pad(char* ptr, const int val) {
    int count = 12 - val;
    if (val == 0) count = 11;

    for (int i = 0; i < count; i++)
	ptr = sprintf_char(ptr, ' ');
    return ptr;
}

static char* _abort_tid(char* ptr, task* const t) {
    if (!t)
	return sprintf_string(ptr, "-           ");

#ifdef ASSERT
    char* name = kWhoTid(t->tid);
    if (name) {
	char* new_ptr = sprintf_string(ptr, name);
	return _abort_pad(new_ptr, (int)(new_ptr - ptr));
    }
    else {
#endif
	ptr = sprintf_int(ptr, t->tid);
	return _abort_pad(ptr, log10(t->tid));
#ifdef ASSERT
    }
#endif
}

static char* _abort_ptid(char* ptr, task* const t) {
    if (!t) return ptr;

    if (t->p_tid == -1)
	return sprintf_string(ptr, "-           ");

    return _abort_tid(ptr, &tasks[task_index_from_tid(t->p_tid)]);
}

static char* _abort_priority(char* ptr, task* const t) {
    if (!t) return ptr;
    ptr = sprintf(ptr, "%d", t->priority);
    return _abort_pad(ptr, log10(t->priority));
}

static char* _abort_next(char* ptr, task* const t) {
    if (!t) return ptr;

    if (t->next == RECV_BLOCKED)
	return sprintf_string(ptr, "RECV        ");

    if (t->next == RPLY_BLOCKED)
	return sprintf_string(ptr, "RPLY        ");

    return _abort_tid(ptr, t->next);
}

static char* _abort_sp(char* ptr, task* const t) {
    if (!t) return ptr;

    if (t->sp)
	return sprintf(ptr, "%p  ", t->sp);
    return sprintf_string(ptr, "-           ");
}

static char* _abort_receiver(char* ptr, task* const t) {
    if (!t) return ptr;

    task_q* const q = &recv_q[task_index_from_tid(t->tid)];
    return _abort_tid(ptr, q->head);
}

static char* _abort_send(char* ptr, task* const t) {
    if (!t) return ptr;

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
			 "Receiver    "
			 "Send\n");
    for (int i = 0; i < 84; i++)
	ptr = sprintf_char(ptr, '#');
    ptr = sprintf_char(ptr, '\n');

    for (int i = 0; i < TASK_MAX; i++) {
	task* t = &tasks[i];

	ptr = _abort_tid(ptr, t);
	ptr = _abort_ptid(ptr, t);
	ptr = _abort_priority(ptr, t);
	ptr = _abort_next(ptr, t);
	ptr = _abort_sp(ptr, t);
	ptr = _abort_receiver(ptr, t);
	ptr = _abort_send(ptr, t);
	ptr = sprintf_string(ptr, "\n");
    }

    uart2_bw_write(buffer, ptr - buffer);
    shutdown();
}
