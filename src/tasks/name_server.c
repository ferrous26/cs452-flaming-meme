#include <std.h>
#include <debug.h>
#include <vt100.h>
#include <scheduler.h>

#include <tasks/name_server.h>
#include <syscall.h>

/*
 * All of the internal structs required by the name server implmentation
 */
typedef enum {
    REGISTER,
    LOOKUP,
    REVERSE
} ns_req_type;

typedef union {
    char text[NAME_MAX_SIZE];
    int  overlay[NAME_OVERLAY_SIZE];
} ns_payload;

typedef struct {
    ns_req_type type;
    ns_payload  payload;
} ns_req;

typedef struct {
    union {
        char text[NAME_MAX][NAME_MAX_SIZE];
    	int  overlay[NAME_MAX][NAME_OVERLAY_SIZE];
    }   lookup;
    int lookup_insert;
    int tasks[NAME_MAX];
} ns_context;

/*
 * Static data
 */
static int name_server_tid;
#ifdef ASSERT
static ns_context* _context;
#endif

/*
 * look up functions
 */
static int __attribute__((const))
find_loc(int directory[][NAME_OVERLAY_SIZE],
         const int value[NAME_OVERLAY_SIZE],
         const int stored) {

    for (int i = 0; i < stored; i++) {
	for (int j = 0;;j++) {
	    if(j == NAME_OVERLAY_SIZE) return i;
            if(directory[i][j] != value[j]) break;
	}
    }
    return -1;
}

static int __attribute__((const))
find_name(int dir[NAME_MAX], const int tid, const int stored) {

    for (int i = 0; i < stored; i++) {
        if (dir[i] == tid) { return i; }
    }
    return -1;
}

static inline void register_tid(ns_context* ctxt, int tid, ns_payload* data) {
    int reply = 0;
    int loc   = find_loc(ctxt->lookup.overlay,
                         data->overlay,
                         ctxt->lookup_insert);

    if(loc < 0) {
        if(ctxt->lookup_insert < NAME_MAX) {
	    ctxt->lookup.overlay[ctxt->lookup_insert][0] = data->overlay[0];
	    ctxt->lookup.overlay[ctxt->lookup_insert][1] = data->overlay[1];
	    ctxt->tasks[ctxt->lookup_insert++] = tid;
	} else {
            reply = -69;
	}
    } else {
        ctxt->tasks[loc] = tid;
    }

    Reply(tid, (char*)&reply, sizeof(reply));
}

static inline int lookup_tid(ns_context* ctxt, ns_payload* data) {
    int loc = find_loc(ctxt->lookup.overlay,
                       data->overlay,
                       ctxt->lookup_insert);
    return loc < 0 ? -69 : ctxt->tasks[loc];
}

static inline char* lookup_name(ns_context* ctxt, ns_payload* data) {
    int loc = find_name(ctxt->tasks, data->overlay[0], ctxt->lookup_insert);
    return loc < 0 ? NULL : ctxt->lookup.text[loc];
}

void name_server() {
    int        tid;
    ns_req     buffer;
    ns_context context;

    #ifdef ASSERT
    _context = &context;
    #endif

    name_server_tid = myTid();

    memset((void*)&buffer, 0, sizeof(buffer));
    sprintf_string(buffer.payload.text, (char*)NAME_SERVER_NAME);
    register_tid(&context, name_server_tid, &buffer.payload);

    FOREVER {
        Receive(&tid, (char*)&buffer, sizeof(ns_req));
	assert(buffer.type == REGISTER
             ||buffer.type == LOOKUP
             ||buffer.type == REVERSE,
	       "NS: Invalid message type from %u (%d)", buffer.type, tid);

        switch(buffer.type) {
        case REGISTER:
            register_tid(&context, tid, &buffer.payload);
            break;
        case LOOKUP: {
            int result = lookup_tid(&context, &buffer.payload);
            Reply(tid, (char*)&result, sizeof(result));
            break;
        }
        case REVERSE: {
            char* result = lookup_name(&context, &buffer.payload);
            Reply(tid, result, result == NULL ? 0 : NAME_MAX_SIZE*sizeof(char));
            break;
        }
	}
    }
}

int WhoIs(char* name) {
    ns_req req;
    req.type = LOOKUP;
    memset((void*)&req.payload, 0, sizeof(ns_payload));

    for(uint i = 0; name[i] != '\0'; i++) {
        if (i == NAME_MAX_SIZE) return -1;
	req.payload.text[i] = name[i];
    }

    int status;
    int result = Send(name_server_tid,
		      (char*)&req,    sizeof(req),
		      (char*)&status, sizeof(int));

    if (result > OK) return status;

    // maybe clock server died, so we can try again
    assert(result == INVALID_TASK || result == INCOMPLETE,
           "Name Server Has Died!");
    // else, error out
    return result;
}

int WhoTid(int tid, char* name) {
    ns_req req;
    req.type = LOOKUP;
    memset((void*)&req.payload, 0, sizeof(ns_payload));
    req.payload.overlay[0] = tid;

    int result = Send(name_server_tid,
                      (char*)&req, sizeof(req),
                      name, NAME_MAX_SIZE);

    if (result == 0) {
        memcpy("NO_ENTY", name, sizeof(name));
        return -63;
    }

    assert(result < 0 || result == NAME_MAX_SIZE,
           "NameServer returned weird value %d", result);
    assert(result == INVALID_TASK || result == INCOMPLETE,
           "Name Server Has Died!");
    // else, error out
    return result;
}


int RegisterAs(char* name) {
    ns_req req;
    req.type = REGISTER;
    memset((void*)&req.payload, 0, sizeof(ns_payload));

    for (uint i = 0; name[i] != '\0'; i++) {
        if (i == NAME_MAX_SIZE) return -1;
	req.payload.text[i] = name[i];
    }

    int status;
    int result = Send(name_server_tid,
		      (char*)&req,    sizeof(req),
		      (char*)&status, sizeof(int));

    if (result > OK) return (status < 0 ? status :  0);

    // maybe clock server died, so we can try again
    if (result == INVALID_TASK || result == INCOMPLETE)
	if (Create(TASK_PRIORITY_HIGH - 1, name_server) > 0)
	    return RegisterAs(name);

    // else, error out
    return result;
}

#ifdef ASSERT
char* kWhoTid(int tid) {
    ns_req req;
    req.payload.overlay[0] = tid;

    return lookup_name(_context, &req.payload);
}
#endif
