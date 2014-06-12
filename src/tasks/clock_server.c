#include <tasks/clock_server.h>
#include <syscall.h>
#include <std.h>
#include <debug.h>

#define LEFT(i)   (i << 1)
#define RIGHT(i)  (LEFT(i) + 1)
#define PARENT(i) (i >> 1)

int clock_server_tid;


typedef struct {
    int time;
    task_id tid;
} clock_delay;

typedef struct {
    int count;
    clock_delay delays[TASK_MAX];
} clock_pq;


static void clock_notifier() {
    int clock = WhoIs((char*)"clock");
    if (clock < 0) {
	vt_log("Clock server not found (%d)", clock);
	vt_flush();
	return;
    }

    int msg = CLOCK_NOTIFY;

    FOREVER {
	int result = AwaitEvent(CLOCK_TICK, NULL, 0);

	UNUSED(result); // it will be unused when asserts are off
	assert(result == 0, "Impossible clock error code (%d)", result);

	// We don't actually need to send anything to the clock
	// server, but sending 0 bytes is going to go down a bad path
	// in memcpy, so send the most optimal case (one word).
	result = Send(clock,
		      (char*)&msg, sizeof(msg),
		      (char*)&msg, sizeof(msg));
	if (result < 0) {
	    vt_log("Failed to send to clock (%d)", result);
	    vt_flush();
	    return;
	}
    }

}

static void pq_init(clock_pq* q) {
    q->count = 1;
    q->delays[0].time = 0;
    q->delays[0].tid  = 0;
    for (size i = 1; i < TASK_MAX; i++) {
	q->delays[i].time = INT_MAX;
	q->delays[i].tid  = 0;
    }
}

static inline int pq_peek(clock_pq* q) {
    return q->delays[1].time;
}

static task_id pq_delete(clock_pq* q) {

    task_id tid = q->delays[1].tid;

    clock_delay* delays = q->delays;
    int curr = --q->count;

    // take butt and put it on head
    delays[1].time    = delays[curr].time;
    delays[1].tid     = delays[curr].tid;
    delays[curr].time = INT_MAX; // mark the end

    // now, we need to bubble the head down
    curr = 1;

    // bubble down until we can bubble no more
    FOREVER {
	int left     = LEFT(curr);
	int right    = RIGHT(curr);
	int smallest = curr;

	if (delays[left].time < delays[curr].time)
	    smallest = left;

	if (delays[right].time < delays[smallest].time)
	    smallest = right;

	// if no swapping needed, then brak
	if (smallest == curr) break;

	// else, swap and prepare for next iteration
	int stime = delays[smallest].time;
	int  stid = delays[smallest].tid;
	delays[smallest].time = delays[curr].time;
	delays[smallest].tid  = delays[curr].tid;
	delays[curr].time     = stime;
	delays[curr].tid      = stid;
	curr = smallest;
    }

    return tid;
}

static void pq_add(clock_pq* q, int time, task_id tid) {

    clock_delay* delays = q->delays;
    int curr   = q->count++;
    int parent = PARENT(curr);

    delays[curr].time = time;
    delays[curr].tid  = tid;

    FOREVER {
	// is it smaller than it's parent?
	if (delays[curr].time >= delays[parent].time) break;

	delays[curr].time   = delays[parent].time;
	delays[curr].tid    = delays[parent].tid;
	delays[parent].time = time;
	delays[parent].tid  = tid;
	curr                = parent;
	parent              = PARENT(curr);
    }
}

#ifdef DEBUG
static void __attribute__ ((unused)) pq_debug(clock_pq* q) {

    debug_log("Queue size is %u", q->count);

    clock_delay* delays = q->delays;

    for (size i = 1; i < TASK_MAX; i++)
	debug_log("q->delays[%u] = %u - %u", i, delays[i].time, delays[i].tid);
}
#endif

static void _error(int tid, int code) {
    vt_log("Failed to reply to %u (%d)", tid, code);
    vt_flush();
}

static void _startup(clock_pq* pq) {
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

    pq_init(pq);

    vt_log("Clock Server started at %d", clock_server_tid);
    vt_flush();
}

void clock_server() {

    int       time = 0;
    int       result; // used by Reply() calls
    clock_req req; // store incoming requests
    clock_pq  q;   // priority queue for tasks waiting for time

    _startup(&q);

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

	    // tick-tock
	    time++;

	    while (pq_peek(&q) <= time) {
		tid = pq_delete(&q);
		result = Reply(tid, (char*)&req, sizeof(clock_req_type));
		if (result) _error(tid, result);
	    }
	    break;

	case CLOCK_DELAY:
	    // value is checked on the calling task
	    pq_add(&q, req.ticks + time, tid);
	    break;

	case CLOCK_TIME:
	    result = Reply(tid, (char*)&time, sizeof(time));
	    if (result) _error(tid, result);
	    break;

	case CLOCK_DELAY_UNTIL:
	    // if we missed the deadline, wake task up right away?
	    if (req.ticks <= time) {
		result = Reply(tid, (char*)&req, sizeof(clock_req_type));
		if (result) _error(tid, result);
	    }
	    else {
		pq_add(&q, req.ticks, tid);
	    }
	    break;

	}
    }
}
