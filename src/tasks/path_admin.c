
#include <vt100.h>
#include <debug.h>

#include <track_node.h>
#include <tasks/path_worker.h>
#include <tasks/name_server.h>

#include <tasks/path_admin.h>

#define NUM_WORKERS 3
#define WORKER_PRIORITY 3

typedef enum {
    PA_GET_PATH,
    PA_REQ_WORK,

    PA_REQ_COUNT
} pa_req_type;

typedef struct {
    pa_req_type  type;
    path_request req;
} pa_request;

void path_admin() {
    int tid, result;
    pa_request req;
    const track_node* const track;
    
    int worker_pool[8];
    int pool_size = 0;
    int pool_head = 0;
    int pool_tail = 0;

    result = RegisterAs((char*)PATH_ADMIN_NAME);
    assert(0 == result, "Failed registering the path admin");

    memset(worker_pool, -1, sizeof(worker_pool));

    result = Receive(&tid, (char*)&track, sizeof(track));
    assert(tid == myParentTid(), "Invalid task tried to setup path admin")
    assert(result == sizeof(track), "received invalid setup info(%d)", result);


    for (int i = 0; i < NUM_WORKERS; i++) {
        result = Create(WORKER_PRIORITY, path_worker);
        assert(result > 0, "path admin failed to create worker %d", result);
        result = Send(result, (char*)&track, sizeof(track), NULL, 0);
    }
    Reply(tid, NULL, 0);

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));
        assert(result == sizeof(req), "Received invalid request %d", result);

        switch (req.type) {
        case PA_GET_PATH:
            assert(pool_size > 0,
                   "called into admin while no workers are present");
            result = Reply(worker_pool[pool_head++],
                           (char*)&req.req,
                           sizeof(req.req));
            pool_head &= 7;
            Reply(tid, NULL, 0);
            pool_size--;
            break;
        case PA_REQ_WORK:
            worker_pool[pool_tail++] = tid;
            pool_tail &= 7;
            pool_size++;
            break;
        default:
        case PA_REQ_COUNT:
            ABORT("path admin got invalid request %d", req.type);
            break;
        }
    }
}

