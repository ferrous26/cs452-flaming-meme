#include <tasks/train_station.h>
#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/mission_control.h>
#include <train.h>
#include <std.h>
#include <debug.h>

#define REQUEST_BUFFER_SIZE 16

typedef struct {
    train_req* head;
    train_req* tail;
    train_req* end;
    uint       count;
    train_req  buffer[REQUEST_BUFFER_SIZE];
} req_buffer;

typedef struct {
    int  tid;
    int  tnum;
    bool is_blocked;
    req_buffer requests;
} train_state;


static int train_station_tid;


static inline void rbuf_init(req_buffer* const cb) {
    cb->head  = cb->tail = cb->buffer;
    cb->end   = cb->buffer + REQUEST_BUFFER_SIZE; // first address past the end
    cb->count = 0;
    memset(cb->buffer, 0, sizeof(cb->buffer));
}

static inline __attribute__ ((const))
uint rbuf_count(const req_buffer* const cb) {
    return cb->count;
}

static inline void rbuf_produce(req_buffer* const cb,
				const train_req* const req) {

    memcpy(cb->head++, req, sizeof(train_req));

    cb->count++;
    if (cb->head == cb->end) {
	cb->head = cb->buffer;
    }

    assert(cb->count < REQUEST_BUFFER_SIZE, "Overfilled buffer!")
}

static inline train_req* rbuf_consume(req_buffer* const cb) {
    assert(cb->count, "Trying to consume from empty puts queue");

    train_req* const req = cb->tail++;

    cb->count--;
    if (cb->tail == cb->end)
	cb->tail = cb->buffer;

    return req;
}


static int train_station_whoami() {
    train_req req = {
	.type = TRAIN_WHOAMI
    };

    int train_num = 0;
    int    result = Send(train_station_tid,
			 (char*)&req, sizeof(train_req_type),
			 (char*)&train_num, sizeof(train_num));

    if (result == sizeof(int)) return train_num;
    ABORT("Failed to perform train WHOAMI (%d)", result);
}

static inline void train_station_request(train_req* incoming) {
    train_req outgoing = {
	.type = TRAIN_REQUEST
    };

    int result = Send(train_station_tid,
		      (char*)&outgoing, sizeof(train_req_type),
		      (char*)incoming,  sizeof(train_req));

    if (result < 0) ABORT("Train station died (%d)", result);
}

static bool is_valid_train_number(int train) {
    if (train < 43  ||
	train > 51  ||
	train == 44 ||
	train == 46) return false;
    return true;
}

void train_reverse(int train) {

    if (!is_valid_train_number(train)) {
        log("Invalid train number %d", train);
        return;
    }

    train_req req = {
	.type  = TRAIN_REVERSE_DIRECTION,
	.train = train
    };

    int result = Send(train_station_tid,
		      (char*)&req, sizeof(req),
		      NULL, 0);
    if (result < 0)
	log("Failed to send reverse command for train %d (%d)", train, result);
}

void train_set_speed(int train, int speed) {

    if (!is_valid_train_number(train)) {
        log("Invalid train number %d", train);
        return;
    }

    if (speed < 0 || speed >= TRAIN_REVERSE) {
        log("Cannot set train number %d to speed %d", train, speed);
        return;
    }

    train_req req = {
	.type  = TRAIN_CHANGE_SPEED,
	.train = train,
	.arg   = speed
    };

    int result = Send(train_station_tid,
		      (char*)&req, sizeof(req),
		      NULL, 0);

    if (result < 0)
	log("Failed to send speed change command for train %d to %d (%d)",
	    train, speed, result);
}

void train_toggle_light(int train) {

    if (!is_valid_train_number(train)) {
        log("Invalid train number %d", train);
        return;
    }

    train_req req = {
	.type  = TRAIN_TOGGLE_LIGHT,
	.train = train
    };

    int result = Send(train_station_tid,
		      (char*)&req, sizeof(req),
		      NULL, 0);

    if (result < 0)
	log("Failed to send light toggle command to %d (%d)", train, result);
}

void train_sound_horn(int train) {

    if (train != 51) {
	log("Only train 51 has a working horn");
	return;
    }

    train_req req = {
	.type  = TRAIN_HORN_SOUND,
	.train = train
    };

    int result = Send(train_station_tid,
		      (char*)&req, sizeof(req),
		      NULL, 0);

    if (result < 0)
	log("Failed to send horn sounding command (%d)", result);
}

static void train() {

    int train_num = train_station_whoami();

    char name[8];
    sprintf(name, "TRAIN%d", train_num);
    name[7] = '\0';

    int result = RegisterAs(name);
    if (result != 0)
	ABORT("Failed to register train %d (%d)", train_num, result);

    result = update_train_speed(train_num, 0);
    if (result < 0) log("Failed to stop train %d", result);

    int speed = 0;
    train_req req;

    FOREVER {
	train_station_request(&req);
	switch (req.type) {
	case TRAIN_WHOAMI:
	case TRAIN_REQUEST:
	    ABORT("Illegal train request (%d)", req.type);
	    break;
	case TRAIN_CHANGE_SPEED:
	    speed = req.arg;
	    update_train_speed(train_num, speed);
	    break;
	case TRAIN_REVERSE_DIRECTION:
	    update_train_speed(train_num, 0);
	    Delay(TRAIN_REVERSE_DELAY_FACTOR * speed);
	    update_train_speed(train_num, TRAIN_REVERSE);
	    update_train_speed(train_num, speed);
	    break;
	case TRAIN_TOGGLE_LIGHT:
	    toggle_train_light(train_num);
	    break;
	case TRAIN_HORN_SOUND:
	    log("Train horn not implemented");
	    break;
	}
    }
}

static void _train_station_init(train_state* state) {

    int result = RegisterAs(TRAIN_STATION_NAME);
    if (result != 0) ABORT("Failed to register train station (%d)", result);

    state[0].tnum = 43;
    state[1].tnum = 45;
    state[2].tnum = 47;
    state[3].tnum = 48;
    state[4].tnum = 49;
    state[5].tnum = 50;
    state[6].tnum = 51;

    for (uint i = 0; i < TRAIN_COUNT; i++) {
	int tid = Create(TASK_PRIORITY_MEDIUM_LOW, train);
	if (tid < 0) ABORT("Failed to create a train (%d)", tid);

	state[i].tid        = tid;
	state[i].is_blocked = false;
	rbuf_init(&state[i].requests);
    }
}

static inline train_state* __attribute__ ((const))
    trains_find_with_tnum(train_state* const trains, const int train_num) {

    for (int i = 0; i < TRAIN_COUNT; i++)
	if (trains[i].tnum == train_num)
	    return &trains[i];

    return NULL;
}

static inline train_state* __attribute__ ((const))
    trains_find_with_tid(train_state* const trains, const int tid) {

    for (int i = 0; i < TRAIN_COUNT; i++)
	if (trains[i].tid == tid)
	    return &trains[i];

    return NULL;
}

void train_station() {
    train_station_tid = myTid();

    train_state trains[TRAIN_COUNT];
    _train_station_init(trains);

    int tid;
    train_req req;

    FOREVER {
	int result = Receive(&tid, (char*)&req, sizeof(train_req));
	if (result < WORD_SIZE) ABORT("Bad message to train");

	assert(req.type <= TRAIN_HORN_SOUND,
	       "Bad train station request (%d) from %d", req.type, tid);

	switch (req.type) {
	case TRAIN_WHOAMI: {
	    train_state* const t = trains_find_with_tid(trains, tid);
	    if (t) {
		result = Reply(tid, (char*)&t->tnum, sizeof(int));
		if (result != 0) ABORT("Train died (%d)", result);
	    }
	    else {
		ABORT("Non-train asked WHOAMI %d", tid);
	    }
	    break;
	}

	case TRAIN_REQUEST: {
	    train_state* const t = trains_find_with_tid(trains, tid);
	    if (t) {
		if (rbuf_count(&t->requests)) {
		    result = Reply(t->tid,
				   (char*)rbuf_consume(&t->requests),
				   sizeof(req));
		    if (result < 0)
			log("Failed to forward job to train %d (%d)",
			    t->tid, result);
		}
		else {
		    t->is_blocked = true;
		}
	    }
	    else {
		ABORT("Non-train asked REQUEST %d", tid);
	    }
	    break;
	}

	    // in all these cases, we just want to queue the job
	case TRAIN_CHANGE_SPEED:
	case TRAIN_REVERSE_DIRECTION:
	case TRAIN_TOGGLE_LIGHT:
	case TRAIN_HORN_SOUND: {
	    train_state* const t = trains_find_with_tnum(trains, req.train);
	    if (t) {
		if (t->is_blocked) {
		    t->is_blocked = false;

		    result = Reply(t->tid, (char*)&req, sizeof(req));
		    if (result < 0)
			log("Failed to forward job to train %d (%d)",
			    t->tid, result);
		}
		else {
		    rbuf_produce(&t->requests, &req);
		}
		result = Reply(tid, NULL, 0);
	    }
	    else {
		result = Reply(tid, (char*)&tid, sizeof(tid));
	    }

	    if (result < 0)
		log("Failed to reply to train request %d", result);
	    break;
	}
	}
    }
}
