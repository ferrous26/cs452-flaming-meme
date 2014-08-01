
#include <std.h>
#include <path.h>
#include <debug.h>
#include <vt100.h>
#include <syscall.h>
#include <dijkstra.h>
#include <track_node.h>

#include <tasks/path_admin.h>
#include <tasks/path_worker.h>

void path_worker() {
    const track_node* const track;
    const int ptid = myParentTid();

    path_node path[80];
    pa_request    req = {
        .type = PA_REQ_WORK
    };
    path_response response = {
        .path = path
    };

    int tid;
    int result = Receive(&tid, (char*)&track, sizeof(track));
    assert(ptid == tid, "Worker %d got init from invalid task", myTid());
    assert(sizeof(track) == result,
           "Worker %d received invalid response", myTid());
    Reply(tid, NULL, 0);


    FOREVER {
        result = Send(ptid,
                      (char*)&req.type, sizeof(req.type),
                      (char*)&req.req, sizeof(req.req));
        assert(sizeof(req.req) == result,
               "Received invalid request from %d", tid);

        path_requisition requisition = {
            .start               = track + req.req.sensor_from,
            .end                 = track + req.req.sensor_to,
            .train_offset        = req.req.opts & OPTS_TRAIN_NUM_MASK,
            .reserve_dist        = req.req.reserve * 1000,
            .allow_short_move    = !(req.req.opts & PATH_SHORT_MOVE_OFF_MASK),
            .allow_start_reverse = !(req.req.opts & PATH_START_REVERSE_OFF_MASK),
            .allow_approach_back = !(req.req.opts & PATH_BACK_APPROACH_OFF_MASK)
        };

        response.response = req.req.header;
        response.size     = dijkstra(track,
                                     &requisition,
                                     path,
                                     &response.reserved);

        result = Send(req.req.requestor,
                      (char*)&response, sizeof(response),
                      NULL, 0);

        assert(0 == result, "Worked failed to respond to %d",
                req.req.requestor);
    }
}
