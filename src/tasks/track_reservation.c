
#include <std.h>
#include <debug.h>
#include <syscall.h>
#include <normalize.h>
#include <track_node.h>
#include <track_data.h>

#include <tasks/priority.h>
#include <tasks/name_server.h>
#include <tasks/track_reservation.h>

#define LOG_HEAD  "[TRACK RESERVATION]"
// #define DIR_FRONT 0
// #define DIR_BACK  1

static int track_reservation_tid;

//TODO: remove when no longer needed
static const track_node* term_hack;

typedef enum {
    RESERVE_SECTION,
    RESERVE_RELEASE,
    RESERVE_WHO
} tr_req_type;

typedef enum {
    RESERVE_SUCCESS,
    RESERVE_FAILURE,
} tr_reply_type;

typedef struct { 
    const track_node* const track;
    int train_pieces[8];
    int reserve[TRACK_MAX];
} _context;

static TEXT_COLD void _init(_context* const ctxt) {
    int tid, result;
    memset(ctxt, -1, sizeof(*ctxt));

    result = RegisterAs((char*)TRACK_RESERVATION_NAME);

    result = Receive(&tid, (char*)&ctxt->track, sizeof(ctxt->track));
    assert(tid == myParentTid(),
           "received startup from non parent (%d)", tid);
    assert(result == sizeof(ctxt->track),
           "received invalid startup message (%d)", result);
    result = Reply(tid, NULL, 0);
    assert(result == 0, "failed replying after initalization");

    term_hack = ctxt->track;
}

void track_reservation() {
    int tid, result;
    track_reservation_tid = myTid();

    _context context;
    _init(&context);

    struct {
        tr_req_type             type;
        const track_node* const node;
        int                     direction;
        int                     train_num;
    } req;

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));
        assert(result > 0,
               "[Track Reservation] Received invalid message (%d)",
               result);

        const int index = (req.direction ? req.node->reverse : req.node)
                            - context.track;
        assert(index < TRACK_MAX, "index is out of bounds (%d - %p + %d)",
               index, req.node, req.direction);

        switch (req.type) {
        case RESERVE_SECTION: {
            log(LOG_HEAD "Reserving Section %s For %d",
                context.track[index].name, req.train_num);

            const int old_owner = context.reserve[index];
            if (old_owner == -1 || old_owner == req.train_num) {
                const int reply[1] = {RESERVE_SUCCESS};
                context.reserve[index] = req.train_num;
                result = Reply(tid, (char*)reply, sizeof(reply));
            } else {
                const int reply[2] = {RESERVE_FAILURE, old_owner};
                result = Reply(tid, (char*)reply, sizeof(reply));
                log(LOG_HEAD "Rejecting owned section %s",
                    context.track[index].name);
            }

            assert(result == 0, "Failed to repond to track query");
        }   break;

        case RESERVE_RELEASE:
            log(LOG_HEAD "Releasing Section %s For %d",
                context.track[index].name, req.train_num);

            if (context.reserve[index] == req.train_num) { 
                const int reply[1] = {RESERVE_SUCCESS};
                context.reserve[index] = -1;
                result = Reply(tid, (char*)reply, sizeof(reply));
            } else {
                const int reply[1] = {RESERVE_FAILURE};
                result = Reply(tid, (char*)reply, sizeof(reply));
            }
            assert(result == 0, "Failed to repond to track query");
            break;

        case RESERVE_WHO: {
            const int* const track_point = &context.reserve[index];
            log(LOG_HEAD "LOOKUP %d on %s", *track_point,
                context.track[index].name); 
            result = Reply(tid, (char*)track_point, sizeof(*track_point));
            assert(result == 0, "Failed to repond to track query");
        }   break;

        }
    }
}

int reserve_who_owns(const track_node* const node, const int dir) {
    struct {
        tr_req_type             type;
        const track_node* const node;
        int                     dir;
    } req = {
        .type = RESERVE_WHO,
        .node = node,
        .dir  = dir > 0 ? 1 : 0
    };

    int result, owner;
    result = Send(track_reservation_tid,
                  (char*)&req,   sizeof(req),
                  (char*)&owner, sizeof(owner));
    assert(result == sizeof(int), "Bad send to track reservation");
    return owner;
}

int reserve_section(const track_node* const node,
                    const int dir,
                    const int train) {
    struct {
        tr_req_type             type;
        const track_node* const node;
        int                     dir;
        int                     train;
    } req = {
        .type  = RESERVE_SECTION,
        .node  = node,
        .dir   = dir > 0 ? 1 : 0,
        .train = train,
    };

    int size, result[2];
    size = Send(track_reservation_tid,
                (char*)&req,   sizeof(req),
                (char*)&result, sizeof(result));
    assert(size >= (int)sizeof(int), "Bad send to track reservation");
    return result[0];
}

int reserve_release(const track_node* const node,
                    const int dir,
                    const int train) {
    struct {
        tr_req_type             type;
        const track_node* const node;
        int                     dir;
        int                     train;
    } req = {
        .type  = RESERVE_RELEASE,
        .node  = node,
        .dir   = dir > 0 ? 1 : 0,
        .train = train,
    };

    int size, result[2];
    size = Send(track_reservation_tid,
                (char*)&req,   sizeof(req),
                (char*)&result, sizeof(result));
    assert(size >= (int)sizeof(int), "Bad send to track reservation");
    return result[0];
}
int reserve_who_owns_term(const int node_index, const int dir) {
    return reserve_who_owns(&term_hack[node_index], dir);
}

int reserve_section_term(const int node_index, const int dir, const int train) {
    return reserve_section(&term_hack[node_index], dir, train);
}

int reserve_release_term(const int node_index, const int dir, const int train) {
    return reserve_release(&term_hack[node_index], dir, train);
}

