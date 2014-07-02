#include <io.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/courier.h>

void courier() {
    int tid;
    char buffer[256];
    courier_package package;

    int result = Receive(&tid, (char*)&package, sizeof(package));
    assert(result == sizeof(package),
           "Courier got invalid package from %d %d/%d",
           tid, result, sizeof(package));
    memcpy(buffer, package.message, package.size);

    log ("courier sending to %d %p(%d)", package.receiver, package.message, package.size);
    
    result = Reply(tid, NULL, 0);
    assert(result == 0,
           "Courier Problem sending back instancing task %d", tid);

    result = package.size;
    do {
        result = Send(package.receiver,
                      buffer, result,
                      buffer, sizeof(buffer));

        assert(result > 0,
               "problem with response to %d from receiver %d (%d)",
               tid, package.receiver, result); 

        result = Send(tid,
                      buffer, result,
                      buffer, sizeof(buffer));

        assert(result >= 0, "Error sending response to %d", tid);
    } while (result > 0);

    log("Courier %d prefers death...", myTid());
}






