
#include <std.h>
#include <debug.h>
#include <train.h>
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
    int reserve[TRACK_MAX];
    
    struct {
        const track_node* front;
        const track_node* back;
    } tracking[NUM_TRAINS + 1];
    // extra space is reserved for the terminal
} _context;

static TEXT_COLD void _init(_context* const ctxt) {
    int tid, result;
    
    memset(ctxt, -1, sizeof(*ctxt));
    for (int i = 0; i < NUM_TRAINS+1; i++) {
        ctxt->tracking[i].front = NULL;
        ctxt->tracking[i].back  = NULL;
    }

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

static inline int __attribute__((pure))
get_reserve_length(const track_node* const node) {
    int result = 0;
    
    if (node->type == NODE_BRANCH)
        result = MIN(node->edge[DIR_STRAIGHT].dist,
                     node->edge[DIR_CURVED].dist);
    else if (node->type != NODE_EXIT)
        result = node->edge[DIR_AHEAD].dist;

    return result >> 1;
}

static inline int __attribute__((pure))
is_node_adjacent(const track_node* const n1,
                 const track_node* const n2) {
    switch (n1->type) {
    case NODE_NONE:
    case NODE_MERGE:
    case NODE_SENSOR:
        return n2 == n1->edge[DIR_AHEAD].dest->reverse
            || n2 == n1->reverse;
    case NODE_ENTER:
        return n2 == n1->edge[DIR_AHEAD].dest->reverse;
    case NODE_EXIT:
        return n2 == n1->reverse;
    case NODE_BRANCH:
        return n2 == n1->edge[DIR_STRAIGHT].dest->reverse
            || n2 == n1->edge[DIR_CURVED].dest->reverse
            || n2 == n1->reverse;
    }
    return 0;
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
        assert(result > 0, LOG_HEAD "Received invalid message (%d)",
               result);

        const track_node* const node =
            req.direction ? req.node->reverse : req.node;
        const int index = node - context.track;

        assert(index < TRACK_MAX, "index is out of bounds (%d - %p + %d)",
               index, req.node, req.direction);

        switch (req.type) {
        case RESERVE_SECTION: {
            const int old_owner = context.reserve[index];
            assert(XBETWEEN(req.train_num, -1, NUM_TRAINS+2),
                   "Bad Register Train Num %d", req.train_num);
            
            if (old_owner == -1 || old_owner == req.train_num) {
                log(LOG_HEAD "Reserving Section %s For %d",
                    context.track[index].name, req.train_num);

                const int length  = get_reserve_length(node);
                      int reply[] = {RESERVE_SUCCESS, length};
                context.reserve[index] = req.train_num;
                result = Reply(tid, (char*)reply, sizeof(reply));

            /*
            if (context.tracking[req.train_num].front == NULL) {
                context.tracking[req.train_num].front = node;
                context.tracking[req.train_num].back  = node;
            } else if (is_node_adjacent(node, context.tracking[req.train_num].front)) {
                context.tracking[req.train_num].front = node;
            } else if (is_node_adjacent(node, context.tracking[req.train_num].back)) {
                context.tracking[req.train_num].back  = node;
            } else {
                log ("NOT IN A LINE REJECTED! %d\n\t%p\t%p", 
                     req.train_num, context.tracking[req.train_num].front, NULL);
                reply[0] = RESERVE_FAILURE;
            }
            */

            } else {
                const int reply[] = {RESERVE_FAILURE, old_owner};
                log(LOG_HEAD "Rejecting owned section %s",
                    context.track[index].name);
                result = Reply(tid, (char*)reply, sizeof(reply));
            }
            assert(result == 0, "Failed to repond to track query");
        }   break;

        case RESERVE_RELEASE:
            log(LOG_HEAD "Releasing Section %s For %d",
                context.track[index].name, req.train_num);
            assert(XBETWEEN(req.train_num, -1, NUM_TRAINS+2),
                   "Bad Register Train Num %d", req.train_num);

            if (context.reserve[index] == req.train_num) { 
                const int reply[] = {RESERVE_SUCCESS};
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
                (char*)result, sizeof(result));
    assert(size == (int)sizeof(result), "Bad send to track reservation");
    
    if (result[0] == RESERVE_FAILURE) return REQUEST_REJECTED;
    return result[1];
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
                (char*)result, sizeof(result));
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

