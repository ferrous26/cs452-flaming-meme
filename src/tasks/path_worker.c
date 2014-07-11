
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

        response.response = req.req.header;
        response.size     = dijkstra(track,
                                     &track[req.req.sensor_from],
                                     &track[req.req.sensor_to],
                                     path);
        assert(response.size > 0, "No path exists to sensor %d",
               req.req.sensor_to);

        result = Send(req.req.requestor,
                      (char*)&response, sizeof(response),
                      NULL, 0);
        assert(0 == result, "Worked failed to respond to %d",
                req.req.requestor);
    }
}

