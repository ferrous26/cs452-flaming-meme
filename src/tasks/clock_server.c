#include <tasks/clock_server.h>
#include <tasks/clock_notifier.h>
#include <syscall.h>
#include <std.h>
#include <debug.h>
#include <scheduler.h>
#include <vt100.h>


#define CLOCK_HOME 7
#define LEFT(i)   (i << 1)
#define RIGHT(i)  (LEFT(i) + 1)
#define PARENT(i) (i >> 1)


typedef struct {
    uint time;
    task_id tid;
} clock_delay;

typedef struct {
    int count;
    clock_delay delays[TASK_MAX];
} clock_pq;

static void pq_init(clock_pq* q) {
    q->count = 1;
    q->delays[0].time = 0;
    q->delays[0].tid  = 0;
    for (size i = 1; i < TASK_MAX; i++) {
	q->delays[i].time = UINT_MAX;
	q->delays[i].tid  = 0;
    }
}

static inline uint pq_peek(clock_pq* q) {
    return q->delays[1].time;
}

static task_id pq_delete(clock_pq* q) {

    task_id tid = q->delays[1].tid;

    clock_delay* delays = q->delays;
    int curr = --q->count;

    // take butt and put it on head
    delays[1].time    = delays[curr].time;
    delays[1].tid     = delays[curr].tid;
    delays[curr].time = UINT_MAX; // mark the end

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
	uint stime = delays[smallest].time;
	int   stid = delays[smallest].tid;
	delays[smallest].time = delays[curr].time;
	delays[smallest].tid  = delays[curr].tid;
	delays[curr].time     = stime;
	delays[curr].tid      = stid;
	curr = smallest;
    }

    return tid;
}

static void pq_add(clock_pq* q, uint time, task_id tid) {

    clock_delay* delays = q->delays;
    int curr   = q->count++;
    int parent = PARENT(curr);

    delays[curr].time = time;
    delays[curr].tid  = tid;

    FOREVER {
	// is it smaller than it's parent?
	if (delays[curr].time < delays[parent].time) {
	    delays[curr].time   = delays[parent].time;
	    delays[curr].tid    = delays[parent].tid;
	    delays[parent].time = time;
	    delays[parent].tid  = tid;
	    curr                = parent;
	    parent              = PARENT(curr);
	}
	else {
	    break;
	}
    }
}

#if DEBUG
static void pq_debug(clock_pq* q) {

    debug_log("Queue size is %u", q->count);

    clock_delay* delays = q->delays;

    for (size i = 1; i < TASK_MAX; i++)
	debug_log("q->delays[%u] = %u - %u", i, delays[i].time, delays[i].tid);
}
#else
#define pq_debug(q)
#endif

static void _error(int tid, int code) {
    vt_log("Failed to reply to %u (%d)", tid, code);
    vt_flush();
}


void clock_server() {

    assert(myPriority() < TASK_PRIORITY_MAX,
	   "Clock server should not be top priority");

    int result = RegisterAs("clock");
    if (result) {
	vt_log("Failed to register clock server name (%d)", result);
	vt_flush();
	return;
    }

    result = Create(TASK_PRIORITY_MAX, clock_notifier);
    if (result < 0) {
	vt_log("Failed to create clock_notifier (%d)", result);
	vt_flush();
	return;
    }

    vt_goto_home();
    kprintf_string("CLOCK ");

    uint time = 0;
    clock_req req;
    clock_pq  q;
    pq_init(&q);

    FOREVER {
	int  tid;
	size siz = Receive(&tid, (char*)&req, sizeof(req));
	switch (req.type) {

	case CLOCK_NOTIFY:
	    time++;

	    // reset notifier ASAP
	    result = Reply(tid, (char*)&req, siz);
	    if (result) {
		vt_log("Failed to reply to clock_notifier (%d)", result);
		vt_flush();
	    }

	    // print the clock to the screen
	    vt_goto(1, CLOCK_HOME);
	    kprintf_uint(time);
	    vt_flush(); // hmmmm

	    while (pq_peek(&q) <= time) {
		tid = pq_delete(&q);
		result = Reply(tid, (char*)&req, sizeof(clock_req_type));
		if (result < 0) _error(tid, result);
	    }
	    break;

	case CLOCK_DELAY:
	    // TODO: check against negative values
	    // TODO: if we missed the deadline, wake task up right away?
	    pq_add(&q, req.ticks + time, tid);
	    break;

	case CLOCK_TIME:
	    result = Reply(tid, (char*)&time, sizeof(time));
	    if (!result) _error(tid, result);
	    break;

	case CLOCK_DELAY_UNTIL:
	    // TODO: check against negative values
	    // TODO: if we missed the deadline, wake task up right away?
	    pq_add(&q, req.ticks, tid);
	    pq_debug(&q);
	    break;

	default:
	    assert(false, "Invalid clock request %d from %d", req.type, tid);
	}
    }
}
