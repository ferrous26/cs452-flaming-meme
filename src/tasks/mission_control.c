
#include <std.h>
#include <debug.h>
#include <train.h>
#include <vt100.h>
#include <syscall.h>

#include <tasks/mission_control.h>

typedef struct {
    short bank;
    short num;
} sensor_name;

typedef enum {
    SENSOR_UPDATE
} mc_type;

typedef union {
    sensor_name sensor;
} mc_payload;

typedef struct {
    int count;
} mc_context;

typedef struct {
    mc_type    type;
    mc_payload payload;
} mc_req;

static void __attribute__((noreturn)) sensor_poll() {
    RegisterAs((char*)SENSOR_POLL_NAME);
    Putc(TRAIN, SENSOR_RESET); 
    Putc(TRAIN, SENSOR_POLL);

    int       sensor_state[10];
    const int ptid = myParentTid();
    mc_req    req = {
        .type = SENSOR_UPDATE   
    };

    for (int i = 0; i < 10; i++) {
        sensor_state[i] = Getc(TRAIN);
    }

    FOREVER {
        Putc(TRAIN, SENSOR_POLL);
        for (int bank = 0; bank < 10; bank++) {
            int c = Getc(TRAIN);
            assert(c >= 0, "sensor_poll got bad return (%d)", c);

            for (int mask = 0x80, i = 1; mask > 0; mask = mask >> 1, i++) {
                if((c & mask) > (sensor_state[bank] & mask)) {
                    req.payload.sensor.bank = 'A' + (bank>>1);
                    req.payload.sensor.num  =  i + 8 * (bank& 1);
                    Send(ptid, (char*)&req, sizeof(req), NULL, 0);
                }
            }

            sensor_state[bank] = c;
        }
    }
}

inline static void mc_update_sensors(mc_context* const ctxt,
                                     const sensor_name* const sensor) {
    ctxt->count++;
    log("SENSOR:%c%d", sensor->bank, sensor->num);
}

void mission_control() {
    mc_req     req;
    mc_context context;
    int        tid = RegisterAs((char*)SENSOR_POLL_NAME);
    assert(tid == 0, "Mission Control has failed to register (%d)", tid);
    
    tid = Create(15, sensor_poll);
    assert(tid > 0, "Mission Control failed creating sensor poll (%d)", tid);

    FOREVER {
        Receive(&tid, (char*)&req, sizeof(req));

        switch (req.type) {
        case SENSOR_UPDATE:
            mc_update_sensors(&context, &req.payload.sensor);
            Reply(tid, NULL, 0);
            break;
        }
    }
}

