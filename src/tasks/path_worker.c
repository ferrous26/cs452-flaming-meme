
#include <std.h>
#include <path.h>
#include <debug.h>
#include <vt100.h>
#include <syscall.h>
#include <dijkstra.h>
#include <track_node.h>

#include <tasks/path_worker.h>

void path_worker() {
    const track_node* const track;

    path_request req;
    path_node    path[80];
    const int    dispatch = myParentTid();
    // const int    req_work = 0;
    UNUSED(dispatch);
    //
    path_response response = {
        .path = path
    };

    int tid;
    int result = Receive(&tid, (char*)&track, sizeof(track));
    assert(dispatch == tid, "Worker %d got init from invalid task", myTid());
    assert(sizeof(track) == result,
           "Worker %d received invalid response", myTid());
    Reply(tid, NULL, 0);

    log("[PATH_FINDR] has started");
    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));
        assert(sizeof(req) == result, "Received invalid request from %d", tid);

        response.response = req.header;
        response.size     = dijkstra(track,
                                     &track[req.sensor_from],
                                     &track[req.sensor_to],
                                     path);

        assert(response.size > 0, "No path exists to sensor %d", req.sensor_to);
        result = Reply(req.requestor, (char*)&response, sizeof(response));
        assert(0 == result, "Worked failed to respond to %d", req.requestor);
        
        break;
    }
}

