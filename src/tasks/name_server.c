#include <std.h>
#include <syscall.h>
#include <debug.h>

#include <tasks/name_server.h>

int name_server_tid;

static int __attribute__((const)) find_loc(uint32 directory[][NAME_OVERLAY_SIZE],
                                           const uint32 value[NAME_OVERLAY_SIZE],
                                           const int stored) {
    for (int i = 0; i < stored; i++) {
	for (int j = 0;;j++) {
	    if(j == NAME_OVERLAY_SIZE) return i;
            if(directory[i][j] != value[j]) break;
	}
    }
    return -1;
}

void name_server() {
    int32 tasks[NAME_MAX];
    union {
        char   name[NAME_MAX][NAME_MAX_SIZE];
    	uint32 overlay[NAME_MAX][NAME_OVERLAY_SIZE];
    } lookup;

    int reply;
    int32 tid;
    ns_req buffer;
    int insert = 0;

    name_server_tid = myTid();
    vt_log("Name Server started at %d", name_server_tid);

    FOREVER {
        Receive(&tid, (char*)&buffer, sizeof(ns_req));
        int loc = find_loc(lookup.overlay, buffer.payload.overlay, insert);

	assert(buffer.type == REGISTER || buffer.type == LOOKUP,
	       "NS: Invalid message type from %u (%d)", buffer.type, tid);

        switch(buffer.type) {
        case REGISTER:
            reply = 0;
	    if(loc < 0) {
		if(insert < NAME_MAX) {
		    lookup.overlay[insert][0] = buffer.payload.overlay[0];
		    lookup.overlay[insert][1] = buffer.payload.overlay[1];
		    tasks[insert++] = tid;
	        } else {
                    reply = -69;
	        }
	    } else {
                tasks[loc] = tid;
	    }
            break;
        case LOOKUP:
            reply = loc < 0 ? -69 : tasks[loc];
	    break;
	}
        Reply(tid, (char*)&reply, sizeof(reply));
    }
}
