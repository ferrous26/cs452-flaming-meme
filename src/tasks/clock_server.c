#include <tasks/clock_server.h>
#include <tasks/clock_notifier.h>
#include <syscall.h>
#include <std.h>
#include <debug.h>

int clock_server_tid;


typedef struct delay_element {
    int time;
    task_id tid;
    struct delay_element* prev;
    struct delay_element* next;
} delay;

typedef struct {
    delay* head;
    delay* tail;
    uint   count;
    // -4 because clock stuff can't delay, idle can't delay, etc.
    delay delays[TASK_MAX - 4];
} delay_list;

static void delay_init(delay_list* l) {
    memset(l, 0, sizeof(delay_list));

    // keep them at INT_MAX so we don't need a special case for checking
    for (size i = 0; i < (TASK_MAX - 4); i++)
	l->delays[i].time = INT_MAX;

    l->head = l->tail = l->delays;
}

static inline int delay_peek(delay_list* l) {
    return l->head->time;
}

static inline task_id delay_pop(delay_list* l) {

    task_id tid = l->head->tid;

    l->head->time       = INT_MAX;
    l->head             = l->head->next;
    l->head->prev->next = NULL;
    l->head->prev       = NULL;
    l->count--;

    return tid;
}

static void delay_push(delay_list* const l, const int time, const task_id tid) {

    delay* new_delay = &l->delays[++l->count];
    new_delay->time   = time;
    new_delay->tid    = tid;

    if (l->count == 1) {
	l->head = l->tail = new_delay;
	return;
    }

    // travel from tail to head
    delay* tail = l->tail;
    delay* head = l->head;

    do { // there must be at least one iteration

	if (tail->time > new_delay->time) {
	    tail = tail->prev;
	}
	else { // we've found the place to insert new_delay

	    // tail->next might be NULL (i.e. the end of the list)
	    if (tail->next == NULL)
		l->tail = new_delay;
	    else
		tail->next->prev = new_delay;

	    new_delay->next  = tail->next;
	    tail->next       = new_delay;
	    new_delay->prev  = tail;

	    return;
	}
    } while (tail != head);

    // if we got here, that means we are inserting at the front of the list
    // so we need to update the head pointer as well
    new_delay->next  = l->head;
    l->head->prev    = new_delay;
    l->head          = new_delay;
}

static void _error(int tid, int code) {
    vt_log("Failed to reply to %u (%d)", tid, code);
    vt_flush();
}

static void _startup(delay_list* l) {

    // for the sake of safety, we should set this up before allowing any
    // requests to show up
    delay_init(l);

    // allow tasks to send messages to the clock server
    clock_server_tid = myTid();
    int result = RegisterAs((char*)"clock");
    if (result) {
	vt_log("Failed to register clock server name (%d)", result);
	vt_flush();
	return;
    }

    result = Create(TASK_PRIORITY_HIGH, clock_notifier);
    if (result < 0) {
	vt_log("Failed to create clock_notifier (%d)", result);
	vt_flush();
	return;
    }

    vt_log("Clock Server started at %d", clock_server_tid);
    vt_flush();
}

void clock_server() {

    int        time = 0;
    int        result; // used by Reply() calls
    clock_req  req; // store incoming requests
    delay_list l;   // priority queue for tasks waiting for time

    _startup(&l);

    FOREVER {
	int tid;
	int siz = Receive(&tid, (char*)&req, sizeof(req));

	assert(req.type >= CLOCK_NOTIFY && req.type <= CLOCK_DELAY_UNTIL,
	       "CS: Invalid message type from %u (%d) size = %u",
	       tid, req.type, siz);

	switch (req.type) {
	case CLOCK_NOTIFY:
	    // reset notifier ASAP
	    result = Reply(tid, (char*)&req, siz);
	    if (result) {
		vt_log("Failed to reply to clock_notifier (%d)", result);
		vt_flush();
	    }

	    time++; // tick-tock

	    while (delay_peek(&l) <= time) {
		tid = delay_pop(&l);
		result = Reply(tid, NULL, 0);
		if (result) _error(tid, result);
	    }
	    break;

	case CLOCK_DELAY:
	    // value is checked on the calling task
	    delay_push(&l, req.ticks + time, tid);
	    break;

	case CLOCK_TIME:
	    result = Reply(tid, (char*)&time, sizeof(time));
	    if (result) _error(tid, result);
	    break;

	case CLOCK_DELAY_UNTIL:
	    // if we missed the deadline, wake task up right away
	    if (req.ticks <= time) {
		result = Reply(tid, NULL, 0);
		if (result) _error(tid, result);
	    }
	    else {
		delay_push(&l, req.ticks, tid);
	    }
	    break;
	}
    }
}
