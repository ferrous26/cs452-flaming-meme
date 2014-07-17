
#include <std.h>
#include <debug.h>
#include <syscall.h>
#include <normalize.h>
#include <track_node.h>

#include <tasks/priority.h>
#include <tasks/name_server.h>

typedef enum {
    RESERVE_SECTION,
    RESERVE_RELEASE,
    RESERVE_WHOOWNS
} tr_req_type;

typedef struct { 
    const track_node* const track;
} _context;

static inline void _init(_context* const ctxt) {
    int tid, result;
    
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
        int type;
        int body;
    } req;

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));
        assert(result > 0,
               "[Track Reservation] Received invalid message (%d)",
               result);

        switch (req.type) {

        }
    }
}

