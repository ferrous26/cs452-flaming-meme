
#include <vt100.h>
#include <debug.h>

#include <track_node.h>
#include <char_buffer.h>
#include <tasks/path_worker.h>
#include <tasks/name_server.h>

#include <tasks/path_admin.h>

#define NUM_WORKERS 8
#define WORKER_PRIORITY 3

TYPE_BUFFER(int, NUM_WORKERS)

typedef struct {
    int_buffer worker_pool;
} pa_context;

void path_admin() {
    int tid, result;
    pa_request req;
    const track_node* const track;
    
    pa_context context;
    intb_init(&context.worker_pool);

    result = RegisterAs((char*)PATH_ADMIN_NAME);
    assert(0 == result, "Failed registering the path admin");

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
        assert(result > 0, "Received invalid request %d", result);

        switch (req.type) {
        case PA_GET_PATH:
            assert(intb_count(&context.worker_pool) > 0,
                   "called into admin while no workers are present"); 
            
            int worker_id = intb_consume(&context.worker_pool);
            result = Reply(worker_id, (char*)&req.req, sizeof(req.req));
            assert(0 == result, "failed passing work to worker %d", worker_id);

            result = Reply(tid, (char*)&worker_id, sizeof(worker_id));
            assert(0 == result, "failed waking up requestor %d", worker_id);
            break;

        case PA_REQ_WORK:
            intb_produce(&context.worker_pool, tid);
            break;

        default:
        case PA_REQ_COUNT:
            ABORT("path admin got invalid request %d", req.type);
            break;
        }
    }
}

