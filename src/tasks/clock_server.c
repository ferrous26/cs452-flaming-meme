
#include <pq.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/priority.h>

#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>

int clock_server_tid;

typedef enum {
    CLOCK_NOTIFY      = 1,
    CLOCK_DELAY       = 2,
    CLOCK_TIME        = 3,
    CLOCK_DELAY_UNTIL = 4
} clock_req_type;

typedef struct {
    clock_req_type type;
    int            ticks;
} clock_req;


#define CLOCK_ASSERT(expr, result)              \
    if (!(expr)) ABORT("Action failed in clock (%d)", result);

static void __attribute__ ((noreturn)) clock_notifier() {
    int clock = myParentTid();
    int   msg = CLOCK_NOTIFY;

    RegisterAs((char*)CLOCK_NOTIFIER_NAME);

    FOREVER {
        int result = AwaitEvent(CLOCK_TICK, NULL, 0);
        CLOCK_ASSERT(result == 0, result);

        result = Send(clock,
                      (char*)&msg, sizeof(msg),
                      NULL, 0);
        CLOCK_ASSERT(result == 0, result);
    }
}

static void __attribute__ ((noreturn)) clock_ui() {
    RegisterAs((char*)CLOCK_UI_NAME);

    // CLOCK MM:SS.D
#define CLOCK_ROW     1
#define CLOCK_MINUTES 7
#define CLOCK_SECONDS 10
#define CLOCK_TENTHS  13

    char buffer[64];
    char* ptr   = buffer;

    ptr    = vt_goto(buffer, CLOCK_ROW, 1);
    ptr    = sprintf_string(ptr, "CLOCK 00:00.0");
    Puts(buffer, ptr - buffer);

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
                Puts(buffer, ptr - buffer);
            }
            else { // need to update seconds and tenths
                ptr    = vt_goto(buffer, CLOCK_ROW, CLOCK_SECONDS);
                ptr    = sprintf(ptr, "%c%c.0",
                                 '0' + (seconds / 10),
                                 '0' + (seconds % 10));
                Puts(buffer, ptr - buffer);
            }
        }
        else { // only need to update deciseconds
            ptr    = vt_goto(buffer, CLOCK_ROW, CLOCK_TENTHS);
            ptr    = sprintf_char(ptr, '0' + (char)tenths);
            Puts(buffer, ptr - buffer);
        }
    }
}

typedef struct {
    int time;
    priority_queue q;
    pq_node        pq_heap[TASK_MAX];
} cs_context;

static inline void _reply_with_time(cs_context* const ctxt,
                                    const int tid,
                                    const char* caller) {

    const int result = Reply(tid,
                             (char*)&ctxt->time,
                             sizeof(ctxt->time));

    assert(result == 0, caller, result);
}

static void _startup(cs_context* const ctxt) {
    // allow tasks to send messages to the clock server
    clock_server_tid = myTid();

    int result = Create(CLOCK_NOTE_PRIORITY, clock_notifier);
    if (result < 0)
        ABORT("Failed to create clock_notifier (%d)", result);

    result = RegisterAs((char*)CLOCK_SERVER_NAME);
    if (result)
        ABORT("Failed to register clock server name (%d)", result);

    result = Create(CLOCK_UI_PRIORITY, clock_ui);
    if (result < 0)
        ABORT("Failed to create clock_ui (%d)", result);

    ctxt->time = 0;
    pq_init(&ctxt->q, ctxt->pq_heap, TASK_MAX);
}

void clock_server() {
    clock_req   req;    // store incoming requests
    int         result; // used by Reply() calls
    cs_context context;
    const int  missed_deadline = MISSED_DEADLINE;

    _startup(&context);

    FOREVER {
        int tid;
        const int siz = Receive(&tid, (char*)&req, sizeof(req));

        UNUSED(siz); // TODO: assert siz is minimum size
        assert(req.type >= CLOCK_NOTIFY && req.type <= CLOCK_DELAY_UNTIL,
               "CS: Invalid message type from %u (%d) size = %u",
               tid, req.type, siz);

        switch (req.type) {
        case CLOCK_NOTIFY:
            // reset notifier ASAP
            result = Reply(tid, NULL, 0);
            CLOCK_ASSERT(result == 0, result);

            // tick-tock
            context.time++;
            while (pq_peek_key(&context.q) <= context.time) {
                tid = pq_delete(&context.q);
                _reply_with_time(&context, tid, "Delay %d");
            }
            break;

        case CLOCK_DELAY:
            // value is checked on the calling task
            pq_add(&context.q, req.ticks + context.time, tid);
            break;

        case CLOCK_TIME:
            _reply_with_time(&context, tid, "Time %d");
            break;

        case CLOCK_DELAY_UNTIL:
            // if we missed the deadline, wake task up right away, but
            // also log something if in debug mode
            if (req.ticks < context.time) {
                clog(context.time,
                     "[Clock] %d missed DelayUntil(%d)",
                     tid, req.ticks);
                result = Reply(tid, (char*)&missed_deadline, sizeof(int));
                assert(result == 0, "MISSED DEADLINE, %d", result);
            }
            // we treat deadlines happening right away as being on time
            else if (req.ticks == context.time) {
                _reply_with_time(&context, tid, "DelayUntil %d");
            }
            else {
                pq_add(&context.q, req.ticks, tid);
            }
            break;
        }
    }
}

int Time() {
    clock_req req = {
        .type = CLOCK_TIME
    };

    int time;
    int result = Send(clock_server_tid,
                      (char*)&req,  sizeof(clock_req_type),
                      (char*)&time, sizeof(time));

    // Note: since we return _result_ in error cases, we inherit
    // additional error codes not in the kernel spec for this function
    if (result == sizeof(time))
        return time;

    // maybe clock server died, so we can try again
    if (result == INVALID_TASK || result == INCOMPLETE)
        ABORT("Clock server died");

    // else, error out
    return result;
}

int Delay(int ticks) {
    // handle negative/non-delay cases on the task side
    if (ticks <= 0) {
        if (ticks < 0) // only when strictly less than 0
            log("%d missed deadline with Delay(%d).", myTid(), ticks);
        return MISSED_DEADLINE;
    }

    clock_req req = {
        .type  = CLOCK_DELAY,
        .ticks = ticks
    };

    int time;
    int result = Send(clock_server_tid,
                      (char*)&req,  sizeof(clock_req),
                      (char*)&time, sizeof(time));

    if (result == sizeof(int))
        return time;

    // maybe clock server died
    if (result == INVALID_TASK || result == INCOMPLETE)
        ABORT("Clock server died");

    return result;
}

int DelayUntil(int ticks) {
    // handle negative/non-delay cases on the task side
    if (ticks <= 0)
        ABORT("%d made nonsensical DelayUntil(%d) call.", myTid(), ticks);

    clock_req req = {
        .type = CLOCK_DELAY_UNTIL,
        .ticks = ticks
    };

    int time;
    int result = Send(clock_server_tid,
                      (char*)&req,  sizeof(clock_req),
                      (char*)&time, sizeof(time));

    if (result == sizeof(time))
        return time;

    // maybe clock server died, so we can try again
    if (result == INVALID_TASK || result == INCOMPLETE)
        ABORT("Clock server died");

    return result;
}
