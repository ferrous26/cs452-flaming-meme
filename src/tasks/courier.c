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
    log ("[SensorNotifier%d] has died...", myTid());
}

void time_notifier() {
    int tid;

    struct {
        tnotify_header head;
        char           reply[248];
    } delay_req;

    int result = Receive(&tid, (char*)&delay_req, sizeof(delay_req));
    
    Reply(tid, NULL, 0);

    do {
        assert(result >= (int)sizeof(delay_req.head),
               "Recived an invalid setup message %d / %d",
               result, sizeof(delay_req.head));

        switch (delay_req.head.type) { 
        case DELAY_RELATIVE:
            Delay(delay_req.head.ticks);
            break;
        case DELAY_UNTIL:
            DelayUntil(delay_req.head.ticks);
            break;
        } 

        const int reply_size = result - (int)sizeof(delay_req.head);
        result = Send(tid,
                      (char*)&delay_req.reply, reply_size,
                      (char*)&delay_req, sizeof(delay_req));
    } while (result != 0);

    log("[TimeNotifier%d] has died...", myTid());
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

    log("[Courier%d] has died...", myTid());
}






