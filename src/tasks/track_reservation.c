
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

#define LOG_HEAD  "[TRACK RESERVATION]\t"
// #define DIR_FRONT 0
// #define DIR_BACK  1


#define NUM_RES_ENTRIES (NUM_TRAINS + 1)
#define CHECK_TRAIN(train) assert(XBETWEEN(train, -1, NUM_RES_ENTRIES + 1), \
                                  "Bad Register Train Num %d", train)       

static int track_reservation_tid;

//TODO: remove when no longer needed
static const track_node* term_hack;

typedef enum {
    RESERVE_SECTION,
    RESERVE_RELEASE,
    RESERVE_CAN_DOUBLE,
    RESERVE_WHO,

    RESERVE_REQ_TYPE_COUNT
} tr_req_type;

typedef enum {
    RESERVE_SUCCESS,
    RESERVE_FAILURE,
} tr_reply_type;

typedef struct { 
    const track_node* const track;
    int reserve[TRACK_MAX];
    int reserved_nodes[NUM_TRAINS + 1];
    // extra space is reserved for the terminal
} _context;

static TEXT_COLD void _init(_context* const ctxt) {
    int tid, result;
    
    memset(ctxt, -1, sizeof(*ctxt));
    memset(ctxt->reserved_nodes, 0, sizeof(ctxt->reserved_nodes));

    result = RegisterAs((char*)TRACK_RESERVATION_NAME);
    if (result != 0) ABORT(LOG_HEAD "failed to register name %d", result);

    result = Receive(&tid, (char*)&ctxt->track, sizeof(ctxt->track));
    assert(tid == myParentTid(),
           "received startup from non parent (%d)", tid);
    assert(result == sizeof(ctxt->track),
           "received invalid startup message (%d)", result);
    result = Reply(tid, NULL, 0);
    assert(result == 0, "failed replying after initalization");

    term_hack = ctxt->track;
}

int get_reserve_length(const track_node* const node) {
    int result = 0;
    
    if (node->type == NODE_BRANCH)
        result = MIN(node->edge[DIR_STRAIGHT].dist,
                     node->edge[DIR_CURVED].dist);
    else if (node->type != NODE_EXIT)
        result = node->edge[DIR_AHEAD].dist;

    return result >> 1;
}

static inline int // __attribute__((pure))
get_node_index(const _context* const ctxt, const track_node* const node) {
    const int index = node - ctxt->track;
    assert(XBETWEEN(index, -1, TRACK_MAX+1),
           "Bad Track Node Index %d %p", index, node);
    return index;
}

static int __attribute__((pure))
_who_owns(const _context* const ctxt,
          const track_node* const node){
    
    const int index = get_node_index(ctxt, node);
    const int owner = ctxt->reserve[index]; 

    return owner;
}

static inline int __attribute__((pure))
is_node_adjacent(const _context* const ctxt,
                 const track_node* const n1,
                 const int train) {
    switch (n1->type) {
    case NODE_NONE:
    case NODE_MERGE:
    case NODE_SENSOR:
        return train == _who_owns(ctxt, n1->edge[DIR_AHEAD].dest->reverse)
            || train == _who_owns(ctxt, n1->reverse);
    case NODE_ENTER:
        return train == _who_owns(ctxt, n1->edge[DIR_AHEAD].dest->reverse);
    case NODE_EXIT:
        return train == _who_owns(ctxt, n1->reverse);
    case NODE_BRANCH:
        return train == _who_owns(ctxt, n1->edge[DIR_STRAIGHT].dest->reverse)
            || train == _who_owns(ctxt, n1->edge[DIR_CURVED].dest->reverse)
            || train == _who_owns(ctxt, n1->reverse);
    }
    return false;
}

static inline int __attribute__((pure))
is_node_edge(const _context* const ctxt,
             const track_node* const n1,
             const int train) {
    switch (n1->type) {
    case NODE_NONE:
    case NODE_MERGE:
    case NODE_SENSOR:
        return train != _who_owns(ctxt, n1->edge[DIR_AHEAD].dest->reverse)
            || train != _who_owns(ctxt, n1->reverse);
    case NODE_ENTER:
        return train != _who_owns(ctxt, n1->edge[DIR_AHEAD].dest->reverse);
    case NODE_EXIT:
        return train != _who_owns(ctxt, n1->reverse);
    case NODE_BRANCH:
        return train != _who_owns(ctxt, n1->edge[DIR_STRAIGHT].dest->reverse)
            || train != _who_owns(ctxt, n1->edge[DIR_CURVED].dest->reverse)
            || train != _who_owns(ctxt, n1->reverse);
    }
    return false;
}

static void _handle_reserve_section(_context* const ctxt,
                                    const int tid,
                                    const track_node* const node,
                                    const int train) {
    CHECK_TRAIN(train);    
    const int index = get_node_index(ctxt, node);
    const int owner = ctxt->reserve[index];
    int result, reply[] = {RESERVE_SUCCESS, 0};
    
    if (owner == -1) {
        log(LOG_HEAD "Reserving Section %s For %d", node->name, train);

        if (ctxt->reserved_nodes[train]) {
            if (!is_node_adjacent(ctxt, node, train)) {
                assert(false, "train %d tried to get unadjacent node %s",
                       train, node->name);
            } 
        }

        ctxt->reserve[index] = train;
        reply[1]             = get_reserve_length(node);
        ctxt->reserved_nodes[train]++;
        result = Reply(tid, (char*)reply, sizeof(reply));
    } else if(owner == train) {
        log(LOG_HEAD "Already Reserved Section %s For %d",
            node->name, train);
        reply[1] = get_reserve_length(node);
        result   = Reply(tid, (char*)reply, sizeof(reply));
    } else {
        reply[0] = RESERVE_FAILURE;
        reply[1] = owner;

        log(LOG_HEAD "train %d can't reserve %s, already owned by %d",
            train, node->name, owner);
        result = Reply(tid, (char*)reply, sizeof(reply));
    } 
    assert(result == 0, "Failed to repond to track query");
}

static void _handle_release_section(_context* const ctxt,
                                    const int tid,
                                    const track_node* const node,
                                    const int train) {
    CHECK_TRAIN(train);    
    const int index = get_node_index(ctxt, node);
    const int owner = ctxt->reserve[index];
    int result;

    assert(XBETWEEN(train, -1, NUM_TRAINS+2),
           "Bad Register Train Num %d", train);
    assert(XBETWEEN(index, -1, TRACK_MAX+1),
           "Bad Track Node Index %d %p", index, node);

    log(LOG_HEAD "Releasing Section %s For %d",
        node->name, train);

    if (owner == train) { 
        ctxt->reserve[index] = -1;
        ctxt->reserved_nodes[train]--;

        const int reply[] = {RESERVE_SUCCESS};
        result = Reply(tid, (char*)reply, sizeof(reply));
    } else {
        const int reply[] = {RESERVE_FAILURE, owner};
        result = Reply(tid, (char*)reply, sizeof(reply));
    }

    assert(result == 0, "Failed to repond to track query");
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
        assert(result >= (int)(sizeof(req.type)
                              +sizeof(req.node)
                              +sizeof(req.direction)),
               LOG_HEAD "Received invalid message (%d)",
               result);
        assert(req.type < RESERVE_REQ_TYPE_COUNT,
               LOG_HEAD"got invalid req type %d", req.type);

        switch (req.type) {
        case RESERVE_SECTION: {
            const track_node* const node =
                req.direction ? req.node->reverse : req.node;
            _handle_reserve_section(&context, tid, node, req.train_num);
        }   break;

        case RESERVE_RELEASE: {
            const track_node* const node =
                req.direction ? req.node->reverse : req.node;
            _handle_release_section(&context, tid, node, req.train_num);
        }   break;

        case RESERVE_WHO: {
            const track_node* const node =
                req.direction ? req.node->reverse : req.node;
            const int owner = _who_owns(&context, node);
            log(LOG_HEAD "LOOKUP %d on %s", owner, node->name); 
            result = Reply(tid, (char*)&owner, sizeof(owner));
            assert(result == 0, "Failed to repond to track query");
        }   break;

        case RESERVE_CAN_DOUBLE: {
            int reply = 0;
            const int own1 = _who_owns(&context, req.node);
            if (own1 == -1 || own1 == req.train_num) {
                const int own2 = _who_owns(&context, req.node->reverse);
                reply          = (own2 == -1 || own2 == req.train_num); 
            }

            log(LOG_HEAD "LOOKUP %d - %d on %s",
                req.train_num, reply, req.node->name);
            result = Reply(tid, (char*)&reply, sizeof(reply));
            assert(result == 0, "Failed to repond to track query");
        }   break;

        case RESERVE_REQ_TYPE_COUNT:
            ABORT("GOT INVALID RESERVATION TYPE");
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

int reserve_can_double(const track_node* const node, const int train) {
    struct {
        tr_req_type             type;
        const track_node* const node;
        int                     train;
    } req = {
        .type  = RESERVE_CAN_DOUBLE,
        .node  = node,
        .train = train
    };

    int size, result;
    size = Send(track_reservation_tid,
                (char*)&req,    sizeof(req),
                (char*)&result, sizeof(result));
    assert(size >= (int)sizeof(int), "Bad send to track reservation");
    return result; 
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

