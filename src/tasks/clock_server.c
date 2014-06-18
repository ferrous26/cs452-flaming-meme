
#include <std.h>
#include <debug.h>
#include <vt100.h>
#include <syscall.h>
#include <tasks/clock_server.h>

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


#define CLOCK_ASSERT(expr, result) if (!(expr)) clock_failure(result, __LINE__)
static void __attribute__ ((noreturn)) clock_failure(int result, uint line) {
    log("Action failed in clock at line %u (%d)", line, result);
    Shutdown();
}

static void __attribute__ ((noreturn)) clock_notifier() {
    int clock = myParentTid();
    int   msg = CLOCK_NOTIFY;

    FOREVER {
	int result = AwaitEvent(CLOCK_TICK, NULL, 0);
	CLOCK_ASSERT(result == 0, result);

	// We don't actually need to send anything to the clock
	// server, but sending 0 bytes is going to go down a bad path
	// in memcpy, so send the most optimal case (one word).
	result = Send(clock,
		      (char*)&msg, sizeof(msg),
		      (char*)&msg, sizeof(msg));
	CLOCK_ASSERT(result == 0, result);
    }
}

static void __attribute__ ((noreturn)) clock_ui() {

// CLOCK MM:SS.D
#define CLOCK_ROW     1
#define CLOCK_MINUTES 7
#define CLOCK_SECONDS 10
#define CLOCK_TENTHS  13

    printf(32, "Clock UI starting on %d", myTid());

    char buffer[64];
    char* ptr   = buffer;
    int result  = 0;

    ptr    = vt_goto(buffer, CLOCK_ROW, 1);
    ptr    = sprintf_string(ptr, "CLOCK 00:00.0");
    result = Puts(buffer, ptr - buffer);
    CLOCK_ASSERT(result == 0, result);

    int minutes = 0;
    int seconds = 0;
    int  tenths = 0;

    FOREVER {
	int time = Delay(10);
	CLOCK_ASSERT(time, time);

	tenths++;
	if (tenths == 10) {
	    seconds++;
	    tenths = 0;

	    if (seconds == 60) {
		// sync with the actual time
		tenths  = time    / 10;
		seconds = tenths  / 10;
		minutes = seconds / 60;
		tenths  = tenths  % 10;
		seconds = seconds % 60;
		minutes = minutes % 100;

		ptr    = vt_goto(buffer, CLOCK_ROW, CLOCK_MINUTES);
		ptr    = sprintf(ptr, "%c%c:%c%c.%c",
				 '0' + (minutes / 10),
				 '0' + (minutes % 10),
				 '0' + (seconds / 10),
				 '0' + (seconds % 10),
				 '0' + tenths);
		result = Puts(buffer, ptr - buffer);
		CLOCK_ASSERT(result == 0, result);

	    }
	    else { // need to update seconds and tenths
		ptr    = vt_goto(buffer, CLOCK_ROW, CLOCK_SECONDS);
		ptr    = sprintf(ptr, "%c%c.0",
				 '0' + (seconds / 10),
				 '0' + (seconds % 10));
		result = Puts(buffer, ptr - buffer);
		CLOCK_ASSERT(result == 0, result);
	    }
	}
	else { // only need to update deciseconds
	    ptr    = vt_goto(buffer, CLOCK_ROW, CLOCK_TENTHS);
	    ptr    = sprintf_char(ptr, '0' + (char)tenths);
	    result = Puts(buffer, ptr - buffer);
	    CLOCK_ASSERT(result == 0, result);
	}
    }
}

static void pq_init(clock_pq* q) {
    q->count = 1;
    q->delays[0].time = 0;
    q->delays[0].tid  = 0;
    for (uint i = 1; i < TASK_MAX; i++) {
	q->delays[i].time = INT_MAX;
	q->delays[i].tid  = -1;
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

    for (uint i = 1; i < TASK_MAX; i++)
	debug_log("q->delays[%u] = %u - %u", i, delays[i].time, delays[i].tid);
}
#endif

static void _error(int tid, int code) {
    log("Failed to reply to %u (%d)", tid, code);
}

static void _startup(clock_pq* pq) {
    // allow tasks to send messages to the clock server
    clock_server_tid = myTid();
    int result = RegisterAs((char*)"clock");
    if (result) {
	log("Failed to register clock server name (%d)", result);
	return;
    }

    result = Create(TASK_PRIORITY_HIGH, clock_notifier);
    if (result < 0) {
	log("Failed to create clock_notifier (%d)", result);
	return;
    }

    result = Create(TASK_PRIORITY_MEDIUM_LO, clock_ui);
    if (result < 0) {
	log("Failed to create clock_ui (%d)", result);
	return;
    }

    pq_init(pq);

    log("Clock Server started at %d", clock_server_tid);
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

        UNUSED(siz);
	assert(req.type >= CLOCK_NOTIFY && req.type <= CLOCK_DELAY_UNTIL,
	       "CS: Invalid message type from %u (%d) size = %u",
	       tid, req.type, siz);

	switch (req.type) {
	case CLOCK_NOTIFY:
	    // reset notifier ASAP
	    result = Reply(tid, NULL, 0);
	    if (result)
		log("Failed to reply to clock_notifier (%d)", result);

	    // tick-tock
	    time++;

	    while (pq_peek(&q) <= time) {
		tid = pq_delete(&q);
		assert(tid >= 0, "Waking up invalid tid!");
		result = Reply(tid, (char*)&time, sizeof(time));
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
