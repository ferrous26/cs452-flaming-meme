#include <io.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/courier.h>

void time_notifier() {
    int tid;

    struct {
        tnotify_header head;
        char           reply[248];
    } delay_req;

    int size = Receive(&tid, (char*)&delay_req, sizeof(delay_req));
    assert(size > 0, "Failed receiving setup info (%d)", size);

    int result = Reply(tid, NULL, 0);
    assert(result == 0, "Failed responding to setup task (%d)", result);
    UNUSED(result);

    do {
        assert(size >= (int)sizeof(delay_req.head),
               "Received an invalid setup message %d / %d",
               size, sizeof(delay_req.head));

        switch (delay_req.head.type) {
        case DELAY_RELATIVE:
            Delay(delay_req.head.ticks);
            break;
        case DELAY_ABSOLUTE:
            DelayUntil(delay_req.head.ticks);
            break;
        }

        const size_t reply_size = (size_t)size - sizeof(delay_req.head);
        size = Send(tid,
                    (char* const)&delay_req.reply, reply_size,
                    (char* const)&delay_req, sizeof(delay_req));
    } while (size != 0);
}

void delayed_one_way_courier() {
    struct {
        tdelay_header head;
        char          message[244];
    } delay_req;

    int tid, result;
    const int size = Receive(&tid, (char*)&delay_req, sizeof(delay_req));
    assert(size >= (int)sizeof(delay_req.head),
           "Recived an invalid setup message %d / %d",
           size, sizeof(delay_req.head));

    result = Reply(tid, NULL, 0);
    assert(result == 0, "Failed reposnding to setup task (%d)", result);
    UNUSED(result);

    if (delay_req.head.ticks) {
        switch (delay_req.head.type) {
        case DELAY_RELATIVE:
            Delay(delay_req.head.ticks);
            break;
        case DELAY_ABSOLUTE:
            DelayUntil(delay_req.head.ticks);
            break;
        }
    }

    const size_t send_size = (size_t)size - sizeof(tdelay_header);
    result = Send(delay_req.head.receiver,
                  delay_req.message, send_size,
                  delay_req.message, sizeof(delay_req.message));

    assert(result >= 0,
           "failed sending to receiver %d (%d)",
           delay_req.head.receiver, result);
}

void async_courier() {

    struct {
        int   receiver;
        char  message[252];
    } package;

    int tid;
    int result;
    const int size = Receive(&tid, (char*)&package, sizeof(package));
    assert(size >= (int)(sizeof(int) * 2),
           "Recived an invalid setup message %d / %d",
           size, sizeof(int));

    result = Reply(tid, NULL, 0);
    assert(result == 0, "Failed reposnding to setup task (%d)", result);
    UNUSED(result);

    const size_t send_size = (size_t)size - sizeof(int);
    result = Send(package.receiver,
                  package.message, send_size,
                  package.message, sizeof(package.message));

    assert(result >= 0,
           "failed sending to receiver %d (%d)",
           package.receiver, result);

    result = Send(tid,
                  package.message, (size_t)result,
                  NULL, 0);

    assert(result >= 0,
           "failed sending back to the receiver %d (%d)",
           package.receiver, result);
}

void courier() {
    int tid;
    courier_package package;

    char buffer[512];

    int result = Receive(&tid, (char*)&package, sizeof(package));
    assert(result == sizeof(package),
           "Courier got invalid package from %d %d/%d",
           tid, result, sizeof(package));

    if (package.size > 0) {
        memcpy(buffer, package.message, (uint)package.size);
    } else if (package.size < 0) {
        memcpy(buffer, package.message, (uint)-package.size);
    }

    result = Reply(tid, NULL, 0);
    assert(result == 0,
           "Courier Problem sending back instancing task %d", tid);

    result = package.size;
    do {
        const size_t size = (size_t)result;
        if (result > 0) {
            result = Send(package.receiver,
                          buffer, size,
                          buffer, sizeof(buffer));
            assert(result >= 0,
                   "problem sending %d bytes to %d from %d (%d)",
                   size, package.receiver, tid, result);
        }

        result = Send(tid,
                      buffer, (size_t)abs(result),
                      buffer, sizeof(buffer));
        assert(result >= 0, "Error sending response to %d (%d)",
               tid, result);
    } while (result > 0);
}
