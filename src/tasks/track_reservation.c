
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
#define DIR_FRONT 0
#define DIR_BACK  1

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
    int reserve[TRACK_MAX][2];
} _context;

static inline void _init(_context* const ctxt) {
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
}

void track_reservation() {
    int tid, result;
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

        const int index_for = context.track - req.node;
        const int index_rev = context.track - req.node->reverse;

        switch (req.type) {
        case RESERVE_SECTION: {
            log(LOG_HEAD "Reserving Section %d For %d",
                index_for, req.train_num);

            const int old_owner = context.reserve[index_for][req.direction];

            if (old_owner == -1 || old_owner == req.train_num) {
                const int reply[1] = {RESERVE_SUCCESS};
                context.reserve[index_for][req.direction]     = req.train_num;
                context.reserve[index_rev][req.direction ^ 1] = req.train_num;
                result = Reply(tid, (char*)reply, sizeof(reply));
            } else {
                const int reply[2] = {RESERVE_FAILURE, old_owner};
                result = Reply(tid, (char*)reply, sizeof(reply));
                log(LOG_HEAD "Rejecting owned section %d->", index_for); 
            }
            assert(result == 0, "Failed to repond to track query");
        }   break;
        case RESERVE_RELEASE:
            log(LOG_HEAD "Releasing Section %d For %d",
                index_for, req.train_num);

            if (context.reserve[index_for][req.direction] == req.train_num) { 
                const int reply[1] = {RESERVE_SUCCESS};
                context.reserve[index_for][req.direction]     = -1;
                context.reserve[index_rev][req.direction ^ 1] = -1;
                result = Reply(tid, (char*)reply, sizeof(reply));
            } else {
                const int reply[1] = {RESERVE_FAILURE};
                result = Reply(tid, (char*)reply, sizeof(reply));
            }
            assert(result == 0, "Failed to repond to track query");
            break;
        case RESERVE_WHO: {
            const int* const track_point = &context.reserve[index_for][0];
            result = Reply(tid, (char*)&track_point, sizeof(*track_point));
            assert(result == 0, "Failed to repond to track query");
        }   break;
        }
    }
}

/*
int reserve_who_owns(const track_node* const node, int direction) {
    struct {
        

    }
    return 2;
}
*/


