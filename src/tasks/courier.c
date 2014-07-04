#include <io.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#include <tasks/courier.h>

void sensor_notifier() {
    int tid, time;
    const int mc_tid = WhoIs((char*)MISSION_CONTROL_NAME);

    mc_req request = {
        .type = MC_D_SENSOR,
    };
    int* const sensor = &request.payload.int_value;

    int result = Receive(&tid, (char*)sensor, sizeof(*sensor));
    assert(result == sizeof(*sensor), "sensor notifier failed %d", result);
    Reply(tid, NULL, 0);

    do {
        assert(*sensor >= 0 && *sensor < 80,
               "sensor notifier got invalid sensor bank from %d (%d)",
               tid, *sensor);

        result = Send(mc_tid,
                      (char*)&request, sizeof(request),
                      (char*)&time, sizeof(time));

        result = Send(tid,
                      (char*)&time, sizeof(time),
                      (char*)sensor, sizeof(*sensor));
    } while (*sensor != -1);
    log ("sensor notifier dying now");
}

void courier() {
    int tid;
    courier_package package;
    
    char buffer[512];

    int result = Receive(&tid, (char*)&package, sizeof(package));
    assert(result == sizeof(package),
           "Courier got invalid package from %d %d/%d",
           tid, result, sizeof(package));
    memcpy(buffer, package.message, package.size);

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

    log("[Courier %d] has died...", myTid());
}






