
#include <std.h>
#include <debug.h>
#include <train.h>
#include <syscall.h>
#include <normalize.h>
#include <track_node.h>
#include <track_data.h>

#include <tasks/priority.h>
#include <tasks/name_server.h>
#include <tasks/term_server.h>  // used to debug print out full paths

#include <tasks/track_reservation.h>



#define LOG_HEAD  "[TRACK RESERVATION]\t"
// #define DIR_FRONT 0
// #define DIR_BACK  1


#define NUM_RES_ENTRIES (NUM_TRAINS + 1)
#define CHECK_TRAIN(train) assert(XBETWEEN(train, -1, NUM_RES_ENTRIES), \
                                  "Bad Register Train Num %d", train)

static int track_reservation_tid;

//TODO: remove when no longer needed
static const track_node* term_hack;

typedef enum {
    RESERVE_WHO,
    RESERVE_SECTION,
    RESERVE_CAN_DOUBLE,

    RESERVE_REQ_TYPE_COUNT
} tr_req_type;

typedef enum {
    RESERVE_SUCCESS,
    RESERVE_FAILURE,
} tr_reply_type;

typedef struct reserve_node {
    int owner;
    int iter;
} reserve_node;

typedef struct {
    const track_node* const track;
    reserve_node reserve[TRACK_MAX];
    int train_iter[NUM_RES_ENTRIES];
    // extra space is reserved for the terminal
} _context;

static TEXT_COLD void _init(_context* const ctxt) {
    int tid, result;
    memset(ctxt, -1, sizeof(*ctxt));

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
_get_node_index(const _context* const ctxt, const track_node* const node) {
    const int index = node - ctxt->track;
    assert(XBETWEEN(index, -1, TRACK_MAX),
           "Bad Track Node Index %d %p", index, node);
    return index;
}

static int _who_owns_index(_context* const ctxt,
                           const int index) {

    reserve_node* const res = &ctxt->reserve[index];    

    if (-1 == res->owner) return -1;
    else if (res->iter != ctxt->train_iter[res->owner]) res->owner = -1;

    return res->owner;
}

static int _who_owns(_context* const ctxt,
                     const track_node* const node){

    return _who_owns_index(ctxt, _get_node_index(ctxt, node));
}

static inline int
is_node_adjacent(_context* const ctxt,
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

static inline int
is_node_edge(_context* const ctxt,
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
                                    const int train,
                                    const track_node** node,
                                    const int lst_size) {
    CHECK_TRAIN(train);

    int result;
    int reply[] = {RESERVE_SUCCESS};
    ctxt->train_iter[train]++;
    
    #ifdef NIK
    char* ptr;
    char path_print[512];
    ptr = log_start(path_print);
    ptr = sprintf(ptr, "RESERVATION FOR TRAIN%d:", train);
    #endif

    for (int i = 0; i < lst_size; i++, node++) {
        const int index = _get_node_index(ctxt, *node);
        const int owner = _who_owns_index(ctxt, index);
        log("reserve %s for %d", ctxt->track[index].name, train);

        if (owner == -1) {
            if (i > 0 && !is_node_adjacent(ctxt, *node, train)) { 
                nik_log("%s is not adjacent to train %d",
                        ctxt->track[index].name, train);
            } else {
                #ifdef NIK
                if ((i & 7) == 0) ptr = sprintf(ptr, "\r\n\t");
                ptr = sprintf_string(ptr, (*node)->name); 
                ptr = sprintf_string(ptr, "\t");
                #endif

                ctxt->reserve[index].owner = train;
                ctxt->reserve[index].iter  = ctxt->train_iter[train];
            }

        } else if (owner == train) {
            nik_log(LOG_HEAD "%s double reserved by %d",
                    ctxt->track[index].name, train);

        } else {
            nik_log(LOG_HEAD "Failed reserving Section %s For %d, owned by %d",
                    (*node)->name, train, owner);
            reply[0] = RESERVE_FAILURE;
        }
    }

    #ifdef NIK
    Puts(path_print, ptr - path_print);
    #endif

    result = Reply(tid, (char*)reply, sizeof(reply));
    assert(0 == result, "Failed to repond to track query");
}

void track_reservation() {
    int tid, result;
    track_reservation_tid = myTid();

    _context context;
    _init(&context);

    struct {
        tr_req_type             type;
        const track_node* const node;
        int                     node_cnt;
        int                     train_num;
    } req;

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));
        assert(result >= (int)(sizeof(req.type) +sizeof(req.node)),
               LOG_HEAD "Received invalid message (%d)", result);
        assert(req.type < RESERVE_REQ_TYPE_COUNT,
               LOG_HEAD"got invalid req type %d", req.type);

        switch (req.type) {
        case RESERVE_SECTION: {
            _handle_reserve_section(&context,
                                    tid,
                                    req.train_num,
                                    (const track_node**)req.node,
                                    req.node_cnt);
        }   break;

        case RESERVE_WHO: {
            const int owner = _who_owns(&context, req.node);
            nik_log(LOG_HEAD "LOOKUP %d on %s", owner, req.node->name);
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

            nik_log(LOG_HEAD "LOOKUP %d gets BOOL(%d) on %s",
                    req.train_num, reply, req.node->name);
            result = Reply(tid, (char*)&reply, sizeof(reply));
            assert(result == 0, "Failed to repond to track query");
        }   break;

        case RESERVE_REQ_TYPE_COUNT:
            ABORT("GOT INVALID RESERVATION TYPE");
        }
    }
}

int reserve_who_owns(const track_node* const node) {
    struct {
        tr_req_type             type;
        const track_node* const node;
    } req = {
        .type = RESERVE_WHO,
        .node = node,
    };

    int result, owner;
    result = Send(track_reservation_tid,
                  (char*)&req,   sizeof(req),
                  (char*)&owner, sizeof(owner));
    assert(result == sizeof(int), "Bad send to track reservation");
    return owner;
}

int reserve_section(const int train,
                    const track_node** const node,
                    const int node_cnt) {
    struct {
        tr_req_type              type;
        const track_node** const node;
        int                      node_cnt;
        int                      train_num;
    } req = {
        .type      = RESERVE_SECTION,
        .node      = node,
        .node_cnt  = node_cnt,
        .train_num = train
    };

    int size, result[2];
    size = Send(track_reservation_tid,
                (char*)&req,   sizeof(req),
                (char*)result, sizeof(result));

    assert(size == (int)sizeof(int), "Bad send to track reservation");

    return result[0] == RESERVE_SUCCESS;
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

int reserve_who_owns_term(const int node_index) {
    return reserve_who_owns(&term_hack[node_index]);
}

int reserve_section_term(const int node_index, const int train) {
    const track_node* temp = &term_hack[node_index];
    return reserve_section(train, &temp, 1);
}
