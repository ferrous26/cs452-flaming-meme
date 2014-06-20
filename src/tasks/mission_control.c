
#include <std.h>
#include <debug.h>
#include <train.h>
#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>

#include <tasks/mission_control.h>

#define RECENT_SENSOR_SIZE 32

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
    mc_type    type;
    mc_payload payload;
} mc_req;

#define TRAIN_ROW           6
#define TRAIN_COL           1
#define TRAIN_SPEED_COL     (TRAIN_COLUMN + 7)
#define TRAIN_DIRECTION_COL (TRAIN_SPEED_COLUMN + 7)
#define TRAIN_LIGHT_COL     (TRAIN_DIRECTION_COLUMN + 6)
#define TRAIN_HORN_COL      (TRAIN_LIGHT_COLUMN + 2)
#define TRAIN_SFX_COL       (TRAIN_HORN_COLUMN + 2)

#define TURNOUT_ROW TRAIN_ROW
#define TURNOUT_COL 36

#define SENSOR_ROW (TRAIN_ROW - 1)
#define SENSOR_COL 64

static void train_ui() {
    char buffer[1024];
    char* ptr = buffer;

    ptr = vt_goto(ptr, TRAIN_ROW - 2, TRAIN_COL);
    ptr = sprintf_string(ptr,
"Train  Speed  Dir.  Special     Turnouts/Gates/Switches     __Sensor__\n"
"---------------------------    +-----------------------+    |        | Newest\n"
" 43                            | 1   | 2   | 3   | 4   |    |        |\n"
" 45                            | 5   | 6   | 7   | 8   |    |        |\n"
" 47                            | 9   |10   |11   |12   |    |        |\n"
" 48                            |13   |14   |15   |16   |    |        |\n"
" 49                            |17   |18   |-----------|    |        |\n"
" 50                            |19    20   |21    22   |    |        |\n"
" 51                            +-----------------------+    |        | Oldest");

    int result = Puts(buffer, ptr - buffer);
    if (result != 0) ABORT("Failed to init train state UI");
}

int __attribute__((const)) train_to_pos(const int train) {
    if (train >= 43 && train <=51) {
        return train - 43;
    } 
    return -1;
}

int __attribute__((const)) pos_to_train(const int pos) {
    if (pos >= 0 && pos <=8) {
        return pos + 43;
    }
    return -1;
}

int __attribute__((const)) turnout_to_pos(const int turnout) {
    if (turnout <  1)   return -1;
    if (turnout <= 18)  return turnout - 1;
    if (turnout <  153) return -1;
    if (turnout <= 156) return turnout - (153-18);
    return -1;
}

int __attribute__((const)) pos_to_turnout(const int pos) {
    if (pos < 0)  return -1;
    if (pos < 18) return pos + 1;
    if (pos < 22) return pos + (153-18);
    return -1;
}

static void __attribute__ ((noreturn)) sensor_poll() {
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
	Delay(10);

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

typedef struct {
    int         insert;
    sensor_name recent_sensors[9];
} mc_context;

inline static void mc_update_sensors(mc_context* const ctxt,
                                     const sensor_name* const sensor) {

    memcpy(ctxt->recent_sensors + ctxt->insert, sensor, sizeof(sensor));
    ctxt->insert = ++ctxt->insert & (RECENT_SENSOR_SIZE-1);
    memset(ctxt->recent_sensors + ctxt->insert, 0, sizeof(ctxt->insert)); 

    log("SENSOR:%c%d in %d", sensor->bank, sensor->num, ctxt->insert);
}

void mission_control() {
    int tid = RegisterAs((char*)MISSION_CONTROL_NAME);
    assert(tid == 0, "Mission Control has failed to register (%d)", tid);

    for (int i = 0;; i++) {
        if (pos_to_train(i) < 0) break;
        tid = put_train_cmd(pos_to_train(i), 0);
        assert(tid == 0, "failed to set train %d", tid);
    }

    for (int i = 0;; i++) {
        if (pos_to_turnout(i) < 0) break;
        put_train_turnout(TURNOUT_CURVED, pos_to_turnout(i));
        assert(tid == 0, "failed to set turnout %d", tid);
    }
    
    tid = Create(15, sensor_poll);
    assert(tid >  0, "Mission Control failed creating sensor poll (%d)", tid);

    tid = Create(TASK_PRIORITY_MEDIUM_LO, train_ui);
    assert(tid > 0, "Mission Control failed creating train UI (%d)", tid);
    
    mc_req     req;
    mc_context context;

    FOREVER {
        Receive(&tid, (char*)&req, sizeof(req));

        switch (req.type) {
        case SENSOR_UPDATE:
            Reply(tid, NULL, 0);
            mc_update_sensors(&context, &req.payload.sensor);
            break;
        }
    }
}
