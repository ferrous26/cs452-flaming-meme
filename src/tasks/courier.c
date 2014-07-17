#include <io.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/name_server.h>
#include <tasks/sensor_farm.h>
#include <tasks/clock_server.h>
#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#include <tasks/courier.h>

void sensor_notifier() {
    int       tid, has_header;
    const int sf_tid = WhoIs((char*)SENSOR_FARM_NAME);


    sf_type sensor_any = SF_D_SENSOR_ANY;
    struct {
        sf_type type;
        int     sensor;
    } sensor_one = {
        .type = SF_D_SENSOR
    }; 
    
    struct {
        int sensor_num;
        int return_head;
    } req;

    struct {
        int header;
        int body[2];
    } reply;

    int* const sensor_idx = &sensor_one.sensor;
    int result = Receive(&tid, (char*)&req, sizeof(req));
    Reply(tid, NULL, 0);

    do {
        assert(result >= (int)sizeof(*sensor_idx),
               "sensor notifier failed %d", result);

        *sensor_idx = req.sensor_num;
        has_header  = result > (int)sizeof(*sensor_idx);
        if(has_header) { reply.header = req.return_head; }

        assert(*sensor_idx >= 0 && *sensor_idx <= 80,
               "sensor notifier got invalid sensor num from %d (%d)",
               tid, *sensor_idx);

        if (80 == *sensor_idx) {
            result = Send(sf_tid,
                          (char*)&sensor_any, sizeof(sensor_any),
                          (char*)&reply.body,   sizeof(reply.body));
        } else {
            result = Send(sf_tid,
                          (char*)&sensor_one, sizeof(sensor_one),
                          (char*)&reply.body,   sizeof(reply.body));
        }
        assert(result > 0, "got back back data (%d)", result);

        if (has_header) {
            result = Send(tid,
                          (char*)&reply, sizeof(reply),
                          (char*)&req,   sizeof(req));
        } else {
            result = Send(tid,
                          (char*)&reply.body, sizeof(reply.body),
                          (char*)&req,        sizeof(req));
        }
        assert(result >= 0, "Failed sending back to creator (%d)", result);
    } while (result > 0);
    log ("[SensorNotifier%d] has died...", myTid());
}

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

        const int reply_size = size - (int)sizeof(delay_req.head);
        size = Send(tid,
                    (char*)&delay_req.reply, reply_size,
                    (char*)&delay_req, sizeof(delay_req));
    } while (size != 0);

    log("[TimeNotifier%d] has died...", myTid());
}

void delayed_one_way_courier() {
    struct {
        tdelay_header head;
        char          message [244];
    } delay_req;

    int tid;
    int size = Receive(&tid, (char*)&delay_req, sizeof(delay_req));
    assert(size >= (int)sizeof(delay_req.head),
           "Recived an invalid setup message %d / %d",
           size, sizeof(delay_req.head));

    int result = Reply(tid, NULL, 0);
    assert(result == 0, "Failed reposnding to setup task (%d)", result);

    switch (delay_req.head.type) {
    case DELAY_RELATIVE:
        Delay(delay_req.head.ticks);
        break;
    case DELAY_ABSOLUTE:
        DelayUntil(delay_req.head.ticks);
        break;
    }

    const int send_size = size - sizeof(delay_req);
    result = Send(delay_req.head.receiver,
                  delay_req.message, send_size,
                  delay_req.message, sizeof(delay_req.message));
    assert(result >= 0, "failed sending to receiver %d", result);


    log("[DelayedCourrier%d] has died", myTid());
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
        assert(result >= 0, "Error sending response to %d (%d)",
               tid, result);
    } while (result > 0);

    log("[Courier%d] has died...", myTid());
}
